#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/ocean/ocean.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/ocean/common.hpp"


// Ocean Tilde Hkt
namespace {

    struct U_OceanTildeHktPushConst {
        float time_;
        float repeat_time_;
        float depth_;
        int32_t L_[3];
        int32_t N_;
    };


    class RpStatesOceanTildeHkt : public mirinae::IRpStates {

    public:
        RpStatesOceanTildeHkt(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            mirinae::CommandPool cmd_pool;
            cmd_pool.init(device);

            // Storage images
            {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_dimensions(mirinae::OCEAN_TEX_DIM)
                    .set_format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.format(cinfo.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];

                    for (size_t j = 0; j < mirinae::CASCADE_COUNT; ++j) {
                        const auto img_name = fmt::format(
                            "hkt_1_c{}_f{}", j, i
                        );
                        auto img = rp_res.new_img(img_name, this->name());
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());
                        img->view_.reset(builder, device);
                        fd.hkt_1_[j] = img;
                    }

                    for (size_t j = 0; j < mirinae::CASCADE_COUNT; ++j) {
                        const auto img_name = fmt::format(
                            "hkt_2_c{}_f{}", j, i
                        );
                        auto img = rp_res.new_img(img_name, this->name());
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());
                        img->view_.reset(builder, device);
                        fd.hkt_2_[j] = img;
                    }
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
                for (auto& fd : frame_data_) {
                    for (size_t i = 0; i < mirinae::CASCADE_COUNT; ++i) {
                        barrier.image(fd.hkt_1_[i]->img_.image());
                        barrier.record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
                        );

                        barrier.image(fd.hkt_2_[i]->img_.image());
                        barrier.record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
                        );
                    }
                }
                cmd_pool.end_single_time(cmdbuf, device);
                cmd_pool.destroy(device.logi_device());
            }

            // Reference images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                for (size_t j = 0; j < mirinae::CASCADE_COUNT; ++j) {
                    const auto img_name = fmt::format(
                        "ocean_tilde_h:height_map_c{}_f#{}", j, i
                    );
                    auto img = rp_res.get_img_reader(img_name, this->name());
                    MIRINAE_ASSERT(nullptr != img);
                    frame_data_[i].hk_[j] = img;
                }
            }

            // Desc layouts
            {
                mirinae::DescLayoutBuilder builder{ name() + ":main" };
                builder
                    .new_binding()  // hkt_1
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // hkt_2
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // hk
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

                auto desc_sets = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_ = desc_sets[i];

                    writer.add_storage_img_info(fd.hkt_1_[0]->view_.get())
                        .add_storage_img_info(fd.hkt_1_[1]->view_.get())
                        .add_storage_img_info(fd.hkt_1_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 0);
                    writer.add_storage_img_info(fd.hkt_2_[0]->view_.get())
                        .add_storage_img_info(fd.hkt_2_[1]->view_.get())
                        .add_storage_img_info(fd.hkt_2_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 1);
                    writer.add_storage_img_info(fd.hk_[0]->view_.get())
                        .add_storage_img_info(fd.hk_[1]->view_.get())
                        .add_storage_img_info(fd.hk_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 2);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline
            {
                mirinae::PipelineLayoutBuilder{}
                    .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                    .pc<U_OceanTildeHktPushConst>()
                    .desc(desclayouts.get(name() + ":main").layout())
                    .build(pipe_layout_, device);

                pipeline_ = mirinae::create_compute_pipeline(
                    ":asset/spv/ocean_tilde_hkt_comp.spv", pipe_layout_, device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanTildeHkt() override {
            for (auto& fd : frame_data_) {
                for (size_t i = 0; i < mirinae::CASCADE_COUNT; ++i) {
                    rp_res_.free_img(fd.hk_[i]->id(), this->name());
                    rp_res_.free_img(fd.hkt_1_[i]->id(), this->name());
                    rp_res_.free_img(fd.hkt_2_[i]->id(), this->name());
                }
            }

            desc_pool_.destroy(device_.logi_device());
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            return RpStatesOceanTildeHkt::name_static();
        }

        void record(const mirinae::RpContext& ctxt) override {
            GET_OCEAN_ENTT(ctxt);
            auto& fd = frame_data_[ctxt.f_index_.get()];

            mirinae::ImageMemoryBarrier barrier;
            barrier.set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .set_src_access(VK_ACCESS_SHADER_WRITE_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_GENERAL)
                .new_layout(VK_IMAGE_LAYOUT_GENERAL)
                .set_signle_mip_layer();
            for (size_t i = 0; i < mirinae::CASCADE_COUNT; ++i) {
                barrier.image(fd.hk_[i]->img_.image());
                barrier.record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                );
            }

            barrier.old_lay(VK_IMAGE_LAYOUT_UNDEFINED)
                .set_src_acc(0)
                .set_dst_acc(VK_ACCESS_SHADER_WRITE_BIT);
            for (size_t i = 0; i < mirinae::CASCADE_COUNT; ++i) {
                barrier.image(fd.hkt_1_[i]->img_.image());
                barrier.record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                );
                barrier.image(fd.hkt_2_[i]->img_.image());
                barrier.record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                );
            }

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_
            );

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
                .layout(pipe_layout_)
                .add(fd.desc_set_)
                .record(cmdbuf);

            ::U_OceanTildeHktPushConst pc;
            pc.time_ = ocean_entt.time_;
            pc.repeat_time_ = ocean_entt.repeat_time_;
            pc.depth_ = ocean_entt.depth_;
            pc.L_[0] = ocean_entt.cascades_[0].L_;
            pc.L_[1] = ocean_entt.cascades_[1].L_;
            pc.L_[2] = ocean_entt.cascades_[2].L_;
            pc.N_ = mirinae::OCEAN_TEX_DIM;

            mirinae::PushConstInfo pc_info;
            pc_info.layout(pipe_layout_)
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

            vkCmdDispatch(
                cmdbuf,
                mirinae::OCEAN_TEX_DIM / 16,
                mirinae::OCEAN_TEX_DIM / 16,
                mirinae::CASCADE_COUNT
            );
        }

        static const std::string& name_static() {
            static const std::string name = "ocean_tilde_hkt";
            return name;
        }

    private:
        struct FrameData {
            std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hk_;
            std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hkt_1_;
            std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hkt_2_;
            VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;
        mirinae::DescPool desc_pool_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        sung::MonotonicRealtimeTimer timer_;
    };

}  // namespace


namespace mirinae::rp::ocean {

    URpStates create_rp_states_ocean_tilde_hkt(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesOceanTildeHkt>(
            rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp::ocean
