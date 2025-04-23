#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/ocean.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"


#define GET_OCEAN_ENTT(ctxt)                        \
    if (!(ctxt).draw_sheet_)                        \
        return;                                     \
    if (!(ctxt).draw_sheet_->ocean_)                \
        return;                                     \
    auto& ocean_entt = *(ctxt).draw_sheet_->ocean_; \
    auto cmdbuf = (ctxt).cmdbuf_;


namespace {

    constexpr uint32_t CASCADE_COUNT = mirinae::cpnt::OCEAN_CASCADE_COUNT;
    constexpr uint32_t OCEAN_TEX_DIM = 256;
    const uint32_t OCEAN_TEX_DIM_LOG2 = std::log(OCEAN_TEX_DIM) / std::log(2);


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
                this->get_all_ref_img(
                    i, this->name(), fdata_[i].hkt_textures_, rp_res
                );
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
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = fdata_[i];

                this->create_storage_img(
                    fd.hkt_textures_.size(),
                    i,
                    this->name(),
                    fd.ppong_textures_,
                    rp_res,
                    device
                );
            }

            // Desc layouts
            {
                MIRINAE_ASSERT(6 == fdata_[0].ppong_textures_.size());

                mirinae::DescLayoutBuilder builder{ name() + ":main" };
                builder
                    .new_binding(0)  // Pingpong images
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(6)
                    .finish_binding()
                    .new_binding(1)  // hkt images
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(6)
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
                pipe_layout_ =
                    mirinae::PipelineLayoutBuilder{}
                        .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                        .pc<U_OceanButterflyPushConst>()
                        .desc(desclayouts.get(name() + ":main").layout())
                        .build(device);
                MIRINAE_ASSERT(VK_NULL_HANDLE != pipe_layout_);

                pipeline_ = ::create_compute_pipeline(
                    ":asset/spv/ocean_butterfly_comp.spv", pipe_layout_, device
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
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
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
                .layout(pipe_layout_)
                .add(fd.desc_set_)
                .record(cmdbuf);

            mirinae::PushConstInfo pc_info;
            pc_info.layout(pipe_layout_).add_stage(VK_SHADER_STAGE_COMPUTE_BIT);

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
                    cmdbuf,
                    OCEAN_TEX_DIM / 16,
                    OCEAN_TEX_DIM / 16,
                    fd.ppong_textures_.size()
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
                    cmdbuf,
                    OCEAN_TEX_DIM / 16,
                    OCEAN_TEX_DIM / 16,
                    fd.ppong_textures_.size()
                );

                pc.pingpong_ = !pc.pingpong_;
            }
        }

        static void get_all_ref_img(
            const size_t frame_index,
            const std::string& name,
            std::vector<mirinae::HRpImage>& out,
            mirinae::RpResources& rp_res
        ) {
            const auto prefix = "ocean_tilde_hkt:";

            for (size_t j = 0; j < CASCADE_COUNT; j++) {
                const auto suffix = fmt::format("_c{}_f{}", j, frame_index);

                out.push_back(rp_res.get_img_reader(
                    fmt::format("{}hkt_1{}", prefix, suffix), name
                ));
                MIRINAE_ASSERT(nullptr != out.back());
                out.push_back(rp_res.get_img_reader(
                    fmt::format("{}hkt_2{}", prefix, suffix), name
                ));
                MIRINAE_ASSERT(nullptr != out.back());
            }
        }

        static void create_storage_img(
            const size_t num_img,
            const size_t frame_index,
            const std::string& name,
            std::vector<mirinae::HRpImage>& out,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        ) {
            const auto i = frame_index;

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

                for (size_t j = 0; j < num_img; j++) {
                    const auto i_name = fmt::format("ppong_i{}_f{}", j, i);
                    out.push_back(rp_res.new_img(i_name, name));
                    MIRINAE_ASSERT(nullptr != out.back());

                    auto& img = out.back()->img_;
                    img.init(cinfo.get(), device.mem_alloc());
                    builder.image(img.image());

                    out.back()->view_.reset(builder, device);
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
                for (auto& ppong : out) {
                    barrier.image(ppong->img_.image());
                    barrier.record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT
                    );
                }
                cmd_pool.end_single_time(cmdbuf, device);
                cmd_pool.destroy(device.logi_device());
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
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
    };

}  // namespace


// Ocean Naive IFT
namespace {

    struct U_OceanNaiveIftPushConst {
        int32_t N_;
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
                RpStatesOceanButterfly::get_all_ref_img(
                    i, this->name(), fdata_[i].hkt_textures_, rp_res
                );
            }

            // Storage images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = fdata_[i];

                RpStatesOceanButterfly::create_storage_img(
                    fd.hkt_textures_.size(),
                    i,
                    this->name(),
                    fd.ppong_textures_,
                    rp_res,
                    device
                );
            }

            // Desc layouts
            {
                MIRINAE_ASSERT(6 == fdata_[0].ppong_textures_.size());

                mirinae::DescLayoutBuilder builder{ name() + ":main" };
                builder
                    .new_binding(0)  // Pingpong images
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(6)
                    .finish_binding()
                    .new_binding(1)  // hkt images
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(6)
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

                    // Pingpong images
                    for (auto& ppong : fd.ppong_textures_)
                        writer.add_storage_img_info(ppong->view_.get());
                    writer.add_storage_img_write(fd.desc_set_, 0);
                    // hkt images
                    for (auto& ppong : fd.hkt_textures_)
                        writer.add_storage_img_info(ppong->view_.get());
                    writer.add_storage_img_write(fd.desc_set_, 1);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline
            {
                mirinae::PipelineLayoutBuilder{}
                    .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                    .pc<U_OceanNaiveIftPushConst>()
                    .desc(desclayouts.get(name() + ":main").layout())
                    .build(pipe_layout_, device);

                pipeline_ = ::create_compute_pipeline(
                    ":asset/spv/ocean_naive_ift_comp.spv", pipe_layout_, device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanNaiveIft() override {
            for (auto& fdata : fdata_) {
                for (auto& ppong : fdata.ppong_textures_)
                    rp_res_.free_img(ppong->id(), this->name());
                for (auto& hkt : fdata.hkt_textures_)
                    rp_res_.free_img(hkt->id(), this->name());

                fdata.desc_set_ = VK_NULL_HANDLE;
            }

            desc_pool_.destroy(device_.logi_device());
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
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
                .layout(pipe_layout_)
                .add(fd.desc_set_)
                .record(cmdbuf);

            ::U_OceanNaiveIftPushConst pc;
            pc.N_ = ::OCEAN_TEX_DIM;
            pc.stage_ = 0;

            mirinae::PushConstInfo pc_info;
            pc_info.layout(pipe_layout_)
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

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

            vkCmdDispatch(
                cmdbuf,
                OCEAN_TEX_DIM / 16,
                OCEAN_TEX_DIM / 16,
                fd.ppong_textures_.size()
            );

            pc.stage_ = 1;
            pc_info.record(cmdbuf, pc);

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

            vkCmdDispatch(
                cmdbuf,
                OCEAN_TEX_DIM / 16,
                OCEAN_TEX_DIM / 16,
                fd.ppong_textures_.size()
            );
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
        mirinae::DescPool desc_pool_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
    };

}  // namespace


namespace mirinae::rp::ocean {

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

}  // namespace mirinae::rp::ocean
