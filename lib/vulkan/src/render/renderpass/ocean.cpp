#include "mirinae/render/renderpass/ocean.hpp"

#include <sung/basic/time.hpp>

#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/renderpass/builder.hpp"

#define GET_OCEAN_ENTT(ctxt)                        \
    if (!(ctxt).draw_sheet_)                        \
        return;                                     \
    if (!(ctxt).draw_sheet_->ocean_)                \
        return;                                     \
    auto& ocean_entt = *(ctxt).draw_sheet_->ocean_; \
    auto cmdbuf = (ctxt).cmdbuf_;


namespace {

    VkPipeline create_compute_pipeline(
        const dal::path& spv_path,
        const VkPipelineLayout pipeline_layout,
        mirinae::VulkanDevice& device
    ) {
        mirinae::PipelineBuilder::ShaderStagesBuilder shader_builder{ device };
        shader_builder.add_comp(spv_path);

        VkComputePipelineCreateInfo cinfo{};
        cinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        cinfo.layout = pipeline_layout;
        cinfo.stage = *shader_builder.data();

        VkPipeline pipeline = VK_NULL_HANDLE;
        const auto res = vkCreateComputePipelines(
            device.logi_device(), VK_NULL_HANDLE, 1, &cinfo, nullptr, &pipeline
        );
        MIRINAE_ASSERT(res == VK_SUCCESS);

        return pipeline;
    }

}  // namespace


// Ocean Tilde H
namespace {

    constexpr uint32_t CASCADE_COUNT = mirinae::cpnt::Ocean::CASCADE_COUNT;
    constexpr uint32_t OCEAN_TEX_DIM = 256;
    const uint32_t OCEAN_TEX_DIM_LOG2 = std::log(OCEAN_TEX_DIM) / std::log(2);

    // Range is [0, 1]
    double rand_uniform() { return (double)rand() / RAND_MAX; }


    struct U_OceanTildeHPushConst {
        glm::vec2 wind_dir_;
        float wind_speed_;
        float amplitude_;
        float fetch_;
        float swell_;
        float spread_blend_;
        float cutoff_high_;
        float cutoff_low_;
        int32_t N_;
        int32_t L_;
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
                    OCEAN_TEX_DIM * OCEAN_TEX_DIM * 4
                );
                for (size_t i = 0; i < noise_data.size(); i++)
                    noise_data[i] = static_cast<uint8_t>(rand_uniform() * 255);

                mirinae::ImageCreateInfo img_info;
                img_info.set_dimensions(OCEAN_TEX_DIM, OCEAN_TEX_DIM)
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
                    OCEAN_TEX_DIM,
                    OCEAN_TEX_DIM,
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
                cinfo.set_dimensions(OCEAN_TEX_DIM, OCEAN_TEX_DIM)
                    .set_format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.format(cinfo.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    for (size_t j = 0; j < CASCADE_COUNT; j++) {
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

            // Pipeline Layout
            {
                pipeline_layout_ =
                    mirinae::PipelineLayoutBuilder{}
                        .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                        .pc<U_OceanTildeHPushConst>()
                        .desc(desclayouts.get(name() + ":main").layout())
                        .build(device);
                MIRINAE_ASSERT(VK_NULL_HANDLE != pipeline_layout_);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder::ShaderStagesBuilder shader_builder{
                    device
                };
                shader_builder.add_comp(":asset/spv/ocean_tilde_h_comp.spv");

                VkComputePipelineCreateInfo cinfo{};
                cinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                cinfo.layout = pipeline_layout_;
                cinfo.stage = *shader_builder.data();

                const auto res = vkCreateComputePipelines(
                    device.logi_device(),
                    VK_NULL_HANDLE,
                    1,
                    &cinfo,
                    nullptr,
                    &pipeline_
                );
                MIRINAE_ASSERT(res == VK_SUCCESS);
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

            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != pipeline_layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), pipeline_layout_, nullptr
                );
                pipeline_layout_ = VK_NULL_HANDLE;
            }
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
                .layout(pipeline_layout_)
                .add(fd.desc_set_)
                .record(cmdbuf);

            ::U_OceanTildeHPushConst pc;
            pc.wind_dir_ = ocean_entt.wind_dir_;
            pc.wind_speed_ = ocean_entt.wind_speed_;
            pc.fetch_ = ocean_entt.fetch_;
            pc.swell_ = ocean_entt.swell_;
            pc.spread_blend_ = ocean_entt.spread_blend_;
            pc.N_ = ::OCEAN_TEX_DIM;
            pc.L_ = ocean_entt.L_;

            for (int i = 0; i < CASCADE_COUNT; ++i) {
                auto& cascade = ocean_entt.cascades_[i];
                pc.amplitude_ = cascade.amplitude();
                pc.cutoff_high_ = cascade.cutoff_high_;
                pc.cutoff_low_ = cascade.cutoff_low_;
                pc.cascade_ = i;

                mirinae::PushConstInfo pc_info;
                pc_info.layout(pipeline_layout_)
                    .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .record(cmdbuf, pc);

                vkCmdDispatch(
                    cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 1
                );
            }
        }

    private:
        struct FrameData {
            std::array<mirinae::HRpImage, CASCADE_COUNT> hk_;
            VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;
        mirinae::HRpImage noise_textures_;
        mirinae::DescPool desc_pool_;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;

        sung::MonotonicRealtimeTimer timer_;
    };

}  // namespace


// Ocean Tilde Hkt
namespace {

    struct U_OceanTildeHktPushConst {
        float time_;
        int32_t N_;
        int32_t L_;
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
                cinfo.set_dimensions(OCEAN_TEX_DIM, OCEAN_TEX_DIM)
                    .set_format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.format(cinfo.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];

                    for (size_t j = 0; j < CASCADE_COUNT; ++j) {
                        const auto img_name = fmt::format(
                            "hkt_dxdy_c{}_f{}", j, i
                        );
                        auto img = rp_res.new_img(img_name, this->name());
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());
                        img->view_.reset(builder, device);
                        fd.hkt_dxdy_[j] = img;
                    }

                    for (size_t j = 0; j < CASCADE_COUNT; ++j) {
                        const auto img_name = fmt::format(
                            "hkt_dz_c{}_f{}", j, i
                        );
                        auto img = rp_res.new_img(img_name, this->name());
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());
                        img->view_.reset(builder, device);
                        fd.hkt_dz_[j] = img;
                    }

                    for (size_t j = 0; j < CASCADE_COUNT; ++j) {
                        const auto img_name = fmt::format(
                            "hkt_ddxddz_c{}_f{}", j, i
                        );
                        auto img = rp_res.new_img(img_name, this->name());
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());
                        img->view_.reset(builder, device);
                        fd.hkt_ddxddz_[j] = img;
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
                    for (size_t i = 0; i < CASCADE_COUNT; ++i) {
                        barrier.image(fd.hkt_dxdy_[i]->img_.image());
                        barrier.record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
                        );

                        barrier.image(fd.hkt_dz_[i]->img_.image());
                        barrier.record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
                        );

                        barrier.image(fd.hkt_ddxddz_[i]->img_.image());
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
                for (size_t j = 0; j < CASCADE_COUNT; ++j) {
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
                    .new_binding()  // hkt_dxdy c0
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt_dxdy c1
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt_dxdy c2
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt_dz c0
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt_dz c1
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt_dz c2
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt_ddxddz c0
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt_ddxddz c1
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt_ddxddz c2
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hk c0
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hk c1
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hk c2
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
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

                mirinae::DescWriteInfoBuilder builder;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_ = desc_sets[i];

                    builder.set_descset(fd.desc_set_)
                        .add_storage_img(fd.hkt_dxdy_[0]->view_.get())
                        .add_storage_img(fd.hkt_dxdy_[1]->view_.get())
                        .add_storage_img(fd.hkt_dxdy_[2]->view_.get())
                        .add_storage_img(fd.hkt_dz_[0]->view_.get())
                        .add_storage_img(fd.hkt_dz_[1]->view_.get())
                        .add_storage_img(fd.hkt_dz_[2]->view_.get())
                        .add_storage_img(fd.hkt_ddxddz_[0]->view_.get())
                        .add_storage_img(fd.hkt_ddxddz_[1]->view_.get())
                        .add_storage_img(fd.hkt_ddxddz_[2]->view_.get())
                        .add_storage_img(fd.hk_[0]->view_.get())
                        .add_storage_img(fd.hk_[1]->view_.get())
                        .add_storage_img(fd.hk_[2]->view_.get());
                }
                builder.apply_all(device.logi_device());
            }

            // Pipeline Layout
            {
                pipeline_layout_ =
                    mirinae::PipelineLayoutBuilder{}
                        .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                        .pc<U_OceanTildeHktPushConst>()
                        .desc(desclayouts.get(name() + ":main").layout())
                        .build(device);
                MIRINAE_ASSERT(VK_NULL_HANDLE != pipeline_layout_);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder::ShaderStagesBuilder shader_builder{
                    device
                };
                shader_builder.add_comp(":asset/spv/ocean_tilde_hkt_comp.spv");

                VkComputePipelineCreateInfo cinfo{};
                cinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                cinfo.layout = pipeline_layout_;
                cinfo.stage = *shader_builder.data();

                const auto res = vkCreateComputePipelines(
                    device.logi_device(),
                    VK_NULL_HANDLE,
                    1,
                    &cinfo,
                    nullptr,
                    &pipeline_
                );
                MIRINAE_ASSERT(res == VK_SUCCESS);
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanTildeHkt() override {
            for (auto& fd : frame_data_) {
                for (size_t i = 0; i < CASCADE_COUNT; ++i) {
                    rp_res_.free_img(fd.hk_[i]->id(), this->name());
                    rp_res_.free_img(fd.hkt_dxdy_[i]->id(), this->name());
                    rp_res_.free_img(fd.hkt_dz_[i]->id(), this->name());
                    rp_res_.free_img(fd.hkt_ddxddz_[i]->id(), this->name());
                }
            }

            desc_pool_.destroy(device_.logi_device());

            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != pipeline_layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), pipeline_layout_, nullptr
                );
                pipeline_layout_ = VK_NULL_HANDLE;
            }
        }

        const std::string& name() const override {
            static const std::string name = "ocean_tilde_hkt";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            GET_OCEAN_ENTT(ctxt);
            auto& fd = frame_data_[ctxt.f_index_.get()];

            mirinae::ImageMemoryBarrier barrier;
            barrier.set_src_access(0)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_GENERAL)
                .new_layout(VK_IMAGE_LAYOUT_GENERAL)
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .mip_base(0)
                .mip_count(1)
                .layer_base(0)
                .layer_count(1);
            for (size_t i = 0; i < CASCADE_COUNT; ++i) {
                barrier.image(fd.hk_[i]->img_.image());
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
                .layout(pipeline_layout_)
                .add(fd.desc_set_)
                .record(cmdbuf);

            ::U_OceanTildeHktPushConst pc;
            pc.time_ = ocean_entt.time_;
            pc.N_ = ::OCEAN_TEX_DIM;
            pc.L_ = ocean_entt.L_;

            mirinae::PushConstInfo pc_info;
            pc_info.layout(pipeline_layout_)
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

            vkCmdDispatch(cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 1);
        }

    private:
        struct FrameData {
            std::array<mirinae::HRpImage, CASCADE_COUNT> hk_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> hkt_dxdy_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> hkt_dz_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> hkt_ddxddz_;
            VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;
        mirinae::DescPool desc_pool_;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;

        sung::MonotonicRealtimeTimer timer_;
    };

}  // namespace


// Ocean Butterfly
namespace {

    class ComplexNum {

    public:
        ComplexNum() = default;

        ComplexNum(double real, double imaginary) : re_(real), im_(imaginary) {}

        double re_ = 0;
        double im_ = 0;
    };


    unsigned int reverse_bits(unsigned int num) {
        unsigned int NO_OF_BITS = sizeof(num) * 8;
        unsigned int reverse_num = 0;
        int i;
        for (i = 0; i < NO_OF_BITS; i++) {
            if ((num & (1 << i)))
                reverse_num |= 1 << ((NO_OF_BITS - 1) - i);
        }
        return reverse_num;
    }

    dal::TDataImage2D<float> create_butterfly_cache_tex(
        uint32_t width, uint32_t height
    ) {
        std::vector<int> bit_reversed_indices(height);
        const int bits = width;
        const int right_shift = sizeof(int) * 8 - bits;
        for (uint32_t i = 0; i < height; i++) {
            unsigned int x = reverse_bits(i);
            bit_reversed_indices[i] = (x << bits) | (x >> right_shift);
        }

        dal::TDataImage2D<float> out;
        out.init(nullptr, width, height, 4);

        const double N = height;

        for (size_t i_x = 0; i_x < width; ++i_x) {
            for (size_t i_y = 0; i_y < height; ++i_y) {
                glm::dvec2 x = glm::vec2(i_x, i_y);
                auto k = std::fmod(x.y * (N / std::pow(2.0, x.x + 1.0)), N);
                auto twiddle = ComplexNum(
                    std::cos(SUNG_TAU * k / N), std::sin(SUNG_TAU * k / N)
                );

                int butterflyspan = int(std::pow(2.0, x.x));
                int butterflywing = 0;
                if (std::fmod(x.y, std::pow(2.0, x.x + 1.0)) <
                    std::pow(2.0, x.x)) {
                    butterflywing = 1;
                } else {
                    butterflywing = 0;
                }

                if (x.x == 0) {
                    if (butterflywing == 1) {
                        auto texel = out.texel_ptr(i_x, i_y);
                        texel[0] = twiddle.re_;
                        texel[1] = twiddle.im_;
                        texel[2] = bit_reversed_indices[i_y];
                        texel[3] = bit_reversed_indices[i_y + 1];
                    } else {
                        auto texel = out.texel_ptr(i_x, i_y);
                        texel[0] = twiddle.re_;
                        texel[1] = twiddle.im_;
                        texel[2] = bit_reversed_indices[i_y - 1];
                        texel[3] = bit_reversed_indices[i_y];
                    }
                } else {
                    if (butterflywing == 1) {
                        auto texel = out.texel_ptr(i_x, i_y);
                        texel[0] = twiddle.re_;
                        texel[1] = twiddle.im_;
                        texel[2] = x.y;
                        texel[3] = x.y + butterflyspan;
                    } else {
                        auto texel = out.texel_ptr(i_x, i_y);
                        texel[0] = twiddle.re_;
                        texel[1] = twiddle.im_;
                        texel[2] = x.y - butterflyspan;
                        texel[3] = x.y;
                    }
                }
            }
        }

        return out;
    }


    struct U_OceanButterflyPushConst {
        int32_t stage_;
        int32_t pingpong_;
        int32_t direction_;
    };


    class RpStatesOceanButterfly : public mirinae::IRpStates {

    public:
        RpStatesOceanButterfly(
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

                for (size_t j = 0; j < CASCADE_COUNT; j++) {
                    fd.hkt_textures_.push_back(rp_res.get_img_reader(
                        fmt::format("ocean_tilde_hkt:hkt_dxdy_c{}_f{}", j, i),
                        name()
                    ));
                    MIRINAE_ASSERT(nullptr != fd.hkt_textures_.back());
                    fd.hkt_textures_.push_back(rp_res.get_img_reader(
                        fmt::format("ocean_tilde_hkt:hkt_dz_c{}_f{}", j, i),
                        name()
                    ));
                    MIRINAE_ASSERT(nullptr != fd.hkt_textures_.back());
                    fd.hkt_textures_.push_back(rp_res.get_img_reader(
                        fmt::format("ocean_tilde_hkt:hkt_ddxddz_c{}_f{}", j, i),
                        name()
                    ));
                    MIRINAE_ASSERT(nullptr != fd.hkt_textures_.back());
                }
            }

            // Butterfly cache texture
            {
                const auto bufffly_img = ::create_butterfly_cache_tex(
                    OCEAN_TEX_DIM_LOG2, OCEAN_TEX_DIM
                );

                mirinae::ImageCreateInfo img_info;
                img_info
                    .set_dimensions(bufffly_img.width(), bufffly_img.height())
                    .deduce_mip_levels()
                    .set_format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .add_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                    .add_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                    .add_usage_sampled();

                mirinae::Buffer staging_buffer;
                staging_buffer.init_staging(
                    bufffly_img.data_size(), device_.mem_alloc()
                );
                staging_buffer.set_data(
                    bufffly_img.data(),
                    bufffly_img.data_size(),
                    device_.mem_alloc()
                );

                butterfly_cache_ = rp_res.new_img(
                    "butterfly_cache", this->name()
                );
                MIRINAE_ASSERT(nullptr != butterfly_cache_);
                butterfly_cache_->img_.init(
                    img_info.get(), device_.mem_alloc()
                );

                auto cmdbuf = cmd_pool.begin_single_time(device_);
                mirinae::record_img_buf_copy_mip(
                    cmdbuf,
                    bufffly_img.width(),
                    bufffly_img.height(),
                    img_info.mip_levels(),
                    butterfly_cache_->img_.image(),
                    staging_buffer.buffer()
                );
                cmd_pool.end_single_time(cmdbuf, device_);
                staging_buffer.destroy(device_.mem_alloc());

                mirinae::ImageViewBuilder iv_builder;
                iv_builder.format(img_info.format())
                    .mip_levels(img_info.mip_levels())
                    .image(butterfly_cache_->img_.image());
                butterfly_cache_->view_.reset(iv_builder, device_);
            }

            // Storage images
            {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_dimensions(OCEAN_TEX_DIM, OCEAN_TEX_DIM)
                    .set_format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.format(cinfo.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = fdata_[i];

                    for (size_t j = 0; j < fd.hkt_textures_.size(); j++) {
                        const auto i_name = fmt::format("ppong_i{}_f{}", j, i);
                        fd.ppong_textures_.push_back(
                            rp_res.new_img(i_name, name())
                        );
                        MIRINAE_ASSERT(nullptr != fd.ppong_textures_.back());

                        auto& img = fd.ppong_textures_.back()->img_;
                        img.init(cinfo.get(), device.mem_alloc());
                        builder.image(img.image());

                        fd.ppong_textures_.back()->view_.reset(builder, device);
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
                for (auto fd : fdata_) {
                    for (auto& ppong : fd.ppong_textures_) {
                        barrier.image(ppong->img_.image());
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
                    .new_binding(0)  // Pingpong images
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(9)
                    .finish_binding()
                    .new_binding(1)  // hkt images
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(9)
                    .finish_binding()
                    .new_binding(2)  // butfly_cache
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(1)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
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

                    MIRINAE_ASSERT(9 == fd.ppong_textures_.size());

                    // Pingpong images
                    for (auto& ppong : fd.ppong_textures_)
                        writer.add_storage_img_info(ppong->view_.get());
                    writer.add_storage_img_write(fd.desc_set_, 0);
                    // hkt images
                    for (auto& ppong : fd.hkt_textures_)
                        writer.add_storage_img_info(ppong->view_.get());
                    writer.add_storage_img_write(fd.desc_set_, 1);
                    // Butterfly texture
                    writer.add_img_info()
                        .set_img_view(butterfly_cache_->view_.get())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        .set_sampler(device.samplers().get_nearest());
                    writer.add_sampled_img_write(fd.desc_set_, 2);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline
            {
                pipeline_layout_ =
                    mirinae::PipelineLayoutBuilder{}
                        .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                        .pc<U_OceanButterflyPushConst>()
                        .desc(desclayouts.get(name() + ":main").layout())
                        .build(device);
                MIRINAE_ASSERT(VK_NULL_HANDLE != pipeline_layout_);

                pipeline_ = ::create_compute_pipeline(
                    ":asset/spv/ocean_butterfly_comp.spv",
                    pipeline_layout_,
                    device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanButterfly() override {
            for (auto& fdata : fdata_) {
                for (auto& ppong : fdata.ppong_textures_)
                    rp_res_.free_img(ppong->id(), this->name());
                for (auto& hkt : fdata.hkt_textures_)
                    rp_res_.free_img(hkt->id(), this->name());

                fdata.desc_set_ = VK_NULL_HANDLE;
            }

            rp_res_.free_img(butterfly_cache_->id(), this->name());
            desc_pool_.destroy(device_.logi_device());

            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != pipeline_layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), pipeline_layout_, nullptr
                );
                pipeline_layout_ = VK_NULL_HANDLE;
            }
        }

        const std::string& name() const override {
            static const std::string name = "ocean_butterfly";
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
                .layout(pipeline_layout_)
                .add(fd.desc_set_)
                .record(cmdbuf);

            mirinae::PushConstInfo pc_info;
            pc_info.layout(pipeline_layout_)
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT);

            VkMemoryBarrier mem_bar = {};
            mem_bar.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            mem_bar.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            mem_bar.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

            ::U_OceanButterflyPushConst pc;
            pc.stage_ = 0;
            pc.pingpong_ = 1;
            pc.direction_ = 0;

            // one dimensional FFT in horizontal direction
            for (int stage = 0; stage < OCEAN_TEX_DIM_LOG2; stage++) {
                pc.direction_ = 0;
                pc.stage_ = stage;
                pc_info.record(cmdbuf, pc);

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

                vkCmdDispatch(
                    cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 9
                );

                pc.pingpong_ = !pc.pingpong_;
            }

            for (int stage = 0; stage < OCEAN_TEX_DIM_LOG2; stage++) {
                pc.direction_ = 1;
                pc.stage_ = stage;
                pc_info.record(cmdbuf, pc);

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

                vkCmdDispatch(
                    cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 9
                );

                pc.pingpong_ = !pc.pingpong_;
            }
        }

    private:
        struct FrameData {
            std::vector<mirinae::HRpImage> hkt_textures_;
            std::vector<mirinae::HRpImage> ppong_textures_;
            VkDescriptorSet desc_set_;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> fdata_;
        mirinae::HRpImage butterfly_cache_;
        mirinae::DescPool desc_pool_;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    };

}  // namespace


// Ocean Naive IFT
namespace {

    struct U_OceanNaiveIftPushConst {
        int32_t N_;
        int32_t L_;
        int32_t stage_;  // 0: hor, 1: ver
    };


    class RpStatesOceanNaiveIft : public mirinae::IRpStates {

    public:
        RpStatesOceanNaiveIft(
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

                fd.hkt_dxdy_ = rp_res.get_img_reader(
                    fmt::format("ocean_tilde_hkt:hkt_dxdy_c0_f{}", i), name()
                );
                MIRINAE_ASSERT(nullptr != fd.hkt_dxdy_);
                fd.hkt_dz_ = rp_res.get_img_reader(
                    fmt::format("ocean_tilde_hkt:hkt_dz_c0_f{}", i), name()
                );
                MIRINAE_ASSERT(nullptr != fd.hkt_dz_);
                fd.hkt_ddxddz_ = rp_res.get_img_reader(
                    fmt::format("ocean_tilde_hkt:hkt_ddxddz_c0_f{}", i), name()
                );
                MIRINAE_ASSERT(nullptr != fd.hkt_ddxddz_);
            }

            // Storage images
            {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_dimensions(OCEAN_TEX_DIM, OCEAN_TEX_DIM)
                    .set_format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.format(cinfo.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = fdata_[i];

                    {
                        const auto i_name = fmt::format(
                            "ppong_naive_xy_f#{}", i
                        );
                        fd.pingpong_xy_ = rp_res.new_img(i_name, name());
                        MIRINAE_ASSERT(nullptr != fd.pingpong_xy_);

                        auto& img = fd.pingpong_xy_->img_;
                        img.init(cinfo.get(), device.mem_alloc());
                        builder.image(img.image());

                        fd.pingpong_xy_->view_.reset(builder, device);
                    }

                    {
                        const auto i_name = fmt::format(
                            "ppong_naive_z_f#{}", i
                        );
                        fd.pingpong_z_ = rp_res.new_img(i_name, name());
                        MIRINAE_ASSERT(nullptr != fd.pingpong_z_);

                        auto& img = fd.pingpong_z_->img_;
                        img.init(cinfo.get(), device.mem_alloc());
                        builder.image(img.image());

                        fd.pingpong_z_->view_.reset(builder, device);
                    }

                    {
                        const auto i_name = fmt::format(
                            "ppong_naive_dxdz_f#{}", i
                        );
                        fd.pingpong_dxdz_ = rp_res.new_img(i_name, name());
                        MIRINAE_ASSERT(nullptr != fd.pingpong_dxdz_);

                        auto& img = fd.pingpong_dxdz_->img_;
                        img.init(cinfo.get(), device.mem_alloc());
                        builder.image(img.image());

                        fd.pingpong_dxdz_->view_.reset(builder, device);
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
                for (auto fd : fdata_) {
                    barrier.image(fd.pingpong_xy_->img_.image());
                    barrier.record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT
                    );

                    barrier.image(fd.pingpong_z_->img_.image());
                    barrier.record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT
                    );

                    barrier.image(fd.pingpong_dxdz_->img_.image());
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
                    .new_binding()  // pingpong xy
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // pingpong z
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // pingpong dxdz
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt dxdy
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt dz
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt ddxddz
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .add_img(VK_SHADER_STAGE_COMPUTE_BIT, 1);  // butfly_cache
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

                mirinae::DescWriteInfoBuilder builder;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = fdata_[i];
                    fd.desc_set_ = sets[i];

                    builder.set_descset(fd.desc_set_)
                        .add_storage_img(fd.pingpong_xy_->view_.get())
                        .add_storage_img(fd.pingpong_z_->view_.get())
                        .add_storage_img(fd.pingpong_dxdz_->view_.get())
                        .add_storage_img(fd.hkt_dxdy_->view_.get())
                        .add_storage_img(fd.hkt_dz_->view_.get())
                        .add_storage_img(fd.hkt_ddxddz_->view_.get());
                }
                builder.apply_all(device.logi_device());
            }

            // Pipeline
            {
                pipeline_layout_ =
                    mirinae::PipelineLayoutBuilder{}
                        .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                        .pc<U_OceanNaiveIftPushConst>()
                        .desc(desclayouts.get(name() + ":main").layout())
                        .build(device);
                MIRINAE_ASSERT(VK_NULL_HANDLE != pipeline_layout_);

                pipeline_ = ::create_compute_pipeline(
                    ":asset/spv/ocean_naive_ift_comp.spv",
                    pipeline_layout_,
                    device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanNaiveIft() override {
            for (auto& fdata : fdata_) {
                rp_res_.free_img(fdata.hkt_dxdy_->id(), this->name());
                rp_res_.free_img(fdata.hkt_dz_->id(), this->name());
                rp_res_.free_img(fdata.hkt_ddxddz_->id(), this->name());
                rp_res_.free_img(fdata.pingpong_xy_->id(), this->name());
                rp_res_.free_img(fdata.pingpong_z_->id(), this->name());
                rp_res_.free_img(fdata.pingpong_dxdz_->id(), this->name());
                fdata.desc_set_ = VK_NULL_HANDLE;
            }

            desc_pool_.destroy(device_.logi_device());

            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != pipeline_layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), pipeline_layout_, nullptr
                );
                pipeline_layout_ = VK_NULL_HANDLE;
            }
        }

        const std::string& name() const override {
            static const std::string name = "ocean_naive_ift";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            GET_OCEAN_ENTT(ctxt);
            auto& fd = fdata_[ctxt.f_index_.get()];

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

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_
            );

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
                .layout(pipeline_layout_)
                .add(fd.desc_set_)
                .record(cmdbuf);

            ::U_OceanNaiveIftPushConst pc;
            pc.L_ = ocean_entt.L_;
            pc.N_ = ::OCEAN_TEX_DIM;
            pc.stage_ = 0;

            mirinae::PushConstInfo pc_info;
            pc_info.layout(pipeline_layout_)
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

            vkCmdDispatch(cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 1);

            vkCmdPipelineBarrier(
                cmdbuf,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                1,
                &mem_bar,
                0,
                nullptr,
                0,
                nullptr
            );

            pc.stage_ = 1;
            pc_info.record(cmdbuf, pc);

            vkCmdDispatch(cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 1);

            vkCmdPipelineBarrier(
                cmdbuf,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                1,
                &mem_bar,
                0,
                nullptr,
                0,
                nullptr
            );
        }

    private:
        struct FrameData {
            mirinae::HRpImage hkt_dxdy_;
            mirinae::HRpImage hkt_dz_;
            mirinae::HRpImage hkt_ddxddz_;
            mirinae::HRpImage pingpong_xy_;
            mirinae::HRpImage pingpong_z_;
            mirinae::HRpImage pingpong_dxdz_;
            VkDescriptorSet desc_set_;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> fdata_;
        mirinae::DescPool desc_pool_;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    };

}  // namespace


// Ocean Finalize
namespace {

    struct U_OceanFinalizePushConst {
        int32_t N_;
        int32_t idx_;
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

                for (size_t j = 0; j < CASCADE_COUNT; j++) {
                    fd.hkt_dxdy_[j] = rp_res.get_img_reader(
                        fmt::format("ocean_tilde_hkt:hkt_dxdy_c{}_f{}", j, i),
                        name()
                    );
                    MIRINAE_ASSERT(nullptr != fd.hkt_dxdy_[j]);
                    fd.hkt_dz_[j] = rp_res.get_img_reader(
                        fmt::format("ocean_tilde_hkt:hkt_dz_c{}_f{}", j, i),
                        name()
                    );
                    MIRINAE_ASSERT(nullptr != fd.hkt_dz_[j]);
                    fd.hkt_ddxddz_[j] = rp_res.get_img_reader(
                        fmt::format("ocean_tilde_hkt:hkt_ddxddz_c{}_f{}", j, i),
                        name()
                    );
                    MIRINAE_ASSERT(nullptr != fd.hkt_ddxddz_[j]);
                }
            }

            // Storage images
            {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_dimensions(OCEAN_TEX_DIM, OCEAN_TEX_DIM)
                    .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = fdata_[i];

                    for (size_t j = 0; j < CASCADE_COUNT; j++) {
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

                        fd.displacement_[j] = std::move(img);
                    }

                    for (size_t j = 0; j < CASCADE_COUNT; j++) {
                        const auto i_name = fmt::format("normal_c{}_f{}", j, i);
                        auto img = rp_res.new_img(i_name, name());
                        MIRINAE_ASSERT(nullptr != img);

                        cinfo.set_format(VK_FORMAT_R8G8B8A8_UNORM);
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());

                        builder.format(cinfo.format());
                        img->view_.reset(builder, device);

                        fd.normal_[j] = std::move(img);
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
                for (auto fd : fdata_) {
                    for (size_t i = 0; i < CASCADE_COUNT; i++) {
                        barrier.image(fd.displacement_[i]->img_.image());
                        barrier.record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
                        );

                        barrier.image(fd.normal_[i]->img_.image());
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
                    .new_binding()  // displacement
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // normal
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

                    writer
                        .add_storage_img_info(fd.displacement_[0]->view_.get())
                        .add_storage_img_info(fd.displacement_[1]->view_.get())
                        .add_storage_img_info(fd.displacement_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 0);
                    writer.add_storage_img_info(fd.normal_[0]->view_.get())
                        .add_storage_img_info(fd.normal_[1]->view_.get())
                        .add_storage_img_info(fd.normal_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 1);
                    writer.add_storage_img_info(fd.hkt_dxdy_[0]->view_.get())
                        .add_storage_img_info(fd.hkt_dxdy_[1]->view_.get())
                        .add_storage_img_info(fd.hkt_dxdy_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 2);
                    writer.add_storage_img_info(fd.hkt_dz_[0]->view_.get())
                        .add_storage_img_info(fd.hkt_dz_[1]->view_.get())
                        .add_storage_img_info(fd.hkt_dz_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 3);
                    writer.add_storage_img_info(fd.hkt_ddxddz_[0]->view_.get())
                        .add_storage_img_info(fd.hkt_ddxddz_[1]->view_.get())
                        .add_storage_img_info(fd.hkt_ddxddz_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 4);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline
            {
                pipeline_layout_ =
                    mirinae::PipelineLayoutBuilder{}
                        .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                        .pc<U_OceanFinalizePushConst>()
                        .desc(desclayouts.get(name() + ":main").layout())
                        .build(device);
                MIRINAE_ASSERT(VK_NULL_HANDLE != pipeline_layout_);

                pipeline_ = ::create_compute_pipeline(
                    ":asset/spv/ocean_finalize_comp.spv",
                    pipeline_layout_,
                    device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanFinalize() override {
            for (auto& fdata : fdata_) {
                for (size_t i = 0; i < CASCADE_COUNT; i++) {
                    rp_res_.free_img(fdata.hkt_dxdy_[i]->id(), this->name());
                    rp_res_.free_img(fdata.hkt_dz_[i]->id(), this->name());
                    rp_res_.free_img(fdata.hkt_ddxddz_[i]->id(), this->name());
                    rp_res_.free_img(
                        fdata.displacement_[i]->id(), this->name()
                    );
                    rp_res_.free_img(fdata.normal_[i]->id(), this->name());
                }

                fdata.desc_set_ = VK_NULL_HANDLE;
            }

            desc_pool_.destroy(device_.logi_device());

            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != pipeline_layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), pipeline_layout_, nullptr
                );
                pipeline_layout_ = VK_NULL_HANDLE;
            }
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
                .layout(pipeline_layout_)
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
            pc.N_ = ::OCEAN_TEX_DIM;
            pc.idx_ = ocean_entt.idx_ % 3;

            mirinae::PushConstInfo{}
                .layout(pipeline_layout_)
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

            vkCmdDispatch(cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 1);
        }

    private:
        struct FrameData {
            std::array<mirinae::HRpImage, CASCADE_COUNT> hkt_dxdy_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> hkt_dz_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> hkt_ddxddz_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> displacement_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> normal_;
            VkDescriptorSet desc_set_;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> fdata_;
        mirinae::DescPool desc_pool_;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    };

}  // namespace


// Ocean tessellation
namespace {

    struct U_OceanTessParams {

    public:
        U_OceanTessParams& height_map_size(uint32_t x, uint32_t y) {
            height_map_size_fbuf_size_.x = static_cast<float>(x);
            height_map_size_fbuf_size_.y = static_cast<float>(y);
            return *this;
        }

        U_OceanTessParams& fbuf_size(const VkExtent2D& x) {
            height_map_size_fbuf_size_.z = static_cast<float>(x.width);
            height_map_size_fbuf_size_.w = static_cast<float>(x.height);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& tile_dimensions(T x, T y) {
            tile_dimensions_.x = static_cast<float>(x);
            tile_dimensions_.y = static_cast<float>(y);
            return *this;
        }

        U_OceanTessParams& texco_offset(size_t idx, float x, float y) {
            texco_offset_rot_[idx].x = x;
            texco_offset_rot_[idx].y = y;
            return *this;
        }

        U_OceanTessParams& texco_offset(size_t idx, const glm::vec2& v) {
            return this->texco_offset(idx, v.x, v.y);
        }

        U_OceanTessParams& texco_scale(size_t idx, float x, float y) {
            texco_offset_rot_[idx].z = x;
            texco_offset_rot_[idx].w = y;
            return *this;
        }

        U_OceanTessParams& texco_scale(size_t idx, const glm::vec2& v) {
            return this->texco_scale(idx, v.x, v.y);
        }

        glm::vec4 texco_offset_rot_[CASCADE_COUNT];
        glm::vec4 height_map_size_fbuf_size_;
        glm::vec2 tile_dimensions_;
    };


    class U_OceanTessPushConst {

    public:
        U_OceanTessPushConst& pvm(
            const glm::dmat4& proj,
            const glm::dmat4& view,
            const glm::dmat4& model
        ) {
            pvm_ = proj * view * model;
            view_ = view;
            model_ = model;
            return *this;
        }

        U_OceanTessPushConst& tile_index(int x, int y) {
            tile_index_count_.x = static_cast<float>(x);
            tile_index_count_.y = static_cast<float>(y);
            return *this;
        }

        U_OceanTessPushConst& tile_count(int x, int y) {
            tile_index_count_.z = static_cast<float>(x);
            tile_index_count_.w = static_cast<float>(y);
            return *this;
        }

    private:
        glm::mat4 pvm_;
        glm::mat4 view_;
        glm::mat4 model_;
        glm::vec4 tile_index_count_;
    };


    class RpStatesOceanTess : public mirinae::IRpStates {

    public:
        RpStatesOceanTess(
            size_t swapchain_count,
            VkImageView sky_tex,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            // Images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_[i];

                for (size_t j = 0; j < CASCADE_COUNT; j++) {
                    const auto img_name = fmt::format(
                        "ocean_finalize:displacement_c{}_f{}", j, i
                    );
                    fd.height_map_[j] = rp_res.get_img_reader(
                        img_name, this->name()
                    );
                    MIRINAE_ASSERT(nullptr != fd.height_map_[j]);
                }

                for (size_t j = 0; j < CASCADE_COUNT; j++) {
                    const auto img_name = fmt::format(
                        "ocean_finalize:normal_c{}_f{}", j, i
                    );
                    fd.normal_map_[j] = rp_res.get_img_reader(
                        img_name, this->name()
                    );
                    MIRINAE_ASSERT(nullptr != fd.normal_map_[j]);
                }
            }

            // Ubuf
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_[i];
                fd.ubuf_.init_ubuf<U_OceanTessParams>(device.mem_alloc());
            }

            // Descriptor layout
            {
                mirinae::DescLayoutBuilder builder{ this->name() + ":main" };
                builder
                    .new_binding()  // U_OceanTessParams
                    .set_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    .set_count(1)
                    .add_stage(VK_SHADER_STAGE_VERTEX_BIT)
                    .add_stage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
                    .add_stage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
                    .add_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .finish_binding();
                builder
                    .new_binding()  // Height map
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(3)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .add_stage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
                    .finish_binding();
                builder
                    .new_binding()  // Normal map
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(3)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .finish_binding()
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // Sky texture
                desclayouts.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                desc_pool_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desclayouts.get(name() + ":main").size_info(),
                    device.logi_device()
                );

                auto desc_sets = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desclayouts.get(name() + ":main").layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_ = desc_sets[i];

                    writer  // U_OceanTessParams
                        .add_buf_info(fd.ubuf_)
                        .add_buf_write(fd.desc_set_, 0);
                    writer  // Height maps
                        .add_img_info()
                        .set_img_view(fd.height_map_[0]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.height_map_[1]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.height_map_[2]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_sampled_img_write(fd.desc_set_, 1);
                    writer  // Normal maps
                        .add_img_info()
                        .set_img_view(fd.normal_map_[0]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.normal_map_[1]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.normal_map_[2]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_sampled_img_write(fd.desc_set_, 2);

                    writer.add_img_info()
                        .set_img_view(sky_tex)
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 3);
                }
                writer.apply_all(device.logi_device());
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(fbuf_bundle.compo().format())
                    .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();
                builder.attach_desc()
                    .add(fbuf_bundle.depth().format())
                    .ini_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .op_pair_load_store();

                builder.color_attach_ref().add_color_attach(0);
                builder.depth_attach_ref().set(1);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                pipe_layout_ =
                    mirinae::PipelineLayoutBuilder{}
                        .desc(desclayouts.get(name() + ":main").layout())
                        .add_vertex_flag()
                        .add_tesc_flag()
                        .add_tese_flag()
                        .add_frag_flag()
                        .pc<U_OceanTessPushConst>(0)
                        .build(device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/ocean_tess_vert.spv")
                    .add_tesc(":asset/spv/ocean_tess_tesc.spv")
                    .add_tese(":asset/spv/ocean_tess_tese.spv")
                    .add_frag(":asset/spv/ocean_tess_frag.spv");

                builder.input_assembly_state().topology_patch_list();

                builder.tes_state().patch_ctrl_points(4);

                builder.rasterization_state().cull_mode_back();
                // builder.rasterization_state().polygon_mode(VK_POLYGON_MODE_LINE);

                builder.depth_stencil_state()
                    .depth_test_enable(true)
                    .depth_write_enable(true);

                builder.color_blend_state().add(false, 1);

                builder.dynamic_state()
                    .add(VK_DYNAMIC_STATE_LINE_WIDTH)
                    .add_viewport()
                    .add_scissor();

                pipeline_ = builder.build(render_pass_, pipe_layout_);
            }

            // Framebuffers
            {
                fbuf_width_ = fbuf_bundle.width();
                fbuf_height_ = fbuf_bundle.height();

                mirinae::FbufCinfo cinfo;
                cinfo.set_rp(render_pass_)
                    .add_attach(fbuf_bundle.compo().image_view())
                    .add_attach(fbuf_bundle.depth().image_view())
                    .set_dim(fbuf_width_, fbuf_height_);
                for (int i = 0; i < swapchain_count; ++i)
                    fbufs_.push_back(cinfo.build(device));
            }

            // Misc
            {
                clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };
                clear_values_.at(1).depthStencil = { 1.0f, 0 };
            }

            return;
        }

        ~RpStatesOceanTess() override {
            for (auto& fd : frame_data_) {
                for (size_t i = 0; i < CASCADE_COUNT; i++) {
                    rp_res_.free_img(fd.height_map_[i]->id(), this->name());
                    rp_res_.free_img(fd.normal_map_[i]->id(), this->name());
                }

                fd.ubuf_.destroy(device_.mem_alloc());
                fd.desc_set_ = VK_NULL_HANDLE;
            }

            desc_pool_.destroy(device_.logi_device());

            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != pipe_layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), pipe_layout_, nullptr
                );
                pipe_layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != render_pass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), render_pass_, nullptr
                );
                render_pass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        void record(const mirinae::RpContext& ctxt) override {
            GET_OCEAN_ENTT(ctxt);
            auto& fd = frame_data_[ctxt.f_index_.get()];

            const VkExtent2D fbuf_exd{ fbuf_width_, fbuf_height_ };

            U_OceanTessParams ubuf;
            ubuf.height_map_size(OCEAN_TEX_DIM, OCEAN_TEX_DIM)
                .fbuf_size(fbuf_exd)
                .tile_dimensions(ocean_entt.L_, ocean_entt.L_);
            for (size_t i = 0; i < CASCADE_COUNT; i++)
                ubuf.texco_offset(i, ocean_entt.cascades_[i].texcoord_offset_);
            fd.ubuf_.set_data(ubuf, device_.mem_alloc());

            mirinae::RenderPassBeginInfo{}
                .rp(render_pass_)
                .fbuf(fbufs_.at(ctxt.i_index_.get()))
                .wh(fbuf_width_, fbuf_height_)
                .clear_value_count(clear_values_.size())
                .clear_values(clear_values_.data())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_
            );

            mirinae::Viewport{ fbuf_exd }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_exd }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(pipe_layout_)
                .add(fd.desc_set_)
                .record(cmdbuf);

            mirinae::PushConstInfo pc_info;
            pc_info.layout(pipe_layout_)
                .add_stage_vert()
                .add_stage_tesc()
                .add_stage_tese()
                .add_stage_frag();

            U_OceanTessPushConst pc;
            pc.tile_count(10, 10).pvm(
                ctxt.proj_mat_,
                ctxt.view_mat_,
                ocean_entt.transform_.make_model_mat()
            );

            for (int x = 0; x < 10; ++x) {
                for (int y = 0; y < 10; ++y) {
                    pc.tile_index(x, y);
                    pc_info.record(cmdbuf, pc);
                    vkCmdDraw(cmdbuf, 4, 1, 0, 0);
                }
            }

            vkCmdEndRenderPass(cmdbuf);
        }

        const std::string& name() const override {
            static const std::string name = "ocean_tess";
            return name;
        }

    private:
        struct FrameData {
            std::array<mirinae::HRpImage, CASCADE_COUNT> height_map_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> normal_map_;
            mirinae::Buffer ubuf_;
            VkDescriptorSet desc_set_;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;
        mirinae::DescPool desc_pool_;
        VkRenderPass render_pass_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout pipe_layout_ = VK_NULL_HANDLE;

        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images
        std::array<VkClearValue, 2> clear_values_;
        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
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

    URpStates create_rp_states_ocean_tilde_hkt(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesOceanTildeHkt>(
            rp_res, desclayouts, device
        );
    }

    URpStates create_rp_states_ocean_butterfly(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesOceanButterfly>(
            rp_res, desclayouts, device
        );
    }

    URpStates create_rp_states_ocean_naive_ift(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesOceanNaiveIft>(
            rp_res, desclayouts, device
        );
    }

    URpStates create_rp_states_ocean_finalize(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesOceanFinalize>(
            rp_res, desclayouts, device
        );
    }

    URpStates create_rp_states_ocean_tess(
        size_t swapchain_count,
        VkImageView sky_tex,
        mirinae::FbufImageBundle& fbuf_bundle,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesOceanTess>(
            swapchain_count, sky_tex, fbuf_bundle, rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp::ocean
