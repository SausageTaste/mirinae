#include "mirinae/render/renderpass/ocean.hpp"

#include <entt/entity/registry.hpp>
#include <sung/basic/aabb.hpp>
#include <sung/basic/cvar.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
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

    sung::AutoCVarFlt cv_foam_add{ "ocean:foam_add", "", 1 };
    sung::AutoCVarFlt cv_foam_sss_base{ "ocean:foam_sss_base", "", 0 };
    sung::AutoCVarFlt cv_foam_sss_scale{ "ocean:foam_sss_scale", "", 4 };
    sung::AutoCVarFlt cv_foam_threshold{ "ocean:foam_threshold", "", 8.5 };
    sung::AutoCVarFlt cv_displace_scale_x{ "ocean:displace_scale_x", "", 1 };
    sung::AutoCVarFlt cv_displace_scale_y{ "ocean:displace_scale_y", "", 1 };


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

    constexpr uint32_t CASCADE_COUNT = mirinae::cpnt::OCEAN_CASCADE_COUNT;
    constexpr uint32_t OCEAN_TEX_DIM = 256;
    const uint32_t OCEAN_TEX_DIM_LOG2 = std::log(OCEAN_TEX_DIM) / std::log(2);

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

            // Pipeline
            {
                mirinae::PipelineLayoutBuilder{}
                    .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                    .pc<U_OceanTildeHPushConst>()
                    .desc(desclayouts.get(name() + ":main").layout())
                    .build(pipe_layout_, device);

                pipeline_ = ::create_compute_pipeline(
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
            pc.N_ = ::OCEAN_TEX_DIM;

            for (int i = 0; i < CASCADE_COUNT; ++i) {
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
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        sung::MonotonicRealtimeTimer timer_;
    };

}  // namespace


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
                            "hkt_1_c{}_f{}", j, i
                        );
                        auto img = rp_res.new_img(img_name, this->name());
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());
                        img->view_.reset(builder, device);
                        fd.hkt_1_[j] = img;
                    }

                    for (size_t j = 0; j < CASCADE_COUNT; ++j) {
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
                    for (size_t i = 0; i < CASCADE_COUNT; ++i) {
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

                pipeline_ = ::create_compute_pipeline(
                    ":asset/spv/ocean_tilde_hkt_comp.spv", pipe_layout_, device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanTildeHkt() override {
            for (auto& fd : frame_data_) {
                for (size_t i = 0; i < CASCADE_COUNT; ++i) {
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
            for (size_t i = 0; i < CASCADE_COUNT; ++i) {
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
            for (size_t i = 0; i < CASCADE_COUNT; ++i) {
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
            pc.N_ = ::OCEAN_TEX_DIM;

            mirinae::PushConstInfo pc_info;
            pc_info.layout(pipe_layout_)
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

            vkCmdDispatch(
                cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, CASCADE_COUNT
            );
        }

        static const std::string& name_static() {
            static const std::string name = "ocean_tilde_hkt";
            return name;
        }

    private:
        struct FrameData {
            std::array<mirinae::HRpImage, CASCADE_COUNT> hk_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> hkt_1_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> hkt_2_;
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
            const auto& src_name = RpStatesOceanTildeHkt::name_static();
            const auto prefix = fmt::format("{}:", src_name);

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

                for (size_t j = 0; j < CASCADE_COUNT; j++) {
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

                        fd.disp_[j] = std::move(img);
                    }

                    for (size_t j = 0; j < CASCADE_COUNT; j++) {
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

                for (size_t j = 0; j < CASCADE_COUNT; j++) {
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
                    for (size_t i = 0; i < CASCADE_COUNT; i++) {
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
                for (size_t i = 0; i < CASCADE_COUNT; i++) {
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

                pipeline_ = ::create_compute_pipeline(
                    ":asset/spv/ocean_finalize_comp.spv", pipe_layout_, device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanFinalize() override {
            for (auto& fdata : fdata_) {
                for (size_t i = 0; i < CASCADE_COUNT; i++) {
                    rp_res_.free_img(fdata.hkt_1_[i]->id(), this->name());
                    rp_res_.free_img(fdata.hkt_2_[i]->id(), this->name());
                    rp_res_.free_img(fdata.disp_[i]->id(), this->name());
                    rp_res_.free_img(fdata.deri_[i]->id(), this->name());
                }

                fdata.desc_set_ = VK_NULL_HANDLE;
            }

            for (size_t i = 0; i < CASCADE_COUNT; i++)
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
            pc.N_ = ::OCEAN_TEX_DIM;

            mirinae::PushConstInfo{}
                .layout(pipe_layout_)
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

            vkCmdDispatch(
                cmdbuf, OCEAN_TEX_DIM / 16, OCEAN_TEX_DIM / 16, CASCADE_COUNT
            );
        }

    private:
        struct FrameData {
            std::array<mirinae::HRpImage, CASCADE_COUNT> hkt_1_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> hkt_2_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> disp_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> deri_;
            VkDescriptorSet desc_set_;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> fdata_;
        std::array<mirinae::HRpImage, CASCADE_COUNT> turb_;
        mirinae::DescPool desc_pool_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
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

        template <typename T>
        U_OceanTessPushConst& patch_offset(T x, T y) {
            patch_offset_scale_.x = static_cast<float>(x);
            patch_offset_scale_.y = static_cast<float>(y);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& patch_scale(T x, T y) {
            patch_offset_scale_.z = static_cast<float>(x);
            patch_offset_scale_.w = static_cast<float>(y);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& tile_dimensions(T x, T y) {
            tile_dims_n_fbuf_size_.x = static_cast<float>(x);
            tile_dims_n_fbuf_size_.y = static_cast<float>(y);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& tile_dimensions(T x) {
            tile_dims_n_fbuf_size_.x = static_cast<float>(x);
            tile_dims_n_fbuf_size_.y = static_cast<float>(x);
            return *this;
        }

        U_OceanTessPushConst& fbuf_size(const VkExtent2D& x) {
            tile_dims_n_fbuf_size_.z = static_cast<float>(x.width);
            tile_dims_n_fbuf_size_.w = static_cast<float>(x.height);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& tile_index(T x, T y) {
            tile_index_count_.x = static_cast<float>(x);
            tile_index_count_.y = static_cast<float>(y);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& tile_count(T x, T y) {
            tile_index_count_.z = static_cast<float>(x);
            tile_index_count_.w = static_cast<float>(y);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& patch_height(T x) {
            patch_height_ = static_cast<float>(x);
            return *this;
        }

    private:
        glm::mat4 pvm_;
        glm::mat4 view_;
        glm::mat4 model_;
        glm::vec4 patch_offset_scale_;
        glm::vec4 tile_dims_n_fbuf_size_;
        glm::vec4 tile_index_count_;
        float patch_height_;
    };

    static_assert(sizeof(U_OceanTessPushConst) < 256, "");


    struct U_OceanTessParams {

    public:
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

        U_OceanTessParams& dlight_color(const glm::vec3& dir) {
            dlight_color_.x = dir.x;
            dlight_color_.y = dir.y;
            dlight_color_.z = dir.z;
            return *this;
        }

        U_OceanTessParams& dlight_dir(const glm::vec3& dir) {
            dlight_dir_.x = dir.x;
            dlight_dir_.y = dir.y;
            dlight_dir_.z = dir.z;
            return *this;
        }

        U_OceanTessParams& fog_color(const glm::vec3& color) {
            fog_color_density_.x = color.r;
            fog_color_density_.y = color.g;
            fog_color_density_.z = color.b;
            return *this;
        }

        U_OceanTessParams& fog_density(float density) {
            fog_color_density_.w = density;
            return *this;
        }

        template <typename T>
        U_OceanTessParams& jacobian_scale(size_t idx, T x) {
            jacobian_scale_[idx] = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& len_scale(size_t idx, T x) {
            len_scales_lod_scale_[idx] = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& len_scales(T x, T y, T z) {
            len_scales_lod_scale_.x = static_cast<float>(x);
            len_scales_lod_scale_.y = static_cast<float>(y);
            len_scales_lod_scale_.z = static_cast<float>(z);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& lod_scale(T x) {
            len_scales_lod_scale_.w = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& ocean_color(const glm::tvec3<T>& color) {
            ocean_color_.x = static_cast<float>(color.x);
            ocean_color_.y = static_cast<float>(color.y);
            ocean_color_.z = static_cast<float>(color.z);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& foam_bias(T x) {
            foam_bias_ = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& foam_scale(T x) {
            foam_scale_ = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& foam_threshold(T x) {
            foam_threshold_ = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& sss_base(T x) {
            sss_base_ = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& sss_scale(T x) {
            sss_scale_ = static_cast<float>(x);
            return *this;
        }

    private:
        glm::vec4 texco_offset_rot_[CASCADE_COUNT];
        glm::vec4 dlight_color_;
        glm::vec4 dlight_dir_;
        glm::vec4 fog_color_density_;
        glm::vec4 jacobian_scale_;
        glm::vec4 len_scales_lod_scale_;
        glm::vec4 ocean_color_;
        float foam_bias_;
        float foam_scale_;
        float foam_threshold_;
        float sss_base_;
        float sss_scale_;
    };


    class RpStatesOceanTess : public mirinae::IRpStates {

    public:
        RpStatesOceanTess(
            size_t swapchain_count,
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            // Sky texture
            {
                auto& reg = cosmos.reg();
                auto& tex = *rp_res.tex_man_;
                for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                    auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                    if (tex.block_for_tex(atmos.sky_tex_path_, false)) {
                        sky_tex_ = tex.get(atmos.sky_tex_path_);
                        break;
                    }
                }
                if (!sky_tex_)
                    sky_tex_ = tex.missing_tex();
            }

            // Images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_[i];

                for (size_t j = 0; j < CASCADE_COUNT; j++) {
                    const auto img_name = fmt::format(
                        "ocean_finalize:displacement_c{}_f{}", j, i
                    );
                    fd.disp_map_[j] = rp_res.get_img_reader(
                        img_name, this->name()
                    );
                    MIRINAE_ASSERT(nullptr != fd.disp_map_[j]);
                }

                for (size_t j = 0; j < CASCADE_COUNT; j++) {
                    const auto img_name = fmt::format(
                        "ocean_finalize:derivatives_c{}_f{}", j, i
                    );
                    fd.deri_map_[j] = rp_res.get_img_reader(
                        img_name, this->name()
                    );
                    MIRINAE_ASSERT(nullptr != fd.deri_map_[j]);
                }

                for (size_t j = 0; j < CASCADE_COUNT; j++) {
                    const auto img_name = fmt::format(
                        "ocean_finalize:turbulence_c{}", j
                    );
                    turb_map_[j] = rp_res.get_img_reader(
                        img_name, this->name()
                    );
                    MIRINAE_ASSERT(nullptr != turb_map_[j]);
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
                    .new_binding()  // Displacement map
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(3)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .add_stage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
                    .finish_binding();
                builder
                    .new_binding()  // Derivative map
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(3)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .finish_binding();
                builder
                    .new_binding()  // Turbulance map
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(3)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .finish_binding();
                builder  // Sky texture
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
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
                        .set_img_view(fd.disp_map_[0]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.disp_map_[1]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.disp_map_[2]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_sampled_img_write(fd.desc_set_, 1);
                    writer  // Normal maps
                        .add_img_info()
                        .set_img_view(fd.deri_map_[0]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.deri_map_[1]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.deri_map_[2]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_sampled_img_write(fd.desc_set_, 2);
                    writer  // Turbulance maps
                        .add_img_info()
                        .set_img_view(turb_map_[0]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(turb_map_[1]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(turb_map_[2]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_sampled_img_write(fd.desc_set_, 3);

                    writer.add_img_info()
                        .set_img_view(sky_tex_->image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 4);
                }
                writer.apply_all(device.logi_device());
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(rp_res.gbuf_.compo_format())
                    .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();
                builder.attach_desc()
                    .add(rp_res.gbuf_.depth_format())
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
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclayouts.get(name() + ":main").layout())
                    .add_vertex_flag()
                    .add_tesc_flag()
                    .add_tese_flag()
                    .add_frag_flag()
                    .pc<U_OceanTessPushConst>(0)
                    .build(pipe_layout_, device);
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
                // builder.rasterization_state().polygon_mode_line();

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
                fbuf_width_ = rp_res.gbuf_.width();
                fbuf_height_ = rp_res.gbuf_.height();

                for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    mirinae::FbufCinfo cinfo;
                    cinfo.set_rp(render_pass_)
                        .add_attach(rp_res.gbuf_.compo(i).image_view())
                        .add_attach(rp_res.gbuf_.depth(i).image_view())
                        .set_dim(fbuf_width_, fbuf_height_);
                    fbufs_.push_back(cinfo.build(device));
                }
            }

            // Misc
            {
                clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };
                clear_values_.at(1).depthStencil = { 0, 0 };
            }

            return;
        }

        ~RpStatesOceanTess() override {
            for (auto& fd : frame_data_) {
                for (size_t i = 0; i < CASCADE_COUNT; i++) {
                    rp_res_.free_img(fd.disp_map_[i]->id(), this->name());
                    rp_res_.free_img(fd.deri_map_[i]->id(), this->name());
                }

                fd.ubuf_.destroy(device_.mem_alloc());
                fd.desc_set_ = VK_NULL_HANDLE;
            }

            for (size_t i = 0; i < CASCADE_COUNT; i++)
                rp_res_.free_img(turb_map_[i]->id(), this->name());

            desc_pool_.destroy(device_.logi_device());
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        void record(mirinae::RpContext& ctxt) override {
            GET_OCEAN_ENTT(ctxt);
            auto& fd = frame_data_[ctxt.f_index_.get()];
            const VkExtent2D fbuf_exd{ fbuf_width_, fbuf_height_ };

            U_OceanTessParams ubuf;
            ubuf.foam_bias(ocean_entt.foam_bias_)
                .foam_scale(ocean_entt.foam_scale_)
                .foam_threshold(cv_foam_threshold.get())
                .jacobian_scale(0, ocean_entt.cascades_[0].jacobian_scale_)
                .jacobian_scale(1, ocean_entt.cascades_[1].jacobian_scale_)
                .jacobian_scale(2, ocean_entt.cascades_[2].jacobian_scale_)
                .len_scale(0, ocean_entt.cascades_[0].lod_scale_)
                .len_scale(1, ocean_entt.cascades_[1].lod_scale_)
                .len_scale(2, ocean_entt.cascades_[2].lod_scale_)
                .lod_scale(ocean_entt.lod_scale_)
                .ocean_color(ocean_entt.ocean_color_)
                .sss_base(cv_foam_sss_base.get())
                .sss_scale(cv_foam_sss_scale.get());
            for (size_t i = 0; i < CASCADE_COUNT; i++)
                ubuf.texco_offset(i, ocean_entt.cascades_[i].texco_offset_)
                    .texco_scale(i, ocean_entt.cascades_[i].texco_scale_);
            if (auto& atmos = ctxt.draw_sheet_->atmosphere_)
                ubuf.fog_color(atmos->fog_color_)
                    .fog_density(atmos->fog_density_);

            namespace cpnt = mirinae::cpnt;
            auto& reg = ctxt.cosmos_->reg();
            for (auto e : reg.view<cpnt::DLight, cpnt::Transform>()) {
                auto& light = reg.get<cpnt::DLight>(e);
                auto& tform = reg.get<cpnt::Transform>(e);
                ubuf.dlight_color(light.color_.scaled_color());
                ubuf.dlight_dir(light.calc_to_light_dir(ctxt.view_mat_, tform));
                break;
            }

            fd.ubuf_.set_data(ubuf, device_.mem_alloc());

            mirinae::RenderPassBeginInfo{}
                .rp(render_pass_)
                .fbuf(fbufs_.at(ctxt.f_index_.get()))
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
            pc.fbuf_size(fbuf_exd)
                .tile_count(ocean_entt.tile_count_x_, ocean_entt.tile_count_y_)
                .tile_dimensions(ocean_entt.tile_size_)
                .pvm(ctxt.proj_mat_, ctxt.view_mat_, glm::dmat4(1))
                .patch_height(ocean_entt.height_);

            const auto z_far = ctxt.proj_mat_[3][2] / ctxt.proj_mat_[2][2];
            const auto scale = z_far * 1.25;
            const auto pv = ctxt.proj_mat_ * ctxt.view_mat_;
            const auto cam_x = std::round(ctxt.view_pos_.x * 0.1) * 10;
            const auto cam_z = std::round(ctxt.view_pos_.z * 0.1) * 10;
            this->traverse_quad_tree<double>(
                0,
                cam_x - scale,
                cam_x + scale,
                cam_z - scale,
                cam_z + scale,
                ocean_entt.height_,
                ctxt,
                pc,
                pc_info,
                pv
            );

            vkCmdEndRenderPass(cmdbuf);
        }

        const std::string& name() const override {
            static const std::string name = "ocean_tess";
            return name;
        }

    private:
        struct FrameData {
            std::array<mirinae::HRpImage, CASCADE_COUNT> disp_map_;
            std::array<mirinae::HRpImage, CASCADE_COUNT> deri_map_;
            mirinae::Buffer ubuf_;
            VkDescriptorSet desc_set_;
        };

        template <typename T>
        static bool has_separating_axis(
            const mirinae::ViewFrustum& view_frustum,
            const std::array<glm::tvec3<T>, 4>& points
        ) {
            const auto to_patch = glm::tmat4x4<T>(view_frustum.view_inv_);
            const auto to_patch3 = glm::tmat3x3<T>(to_patch);

            MIRINAE_ASSERT(view_frustum.vtx_.size() == 8);
            std::array<glm::tvec3<T>, 8> frustum_points;
            for (size_t i = 0; i < 8; ++i) {
                const auto& p = glm::tvec4<T>(view_frustum.vtx_[i], 1);
                frustum_points[i] = glm::tvec3<T>(to_patch * p);
            }

            std::vector<glm::tvec3<T>> axes;
            axes.reserve(view_frustum.axes_.size() + 3);
            for (auto& v : view_frustum.axes_) {
                axes.push_back(to_patch3 * v);
            }
            axes.push_back(glm::tvec3<T>(1, 0, 0));
            axes.push_back(glm::tvec3<T>(0, 1, 0));
            axes.push_back(glm::tvec3<T>(0, 0, 1));

            for (auto& axis : axes) {
                sung::AABB1<T> frustum_aabb;
                for (auto& p : frustum_points)
                    frustum_aabb.set_or_expand(glm::dot(p, axis));

                sung::AABB1<T> points_aabb;
                for (auto& p : points)
                    points_aabb.set_or_expand(glm::dot(p, axis));

                if (!frustum_aabb.is_intersecting_cl(points_aabb))
                    return true;
            }

            return false;
        }

        template <typename T>
        void traverse_quad_tree(
            const int depth,
            const T x_min,
            const T x_max,
            const T y_min,
            const T y_max,
            const T height,
            mirinae::RpContext& ctxt,
            U_OceanTessPushConst& pc,
            const mirinae::PushConstInfo& pc_info,
            const glm::tmat4x4<T>& pv
        ) {
            using Vec2 = glm::tvec2<T>;
            using Vec3 = glm::tvec3<T>;
            using Vec4 = glm::tvec4<T>;

            constexpr T HALF = 0.5;
            constexpr T MARGIN = 1;

            const auto x_margin = MARGIN;
            const auto y_margin = MARGIN;
            const std::array<Vec3, 4> points{
                Vec3(x_min - x_margin, height, y_min - y_margin),
                Vec3(x_min - x_margin, height, y_max + y_margin),
                Vec3(x_max + x_margin, height, y_max + y_margin),
                Vec3(x_max + x_margin, height, y_min - y_margin),
            };

            // Check frustum
            if (this->has_separating_axis<T>(ctxt.view_frustum_, points)) {
                /*
                auto& dbg = ctxt.debug_ren_;
                dbg.add_tri(
                    pv * Vec4(points[0], 1),
                    pv * Vec4(points[1], 1),
                    pv * Vec4(points[2], 1),
                    glm::vec4(1, 0, 0, 0.1)
                );
                dbg.add_tri(
                    pv * Vec4(points[0], 1),
                    pv * Vec4(points[2], 1),
                    pv * Vec4(points[3], 1),
                    glm::vec4(1, 0, 0, 0.1)
                );
                */
                return;
            }

            std::array<Vec3, 4> ndc_points;
            for (size_t i = 0; i < 4; ++i) {
                auto ndc4 = pv * Vec4(points[i], 1);
                ndc4 /= ndc4.w;
                ndc_points[i] = Vec3(ndc4);
            }

            if (depth > 8) {
                pc.patch_offset(x_min - x_margin, y_min - y_margin)
                    .patch_scale(
                        x_max - x_min + x_margin + x_margin,
                        y_max - y_min + y_margin + y_margin
                    );
                pc_info.record(ctxt.cmdbuf_, pc);
                vkCmdDraw(ctxt.cmdbuf_, 4, 1, 0, 0);
                return;
            }

            T longest_edge = 0;
            const Vec2 fbuf_size(fbuf_width_, fbuf_height_);
            for (size_t i = 0; i < 4; ++i) {
                const auto next_idx = (i + 1) % ndc_points.size();
                const auto& p0 = Vec2(ndc_points[i]) * HALF + HALF;
                const auto& p1 = Vec2(ndc_points[next_idx]) * HALF + HALF;
                const auto edge = (p1 - p0) * fbuf_size;
                const auto len = glm::length(edge);
                longest_edge = (std::max<T>)(longest_edge, len);
            }
            for (size_t i = 0; i < 2; ++i) {
                const auto next_idx = (i + 2) % ndc_points.size();
                const auto& p0 = Vec2(ndc_points[i]) * HALF + HALF;
                const auto& p1 = Vec2(ndc_points[next_idx]) * HALF + HALF;
                const auto edge = (p1 - p0) * fbuf_size;
                const auto len = glm::length(edge);
                longest_edge = (std::max<T>)(longest_edge, len);
            }

            if (glm::length(longest_edge) > 1000) {
                const auto x_mid = (x_min + x_max) * 0.5;
                const auto y_mid = (y_min + y_max) * 0.5;
                this->traverse_quad_tree<T>(
                    depth + 1,
                    x_min,
                    x_mid,
                    y_min,
                    y_mid,
                    height,
                    ctxt,
                    pc,
                    pc_info,
                    pv
                );
                this->traverse_quad_tree<T>(
                    depth + 1,
                    x_min,
                    x_mid,
                    y_mid,
                    y_max,
                    height,
                    ctxt,
                    pc,
                    pc_info,
                    pv
                );
                this->traverse_quad_tree<T>(
                    depth + 1,
                    x_mid,
                    x_max,
                    y_mid,
                    y_max,
                    height,
                    ctxt,
                    pc,
                    pc_info,
                    pv
                );
                this->traverse_quad_tree<T>(
                    depth + 1,
                    x_mid,
                    x_max,
                    y_min,
                    y_mid,
                    height,
                    ctxt,
                    pc,
                    pc_info,
                    pv
                );
            } else {
                pc.patch_offset(x_min - x_margin, y_min - y_margin)
                    .patch_scale(
                        x_max - x_min + x_margin + x_margin,
                        y_max - y_min + y_margin + y_margin
                    );
                pc_info.record(ctxt.cmdbuf_, pc);
                vkCmdDraw(ctxt.cmdbuf_, 4, 1, 0, 0);
                return;
            }
        }

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;
        std::array<mirinae::HRpImage, CASCADE_COUNT> turb_map_;
        std::shared_ptr<mirinae::ITexture> sky_tex_;
        mirinae::DescPool desc_pool_;
        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

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
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesOceanTess>(
            swapchain_count, cosmos, rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp::ocean
