#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/ocean/ocean.hpp"

#include <entt/entity/registry.hpp>
#include <sung/basic/cvar.hpp>

#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/ocean/common.hpp"


namespace {

    sung::AutoCVarFlt cv_displace_scale_x{ "ocean:displace_scale_x", "", 1 };
    sung::AutoCVarFlt cv_displace_scale_y{ "ocean:displace_scale_y", "", 1 };

}  // namespace


// Ocean Finalize
namespace {

    struct U_OceanFinalizePushConst {
        glm::vec2 hor_displace_scale_;
        float dt_;
        float turb_time_factor_;
        int32_t N_;
    };


    class RpStatesOceanFinalize : public mirinae::IRpStates {

    public:
        RpStatesOceanFinalize(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            mirinae::CommandPool cmd_pool;
            cmd_pool.init(device);

            // Reference images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = fdata_[i];

                for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                    fd.hkt_1_[j] = rp_res.get_img_reader(
                        fmt::format("ocean_tilde_hkt:hkt_1_c{}_f{}", j, i),
                        name()
                    );
                    MIRINAE_ASSERT(nullptr != fd.hkt_1_[j]);
                    fd.hkt_2_[j] = rp_res.get_img_reader(
                        fmt::format("ocean_tilde_hkt:hkt_2_c{}_f{}", j, i),
                        name()
                    );
                    MIRINAE_ASSERT(nullptr != fd.hkt_2_[j]);
                }
            }

            // Storage images
            {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_dimensions(mirinae::OCEAN_TEX_DIM)
                    .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = fdata_[i];

                    for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                        const auto i_name = fmt::format(
                            "displacement_c{}_f{}", j, i
                        );
                        auto img = rp_res.new_img(i_name, name());
                        MIRINAE_ASSERT(nullptr != img);

                        cinfo.set_format(VK_FORMAT_R32G32B32A32_SFLOAT);
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());

                        builder.format(cinfo.format());
                        img->view_.reset(builder, device);

                        fd.disp_[j] = std::move(img);
                    }

                    for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                        const auto i_name = fmt::format(
                            "derivatives_c{}_f{}", j, i
                        );
                        auto img = rp_res.new_img(i_name, name());
                        MIRINAE_ASSERT(nullptr != img);

                        cinfo.set_format(VK_FORMAT_R32G32B32A32_SFLOAT);
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());

                        builder.format(cinfo.format());
                        img->view_.reset(builder, device);

                        fd.deri_[j] = std::move(img);
                    }
                }

                for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                    const auto i_name = fmt::format("turbulence_c{}", j);
                    auto img = rp_res.new_img(i_name, name());
                    MIRINAE_ASSERT(nullptr != img);

                    cinfo.set_format(VK_FORMAT_R32G32B32A32_SFLOAT);
                    img->img_.init(cinfo.get(), device.mem_alloc());
                    builder.image(img->img_.image());

                    builder.format(cinfo.format());
                    img->view_.reset(builder, device);

                    turb_[j] = std::move(img);
                }
            }

            // Image transitions
            {
                mirinae::ImageMemoryBarrier barrier;
                barrier.set_src_access(0)
                    .set_dst_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .new_layout(VK_IMAGE_LAYOUT_GENERAL)
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .layer_count(1)
                    .mip_count(1);

                mirinae::CommandPool cmd_pool;
                cmd_pool.init(device);
                auto cmdbuf = cmd_pool.begin_single_time(device);
                for (auto fd : fdata_) {
                    for (size_t i = 0; i < mirinae::CASCADE_COUNT; i++) {
                        barrier.image(fd.disp_[i]->img_.image());
                        barrier.record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
                        );

                        barrier.image(fd.deri_[i]->img_.image());
                        barrier.record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
                        );
                    }
                }
                for (size_t i = 0; i < mirinae::CASCADE_COUNT; i++) {
                    barrier.image(turb_[i]->img_.image());
                    barrier.record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT
                    );
                }
                cmd_pool.end_single_time(cmdbuf, device);
                cmd_pool.destroy(device.logi_device());
            }

            // Desc layouts
            {
                mirinae::DescLayoutBuilder builder{ name() + ":main" };
                builder
                    .new_binding()  // displacement
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // derivatives
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // turbulence
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // hkt dxdy
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // hkt dz
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // hkt ddxddz
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding();
                desclayouts.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                auto& layout = desclayouts.get(name() + ":main");

                desc_pool_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    layout.size_info(),
                    device.logi_device()
                );

                auto sets = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = fdata_[i];
                    fd.desc_set_ = sets[i];

                    writer.add_storage_img_info(fd.disp_[0]->view_.get())
                        .add_storage_img_info(fd.disp_[1]->view_.get())
                        .add_storage_img_info(fd.disp_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 0);
                    writer.add_storage_img_info(fd.deri_[0]->view_.get())
                        .add_storage_img_info(fd.deri_[1]->view_.get())
                        .add_storage_img_info(fd.deri_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 1);
                    writer.add_storage_img_info(turb_[0]->view_.get())
                        .add_storage_img_info(turb_[1]->view_.get())
                        .add_storage_img_info(turb_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 2);
                    writer.add_storage_img_info(fd.hkt_1_[0]->view_.get())
                        .add_storage_img_info(fd.hkt_1_[1]->view_.get())
                        .add_storage_img_info(fd.hkt_1_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 3);
                    writer.add_storage_img_info(fd.hkt_2_[0]->view_.get())
                        .add_storage_img_info(fd.hkt_2_[1]->view_.get())
                        .add_storage_img_info(fd.hkt_2_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 4);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline
            {
                mirinae::PipelineLayoutBuilder{}
                    .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                    .pc<U_OceanFinalizePushConst>()
                    .desc(desclayouts.get(name() + ":main").layout())
                    .build(pipe_layout_, device);

                pipeline_ = mirinae::create_compute_pipeline(
                    ":asset/spv/ocean_finalize_comp.spv", pipe_layout_, device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanFinalize() override {
            for (auto& fdata : fdata_) {
                for (size_t i = 0; i < mirinae::CASCADE_COUNT; i++) {
                    rp_res_.free_img(fdata.hkt_1_[i]->id(), this->name());
                    rp_res_.free_img(fdata.hkt_2_[i]->id(), this->name());
                    rp_res_.free_img(fdata.disp_[i]->id(), this->name());
                    rp_res_.free_img(fdata.deri_[i]->id(), this->name());
                }

                fdata.desc_set_ = VK_NULL_HANDLE;
            }

            for (size_t i = 0; i < mirinae::CASCADE_COUNT; i++)
                rp_res_.free_img(turb_[i]->id(), this->name());

            desc_pool_.destroy(device_.logi_device());
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "ocean_finalize";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            GET_OCEAN_ENTT(ctxt);
            auto& fd = fdata_[ctxt.f_index_.get()];

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_
            );

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
                .layout(pipe_layout_)
                .add(fd.desc_set_)
                .record(cmdbuf);

            VkMemoryBarrier mem_bar = {};
            mem_bar.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            mem_bar.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            mem_bar.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

            vkCmdPipelineBarrier(
                cmdbuf,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1,
                &mem_bar,
                0,
                nullptr,
                0,
                nullptr
            );

            U_OceanFinalizePushConst pc;
            pc.hor_displace_scale_.x = cv_displace_scale_x.get();
            pc.hor_displace_scale_.y = cv_displace_scale_y.get();
            pc.dt_ = ctxt.cosmos_->scene().clock().dt();
            pc.turb_time_factor_ = ocean_entt.trub_time_factor_;
            pc.N_ = ::mirinae::OCEAN_TEX_DIM;

            mirinae::PushConstInfo{}
                .layout(pipe_layout_)
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

            vkCmdDispatch(
                cmdbuf,
                mirinae::OCEAN_TEX_DIM / 16,
                mirinae::OCEAN_TEX_DIM / 16,
                mirinae::CASCADE_COUNT
            );
        }

    private:
        struct FrameData {
            std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hkt_1_;
            std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hkt_2_;
            std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> disp_;
            std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> deri_;
            VkDescriptorSet desc_set_;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> fdata_;
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> turb_;
        mirinae::DescPool desc_pool_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
    };

}  // namespace


namespace mirinae::rp::ocean {

    URpStates create_rp_states_ocean_finalize(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesOceanFinalize>(
            rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp::ocean
