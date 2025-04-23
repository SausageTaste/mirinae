#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/ocean/ocean.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/ocean/common.hpp"


// Ocean Tilde H
namespace {

    // Range is [0, 1]
    double rand_uniform() { return (double)rand() / RAND_MAX; }


    struct U_OceanTildeHPushConst {
        glm::vec2 wind_dir_;
        float wind_speed_;
        float amplitude_;
        float fetch_;
        float depth_;
        float swell_;
        float spread_blend_;
        float cutoff_high_;
        float cutoff_low_;
        float L_;
        int32_t N_;
        int32_t cascade_;
    };


    class RpStatesOceanTildeH : public mirinae::IRpStates {

    public:
        RpStatesOceanTildeH(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            mirinae::CommandPool cmd_pool;
            cmd_pool.init(device);

            // Noise textures
            {
                std::vector<uint8_t> noise_data(
                    mirinae::OCEAN_TEX_DIM * mirinae::OCEAN_TEX_DIM * 4
                );
                for (size_t i = 0; i < noise_data.size(); i++)
                    noise_data[i] = static_cast<uint8_t>(rand_uniform() * 255);

                mirinae::ImageCreateInfo img_info;
                img_info.set_dimensions(mirinae::OCEAN_TEX_DIM)
                    .set_format(VK_FORMAT_R8G8B8A8_UNORM)
                    .deduce_mip_levels()
                    .add_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                    .add_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                    .add_usage_sampled();

                mirinae::ImageViewBuilder iv_builder;
                iv_builder.format(img_info.format())
                    .mip_levels(img_info.mip_levels());

                mirinae::Buffer staging_buffer;
                staging_buffer.init_staging(
                    noise_data.size(), device_.mem_alloc()
                );
                staging_buffer.set_data(
                    noise_data.data(), noise_data.size(), device_.mem_alloc()
                );

                auto img = rp_res.new_img("ocean_noise", this->name());
                MIRINAE_ASSERT(nullptr != img);
                noise_textures_ = img;
                img->img_.init(img_info.get(), device_.mem_alloc());

                auto cmdbuf = cmd_pool.begin_single_time(device_);
                mirinae::record_img_buf_copy_mip(
                    cmdbuf,
                    mirinae::OCEAN_TEX_DIM,
                    mirinae::OCEAN_TEX_DIM,
                    img_info.mip_levels(),
                    img->img_.image(),
                    staging_buffer.buffer()
                );
                cmd_pool.end_single_time(cmdbuf, device_);
                staging_buffer.destroy(device_.mem_alloc());

                iv_builder.image(img->img_.image());
                img->view_.reset(iv_builder, device_);
            }

            // Storage images
            {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_dimensions(mirinae::OCEAN_TEX_DIM)
                    .set_format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.format(cinfo.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                        const auto img_name = fmt::format(
                            "height_map_c{}_f#{}", j, i
                        );
                        auto img = rp_res.new_img(img_name, this->name());
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());
                        img->view_.reset(builder, device);
                        frame_data_[i].hk_[j] = img;
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
                    for (auto& hk : fd.hk_) {
                        barrier.image(hk->img_.image());
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

            // Desc layouts
            {
                mirinae::DescLayoutBuilder builder{ name() + ":main" };
                builder
                    .new_binding()  // Cascade 0, 1, 2
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .add_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .set_count(3)
                    .finish_binding()
                    .add_img(VK_SHADER_STAGE_COMPUTE_BIT, 1);
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

                    writer.add_storage_img_info(fd.hk_[0]->view_.get())
                        .add_storage_img_info(fd.hk_[1]->view_.get())
                        .add_storage_img_info(fd.hk_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 0);
                    writer.add_img_info()
                        .set_img_view(noise_textures_->view_.get())
                        .set_sampler(device.samplers().get_nearest())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 1);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline
            {
                mirinae::PipelineLayoutBuilder{}
                    .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                    .pc<U_OceanTildeHPushConst>()
                    .desc(desclayouts.get(name() + ":main").layout())
                    .build(pipe_layout_, device);

                pipeline_ = mirinae::create_compute_pipeline(
                    ":asset/spv/ocean_tilde_h_comp.spv", pipe_layout_, device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanTildeH() override {
            for (auto& fd : frame_data_) {
                for (auto& hk : fd.hk_) {
                    rp_res_.free_img(hk->id(), this->name());
                }
                fd.desc_set_ = VK_NULL_HANDLE;
            }

            rp_res_.free_img(noise_textures_->id(), this->name());
            desc_pool_.destroy(device_.logi_device());
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "ocean_tilde_h";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            GET_OCEAN_ENTT(ctxt);
            auto& fd = frame_data_.at(ctxt.f_index_.get());

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_
            );

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
                .layout(pipe_layout_)
                .add(fd.desc_set_)
                .record(cmdbuf);

            ::U_OceanTildeHPushConst pc;
            pc.wind_dir_ = ocean_entt.wind_dir_;
            pc.wind_speed_ = ocean_entt.wind_speed_;
            pc.fetch_ = ocean_entt.fetch_;
            pc.swell_ = ocean_entt.swell_;
            pc.depth_ = ocean_entt.depth_;
            pc.spread_blend_ = ocean_entt.spread_blend_;
            pc.N_ = mirinae::OCEAN_TEX_DIM;

            for (int i = 0; i < mirinae::CASCADE_COUNT; ++i) {
                auto& cascade = ocean_entt.cascades_[i];
                pc.amplitude_ = cascade.amplitude();
                pc.cutoff_high_ = cascade.cutoff_high_;
                pc.cutoff_low_ = cascade.cutoff_low_;
                pc.L_ = cascade.L_;
                pc.cascade_ = i;

                mirinae::PushConstInfo pc_info;
                pc_info.layout(pipe_layout_)
                    .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .record(cmdbuf, pc);

                vkCmdDispatch(
                    cmdbuf,
                    mirinae::OCEAN_TEX_DIM / 16,
                    mirinae::OCEAN_TEX_DIM / 16,
                    1
                );
            }
        }

    private:
        struct FrameData {
            std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hk_;
            VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;
        mirinae::HRpImage noise_textures_;
        mirinae::DescPool desc_pool_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        sung::MonotonicRealtimeTimer timer_;
    };

}  // namespace


namespace mirinae::rp::ocean {

    URpStates create_rp_states_ocean_tilde_h(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesOceanTildeH>(
            rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp::ocean
