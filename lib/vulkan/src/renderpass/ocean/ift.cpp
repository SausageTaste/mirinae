#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/ocean/ocean.hpp"

#include <complex>

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/ocean/common.hpp"


namespace {

    void get_all_ref_img(
        const size_t frame_index,
        const std::string& name,
        std::vector<mirinae::HRpImage>& out,
        mirinae::RpResources& rp_res
    ) {
        const auto prefix = "ocean_tilde_hkt:";

        for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
            const auto suffix = fmt::format("_c{}_f{}", j, frame_index);

            out.push_back(rp_res.ren_img_.get_img_reader(
                fmt::format("{}hkt_1{}", prefix, suffix), name
            ));
            MIRINAE_ASSERT(nullptr != out.back());
            out.push_back(rp_res.ren_img_.get_img_reader(
                fmt::format("{}hkt_2{}", prefix, suffix), name
            ));
            MIRINAE_ASSERT(nullptr != out.back());
        }
    }

    void create_storage_img(
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
            cinfo.set_dimensions(mirinae::OCEAN_TEX_DIM)
                .set_format(VK_FORMAT_R32G32B32A32_SFLOAT)
                .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

            mirinae::ImageViewBuilder builder;
            builder.format(cinfo.format())
                .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

            for (size_t j = 0; j < num_img; j++) {
                const auto i_name = fmt::format("ppong_i{}_f{}", j, i);
                out.push_back(rp_res.ren_img_.new_img(i_name, name));
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


    template <typename T>
    constexpr T reverse_bits(T num) {
        static_assert(std::is_unsigned_v<T>);

        constexpr int NO_OF_BITS = sizeof(T) * 8;
        T reverse_num = 0;
        for (int i = 0; i < NO_OF_BITS; ++i) {
            if (num & (T(1) << i))
                reverse_num |= T(1) << ((NO_OF_BITS - 1) - i);
        }
        return reverse_num;
    }

    dal::TDataImage2D<float> create_butterfly_cache_tex(
        uint32_t width, uint32_t height
    ) {
        static_assert(sizeof(uint32_t) == sizeof(unsigned int));

        std::vector<int> bit_reversed_indices(height);
        const int bits = width;
        const int right_shift = sizeof(int) * 8 - bits;
        for (uint32_t i = 0; i < height; i++) {
            const auto x = ::reverse_bits(i);
            bit_reversed_indices[i] = (x << bits) | (x >> right_shift);
        }

        dal::TDataImage2D<float> out;
        out.init(nullptr, width, height, 4);

        const double N = height;

        for (size_t i_x = 0; i_x < width; ++i_x) {
            for (size_t i_y = 0; i_y < height; ++i_y) {
                const glm::dvec2 x(i_x, i_y);
                const auto k = std::fmod(x.y * (N / std::pow(2, x.x + 1)), N);
                const std::complex<double> twiddle(
                    std::cos(SUNG_TAU * k / N), std::sin(SUNG_TAU * k / N)
                );

                const int butterflyspan = int(std::pow(2.0, x.x));
                const auto a = std::fmod(x.y, std::pow(2.0, x.x + 1.0));
                const auto b = std::pow(2.0, x.x);
                const int butterfly_wing = (a < b) ? 1 : 0;

                if (x.x == 0) {
                    if (butterfly_wing == 1) {
                        auto texel = out.texel_ptr(i_x, i_y);
                        texel[0] = twiddle.real();
                        texel[1] = twiddle.imag();
                        texel[2] = bit_reversed_indices[i_y];
                        texel[3] = bit_reversed_indices[i_y + 1];
                    } else {
                        auto texel = out.texel_ptr(i_x, i_y);
                        texel[0] = twiddle.real();
                        texel[1] = twiddle.imag();
                        texel[2] = bit_reversed_indices[i_y - 1];
                        texel[3] = bit_reversed_indices[i_y];
                    }
                } else {
                    if (butterfly_wing == 1) {
                        auto texel = out.texel_ptr(i_x, i_y);
                        texel[0] = twiddle.real();
                        texel[1] = twiddle.imag();
                        texel[2] = x.y;
                        texel[3] = x.y + butterflyspan;
                    } else {
                        auto texel = out.texel_ptr(i_x, i_y);
                        texel[0] = twiddle.real();
                        texel[1] = twiddle.imag();
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

    struct U_OceanNaiveIftPushConst {
        int32_t N_;
        int32_t stage_;  // 0: hor, 1: ver
    };


    struct ButterFrameData {
        std::vector<mirinae::HRpImage> hkt_textures_;
        std::vector<mirinae::HRpImage> ppong_textures_;
        VkDescriptorSet desc_set_;
    };

    struct NaiveFrameData {
        std::vector<mirinae::HRpImage> hkt_textures_;
        std::vector<mirinae::HRpImage> ppong_textures_;
        VkDescriptorSet desc_set_;
    };

    using ButterFrameDataArr =
        std::array<ButterFrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;
    using NaiveFrameDataArr =
        std::array<NaiveFrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;

}  // namespace


// Tasks for butterfly
namespace { namespace task { namespace butterfly {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const entt::registry& reg,
            const mirinae::IPipelinePair& rp,
            const ::ButterFrameDataArr& frame_data,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            reg_ = &reg;
            rp_ = &rp;
            frame_data_ = &frame_data;
            cmd_pool_ = &cmd_pool;
            device_ = &device;
        }

        void prepare(const mirinae::RpCtxt& ctxt) { ctxt_ = &ctxt; }

        enki::ITaskSet& fence() { return fence_; }

        void collect_cmdbuf(std::vector<VkCommandBuffer>& out) {
            if (VK_NULL_HANDLE != cmdbuf_) {
                out.push_back(cmdbuf_);
            }
        }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            cmdbuf_ = VK_NULL_HANDLE;
            auto ocean = mirinae::find_ocean_cpnt(*reg_);
            if (!ocean)
                return;

            cmdbuf_ = cmd_pool_->get(ctxt_->f_index_, tid, *device_);
            if (cmdbuf_ == VK_NULL_HANDLE)
                return;

            mirinae::begin_cmdbuf(cmdbuf_);
            this->record(cmdbuf_, *rp_, *ctxt_, *frame_data_);
            mirinae::end_cmdbuf(cmdbuf_);
        }

        static bool record(
            const VkCommandBuffer cmdbuf,
            const mirinae::IPipelinePair& rp,
            const mirinae::RpCtxt& ctxt,
            const ::ButterFrameDataArr& frame_data
        ) {
            auto& fd = frame_data.at(ctxt.f_index_.get());

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, rp.pipeline()
            );

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
                .layout(rp.pipe_layout())
                .add(fd.desc_set_)
                .record(cmdbuf);

            mirinae::PushConstInfo pc_info;
            pc_info.layout(rp.pipe_layout())
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
            for (int stage = 0; stage < mirinae::OCEAN_TEX_DIM_LOG2; stage++) {
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
                    mirinae::OCEAN_TEX_DIM / 16,
                    mirinae::OCEAN_TEX_DIM / 16,
                    fd.ppong_textures_.size()
                );

                pc.pingpong_ = !pc.pingpong_;
            }

            for (int stage = 0; stage < mirinae::OCEAN_TEX_DIM_LOG2; stage++) {
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
                    mirinae::OCEAN_TEX_DIM / 16,
                    mirinae::OCEAN_TEX_DIM / 16,
                    fd.ppong_textures_.size()
                );

                pc.pingpong_ = !pc.pingpong_;
            }

            return true;
        }

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const ::ButterFrameDataArr* frame_data_ = nullptr;
        const entt::registry* reg_ = nullptr;
        const mirinae::IPipelinePair* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        RpTask() {}

        void init(
            const entt::registry& reg,
            const mirinae::IPipelinePair& rp,
            const ::ButterFrameDataArr& frame_data,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(reg, rp, frame_data, cmd_pool, device);
        }

        std::string_view name() const override { return "ocean h0"; }

        void prepare(const mirinae::RpCtxt& ctxt) override {
            record_tasks_.prepare(ctxt);
        }

        void collect_cmdbuf(std::vector<VkCommandBuffer>& out) override {
            record_tasks_.collect_cmdbuf(out);
        }

        enki::ITaskSet* record_task() override { return &record_tasks_; }

        enki::ITaskSet* record_fence() override {
            return &record_tasks_.fence();
        }

    private:
        DrawTasks record_tasks_;
    };

}}}  // namespace ::task::butterfly


// Ocean Butterfly
namespace {

    class RpStatesOceanButterfly
        : public mirinae::IRpBase
        , public mirinae::IPipelinePair {

    public:
        RpStatesOceanButterfly(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
            mirinae::CommandPool cmd_pool;
            cmd_pool.init(device);

            // Reference images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                ::get_all_ref_img(i, name_s(), fdata_[i].hkt_textures_, rp_res);
            }

            // Butterfly cache texture
            {
                const auto bufffly_img = ::create_butterfly_cache_tex(
                    mirinae::OCEAN_TEX_DIM_LOG2, mirinae::OCEAN_TEX_DIM
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

                butterfly_cache_ = rp_res.ren_img_.new_img(
                    "butterfly_cache", name_s()
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

                ::create_storage_img(
                    fd.hkt_textures_.size(),
                    i,
                    name_s(),
                    fd.ppong_textures_,
                    rp_res,
                    device
                );
            }

            // Desc layouts
            {
                MIRINAE_ASSERT(6 == fdata_[0].ppong_textures_.size());

                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
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
                rp_res.desclays_.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                auto& layout = rp_res.desclays_.get(name_s() + ":main");

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
                        .desc(rp_res.desclays_.get(name_s() + ":main").layout())
                        .build(device);
                MIRINAE_ASSERT(VK_NULL_HANDLE != pipe_layout_);

                pipeline_ = mirinae::create_compute_pipeline(
                    ":asset/spv/ocean_butterfly_comp.spv", pipe_layout_, device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanButterfly() override {
            for (auto& fdata : fdata_) {
                for (auto& ppong : fdata.ppong_textures_)
                    rp_res_.ren_img_.free_img(ppong->id(), name_s());
                for (auto& hkt : fdata.hkt_textures_)
                    rp_res_.ren_img_.free_img(hkt->id(), name_s());

                fdata.desc_set_ = VK_NULL_HANDLE;
            }

            rp_res_.ren_img_.free_img(butterfly_cache_->id(), name_s());
            desc_pool_.destroy(device_.logi_device());
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        std::string_view name() const override { return "ocean_butterfly"; }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<task::butterfly::RpTask>();
            out->init(cosmos_.reg(), *this, fdata_, rp_res_.cmd_pool_, device_);
            return out;
        }

        VkPipeline pipeline() const override { return pipeline_; }
        VkPipelineLayout pipe_layout() const override { return pipe_layout_; }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        ::ButterFrameDataArr fdata_;
        mirinae::HRpImage butterfly_cache_;
        mirinae::DescPool desc_pool_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
    };

}  // namespace


// Ocean Naive IFT
namespace {

    class RpStatesOceanNaiveIft : public mirinae::IRpStates {

    public:
        RpStatesOceanNaiveIft(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
            mirinae::CommandPool cmd_pool;
            cmd_pool.init(device);

            // Reference images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                ::get_all_ref_img(
                    i, this->name(), fdata_[i].hkt_textures_, rp_res
                );
            }

            // Storage images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = fdata_[i];

                ::create_storage_img(
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
                rp_res.desclays_.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                auto& layout = rp_res.desclays_.get(this->name() + ":main");

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
                    .desc(rp_res.desclays_.get(this->name() + ":main").layout())
                    .build(pipe_layout_, device);

                pipeline_ = mirinae::create_compute_pipeline(
                    ":asset/spv/ocean_naive_ift_comp.spv", pipe_layout_, device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanNaiveIft() override {
            for (auto& fdata : fdata_) {
                for (auto& ppong : fdata.ppong_textures_)
                    rp_res_.ren_img_.free_img(ppong->id(), this->name());
                for (auto& hkt : fdata.hkt_textures_)
                    rp_res_.ren_img_.free_img(hkt->id(), this->name());

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
            auto ocean = mirinae::find_ocean_cpnt(cosmos_.reg());
            if (!ocean)
                return;
            auto& ocean_entt = *ocean;
            auto& fd = fdata_[ctxt.f_index_.get()];
            auto cmdbuf = ctxt.cmdbuf_;

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
            pc.N_ = ::mirinae::OCEAN_TEX_DIM;
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
                mirinae::OCEAN_TEX_DIM / 16,
                mirinae::OCEAN_TEX_DIM / 16,
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
                mirinae::OCEAN_TEX_DIM / 16,
                mirinae::OCEAN_TEX_DIM / 16,
                fd.ppong_textures_.size()
            );
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        ::NaiveFrameDataArr fdata_;
        mirinae::DescPool desc_pool_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<mirinae::IRpBase> create_rp_ocean_butterfly(
        RpCreateBundle& bundle
    ) {
        return std::make_unique<RpStatesOceanButterfly>(
            bundle.cosmos_, bundle.rp_res_, bundle.device_
        );
    }

    URpStates create_rp_ocean_naive_ift(RpCreateBundle& bundle) {
        return std::make_unique<RpStatesOceanNaiveIft>(
            bundle.cosmos_, bundle.rp_res_, bundle.device_
        );
    }

}  // namespace mirinae::rp
