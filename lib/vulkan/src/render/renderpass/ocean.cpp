#include "mirinae/render/renderpass/ocean.hpp"

#include <sung/basic/time.hpp>

#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/renderpass/builder.hpp"


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

    constexpr uint32_t OCEAN_TEX_DIM = 256;


    struct U_OceanTildeHPushConst {
        float time_;
    };


    class RpStatesOceanTildeH : public mirinae::rp::ocean::IRpStates {

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
                std::array<uint8_t, 256 * 256 * 1> noise_data;
                mirinae::Buffer staging_buffer;
                staging_buffer.init_staging(
                    noise_data.size(), device_.mem_alloc()
                );

                mirinae::ImageCreateInfo img_info;
                img_info.set_dimensions(256, 256)
                    .set_format(VK_FORMAT_R8_UNORM)
                    .deduce_mip_levels()
                    .add_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                    .add_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                    .add_usage_sampled();

                mirinae::ImageViewBuilder iv_builder;
                iv_builder.format(img_info.format())
                    .mip_levels(img_info.mip_levels());

                for (size_t i = 0; i < noise_textures_.size(); ++i) {
                    for (size_t i = 0; i < noise_data.size(); i++)
                        noise_data[i] = static_cast<uint8_t>(rand() % 256);

                    staging_buffer.set_data(
                        noise_data.data(),
                        noise_data.size(),
                        device_.mem_alloc()
                    );

                    const auto img_name = fmt::format("ocean_noise_{}", i);
                    auto img = rp_res.new_img(img_name, this->name());
                    MIRINAE_ASSERT(nullptr != img);
                    noise_textures_[i] = img;
                    img->img_.init(img_info.get(), device_.mem_alloc());

                    auto cmdbuf = cmd_pool.begin_single_time(device_);
                    mirinae::record_img_buf_copy_mip(
                        cmdbuf,
                        256,
                        256,
                        img_info.mip_levels(),
                        img->img_.image(),
                        staging_buffer.buffer()
                    );
                    cmd_pool.end_single_time(cmdbuf, device_);

                    iv_builder.image(img->img_.image());
                    img->view_.reset(iv_builder, device_);
                }

                staging_buffer.destroy(device_.mem_alloc());
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
                    const auto img_name = fmt::format("height_map_f#{}", i);
                    auto img = rp_res.new_img(img_name, this->name());
                    img->img_.init(cinfo.get(), device.mem_alloc());
                    builder.image(img->img_.image());
                    img->view_.reset(builder, device);
                    imgs_.push_back(std::move(img));
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
                for (auto p_img : imgs_) {
                    barrier.image(p_img->img_.image());
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
                builder.new_binding()
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .add_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .set_count(1)
                    .finish_binding()
                    .add_img(VK_SHADER_STAGE_COMPUTE_BIT, 1)
                    .add_img(VK_SHADER_STAGE_COMPUTE_BIT, 1)
                    .add_img(VK_SHADER_STAGE_COMPUTE_BIT, 1)
                    .add_img(VK_SHADER_STAGE_COMPUTE_BIT, 1);
                desclayouts.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                auto& layout = desclayouts.get(name() + ":main");

                desc_pool_.init(
                    static_cast<uint32_t>(imgs_.size()),
                    layout.size_info(),
                    device.logi_device()
                );

                desc_sets_ = desc_pool_.alloc(
                    static_cast<uint32_t>(imgs_.size()),
                    layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriteInfoBuilder builder;
                for (size_t i = 0; i < imgs_.size(); i++) {
                    builder.set_descset(desc_sets_[i])
                        .add_storage_img(imgs_[i]->view_.get())
                        .add_img_sampler(
                            noise_textures_[0]->view_.get(),
                            device.samplers().get_nearest()
                        )
                        .add_img_sampler(
                            noise_textures_[1]->view_.get(),
                            device.samplers().get_nearest()
                        )
                        .add_img_sampler(
                            noise_textures_[2]->view_.get(),
                            device.samplers().get_nearest()
                        )
                        .add_img_sampler(
                            noise_textures_[3]->view_.get(),
                            device.samplers().get_nearest()
                        );
                }
                builder.apply_all(device.logi_device());
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
            for (auto& img : noise_textures_)
                rp_res_.free_img(img->id(), this->name());
            for (auto& img : imgs_) rp_res_.free_img(img->id(), this->name());
            imgs_.clear();

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

        void record(const mirinae::rp::ocean::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_
            );

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
                .layout(pipeline_layout_)
                .add(desc_sets_.at(ctxt.f_index_.get()))
                .record(cmdbuf);

            ::U_OceanTildeHPushConst pc;
            pc.time_ = timer_.elapsed();

            mirinae::PushConstInfo pc_info;
            pc_info.layout(pipeline_layout_)
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

            vkCmdDispatch(cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 1);
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<mirinae::HRpImage, 4> noise_textures_;
        std::vector<mirinae::HRpImage> imgs_;
        std::vector<VkDescriptorSet> desc_sets_;
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
    };


    class RpStatesOceanTildeHkt : public mirinae::rp::ocean::IRpStates {

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
                    auto& img_struct = hkt_images_.emplace_back();

                    {
                        const auto img_name = fmt::format("hkt_dxdy_f#{}", i);
                        auto img = rp_res.new_img(img_name, this->name());
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());
                        img->view_.reset(builder, device);
                        img_struct.hkt_dxdy_ = img;
                    }

                    {
                        const auto img_name = fmt::format("hkt_dz_f#{}", i);
                        auto img = rp_res.new_img(img_name, this->name());
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());
                        img->view_.reset(builder, device);
                        img_struct.hkt_dz_ = img;
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
                for (auto img_struct : hkt_images_) {
                    barrier.image(img_struct.hkt_dxdy_->img_.image());
                    barrier.record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT
                    );

                    barrier.image(img_struct.hkt_dz_->img_.image());
                    barrier.record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT
                    );
                }
                cmd_pool.end_single_time(cmdbuf, device);
                cmd_pool.destroy(device.logi_device());
            }

            // Reference images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                const auto img_name = fmt::format(
                    "ocean_tilde_h:height_map_f#{}", i
                );
                auto img = rp_res.get_img_reader(img_name, this->name());
                MIRINAE_ASSERT(nullptr != img);
                hk_images_.push_back(img);
            }

            // Desc layouts
            {
                mirinae::DescLayoutBuilder builder{ name() + ":main" };
                builder
                    .new_binding()  // hkt_dxdy
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hkt_dz
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding()
                    .new_binding()  // hk
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

                desc_sets_ = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriteInfoBuilder builder;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& images = hkt_images_[i];

                    builder.set_descset(desc_sets_[i])
                        .add_storage_img(images.hkt_dxdy_->view_.get())
                        .add_storage_img(images.hkt_dz_->view_.get())
                        .add_storage_img(hk_images_[i]->view_.get());
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
            for (auto& img : hk_images_)
                rp_res_.free_img(img->id(), this->name());
            for (auto& img : hkt_images_) {
                rp_res_.free_img(img.hkt_dxdy_->id(), this->name());
                rp_res_.free_img(img.hkt_dz_->id(), this->name());
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

        void record(const mirinae::rp::ocean::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;

            mirinae::ImageMemoryBarrier barrier;
            barrier.image(hk_images_[ctxt.f_index_.get()]->img_.image())
                .set_src_access(0)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_GENERAL)
                .new_layout(VK_IMAGE_LAYOUT_GENERAL)
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .mip_base(0)
                .mip_count(1)
                .layer_base(0)
                .layer_count(1);
            barrier.record_single(
                cmdbuf,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
            );

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_
            );

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
                .layout(pipeline_layout_)
                .add(desc_sets_.at(ctxt.f_index_.get()))
                .record(cmdbuf);

            ::U_OceanTildeHktPushConst pc;
            pc.time_ = 5;

            mirinae::PushConstInfo pc_info;
            pc_info.layout(pipeline_layout_)
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

            vkCmdDispatch(cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 1);
        }

    private:
        struct TildeHktTextures {
            mirinae::HRpImage hkt_dxdy_;
            mirinae::HRpImage hkt_dz_;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::vector<mirinae::HRpImage> hk_images_;
        std::vector<TildeHktTextures> hkt_images_;
        std::vector<VkDescriptorSet> desc_sets_;
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
            int x = reverse_bits(i);
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
                    std::cos(2.0 * SUNG_PI * k / N),
                    std::sin(2.0 * SUNG_PI * k / N)
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


    class RpStatesOceanButterfly : public mirinae::rp::ocean::IRpStates {

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

                fd.hkt_dxdy_ = rp_res.get_img_reader(
                    fmt::format("ocean_tilde_hkt:hkt_dxdy_f#{}", i), name()
                );
                MIRINAE_ASSERT(nullptr != fd.hkt_dxdy_);
                fd.hkt_dz_ = rp_res.get_img_reader(
                    fmt::format("ocean_tilde_hkt:hkt_dz_f#{}", i), name()
                );
                MIRINAE_ASSERT(nullptr != fd.hkt_dz_);
            }

            // Butterfly cache texture
            {
                const auto bufffly_img = ::create_butterfly_cache_tex(
                    std::log(OCEAN_TEX_DIM) / std::log(2), OCEAN_TEX_DIM
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

                    {
                        const auto i_name = fmt::format("pingpong_xy_f#{}", i);
                        fd.pingpong_xy_ = rp_res.new_img(i_name, name());
                        MIRINAE_ASSERT(nullptr != fd.pingpong_xy_);

                        auto& img = fd.pingpong_xy_->img_;
                        img.init(cinfo.get(), device.mem_alloc());
                        builder.image(img.image());

                        fd.pingpong_xy_->view_.reset(builder, device);
                    }

                    {
                        const auto i_name = fmt::format("pingpong_z_f#{}", i);
                        fd.pingpong_z_ = rp_res.new_img(i_name, name());
                        MIRINAE_ASSERT(nullptr != fd.pingpong_z_);

                        auto& img = fd.pingpong_z_->img_;
                        img.init(cinfo.get(), device.mem_alloc());
                        builder.image(img.image());

                        fd.pingpong_z_->view_.reset(builder, device);
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
                        .add_storage_img(fd.hkt_dxdy_->view_.get())
                        .add_storage_img(fd.hkt_dz_->view_.get())
                        .add_img_sampler(
                            butterfly_cache_->view_.get(),
                            device.samplers().get_nearest()
                        );
                }
                builder.apply_all(device.logi_device());
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
                rp_res_.free_img(fdata.hkt_dxdy_->id(), this->name());
                rp_res_.free_img(fdata.hkt_dz_->id(), this->name());
                rp_res_.free_img(fdata.pingpong_xy_->id(), this->name());
                rp_res_.free_img(fdata.pingpong_z_->id(), this->name());
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

        void record(const mirinae::rp::ocean::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;
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
            for (int stage = 0; stage < 8; stage++) {
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
                    cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 1
                );

                pc.pingpong_ = !pc.pingpong_;
            }

            for (int stage = 0; stage < 8; stage++) {
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
                    cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 1
                );

                pc.pingpong_ = !pc.pingpong_;
            }
        }

    private:
        struct FrameData {
            mirinae::HRpImage hkt_dxdy_;
            mirinae::HRpImage hkt_dz_;
            mirinae::HRpImage pingpong_xy_;
            mirinae::HRpImage pingpong_z_;
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


// Ocean Finalize
namespace {

    class RpStatesOceanFinalize : public mirinae::rp::ocean::IRpStates {

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

                fd.hkt_dxdy_ = rp_res.get_img_reader(
                    fmt::format("ocean_tilde_hkt:hkt_dxdy_f#{}", i), name()
                );
                MIRINAE_ASSERT(nullptr != fd.hkt_dxdy_);
                fd.hkt_dz_ = rp_res.get_img_reader(
                    fmt::format("ocean_tilde_hkt:hkt_dz_f#{}", i), name()
                );
                MIRINAE_ASSERT(nullptr != fd.hkt_dz_);
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

                    const auto i_name = fmt::format("displacement_f#{}", i);
                    fd.displacement_ = rp_res.new_img(i_name, name());
                    MIRINAE_ASSERT(nullptr != fd.displacement_);

                    auto& img = fd.displacement_->img_;
                    img.init(cinfo.get(), device.mem_alloc());
                    builder.image(img.image());

                    fd.displacement_->view_.reset(builder, device);
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
                    barrier.image(fd.displacement_->img_.image());
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
                        .add_storage_img(fd.displacement_->view_.get())
                        .add_storage_img(fd.hkt_dxdy_->view_.get())
                        .add_storage_img(fd.hkt_dz_->view_.get());
                }
                builder.apply_all(device.logi_device());
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
                rp_res_.free_img(fdata.hkt_dxdy_->id(), this->name());
                rp_res_.free_img(fdata.hkt_dz_->id(), this->name());
                rp_res_.free_img(fdata.displacement_->id(), this->name());
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

        void record(const mirinae::rp::ocean::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;
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

            vkCmdDispatch(cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, 1);
        }

    private:
        struct FrameData {
            mirinae::HRpImage hkt_dxdy_;
            mirinae::HRpImage hkt_dz_;
            mirinae::HRpImage displacement_;
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

        U_OceanTessPushConst& height_map_size(uint32_t x, uint32_t y) {
            height_map_size_fbuf_size_.x = static_cast<float>(x);
            height_map_size_fbuf_size_.y = static_cast<float>(y);
            return *this;
        }

        U_OceanTessPushConst& fbuf_size(const VkExtent2D& x) {
            height_map_size_fbuf_size_.z = static_cast<float>(x.width);
            height_map_size_fbuf_size_.w = static_cast<float>(x.height);
            return *this;
        }

        U_OceanTessPushConst& height_scale(float x) {
            height_scale_ = x;
            return *this;
        }

    private:
        glm::mat4 pvm_;
        glm::mat4 view_;
        glm::mat4 model_;
        glm::vec4 tile_index_count_;
        glm::vec4 height_map_size_fbuf_size_;
        float height_scale_;
    };


    class RpStatesOceanTess : public mirinae::rp::ocean::IRpStates {

    public:
        RpStatesOceanTess(
            size_t swapchain_count,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            // Images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                const auto img_name = fmt::format(
                    "ocean_tilde_h:height_map_f#{}", i
                );
                auto img = rp_res.get_img_reader(img_name, this->name());
                MIRINAE_ASSERT(nullptr != img);
                height_maps_.push_back(img);
            }

            // Descriptor layout
            {
                mirinae::DescLayoutBuilder builder{ this->name() + ":main" };
                builder
                    .new_binding()  // Height map
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(1)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .add_stage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
                    .finish_binding();

                desclayouts.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                desc_pool_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desclayouts.get(name() + ":main").size_info(),
                    device.logi_device()
                );

                desc_sets_ = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desclayouts.get(name() + ":main").layout(),
                    device.logi_device()
                );

                mirinae::DescWriteInfoBuilder builder;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    builder.set_descset(desc_sets_[i])
                        .add_img_sampler_general(
                            height_maps_[i]->view_.get(),
                            device.samplers().get_linear()
                        );
                }
                builder.apply_all(device.logi_device());
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
            for (auto& img : height_maps_)
                rp_res_.free_img(img->id(), this->name());
            height_maps_.clear();

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

        void record(const mirinae::rp::ocean::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;

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

            const VkExtent2D fbuf_exd{ fbuf_width_, fbuf_height_ };
            mirinae::Viewport{ fbuf_exd }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_exd }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(pipe_layout_)
                .add(desc_sets_.at(ctxt.f_index_.get()))
                .record(cmdbuf);

            mirinae::PushConstInfo pc_info;
            pc_info.layout(pipe_layout_)
                .add_stage_vert()
                .add_stage_tesc()
                .add_stage_tese()
                .add_stage_frag();

            const auto model_mat = glm::translate(
                glm::dmat4{ 1 }, glm::dvec3{ -180, 0, -180 }
            );

            U_OceanTessPushConst pc;
            pc.pvm(ctxt.proj_mat_, ctxt.view_mat_, model_mat)
                .tile_count(10, 10)
                .height_map_size(OCEAN_TEX_DIM, OCEAN_TEX_DIM)
                .fbuf_size(fbuf_exd)
                .height_scale(1);

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
        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::vector<mirinae::HRpImage> height_maps_;
        std::vector<VkDescriptorSet> desc_sets_;
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
        mirinae::FbufImageBundle& fbuf_bundle,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesOceanTess>(
            swapchain_count, fbuf_bundle, rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp::ocean
