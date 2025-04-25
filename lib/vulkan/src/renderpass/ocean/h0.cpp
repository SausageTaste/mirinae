#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/ocean/ocean.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/ocean/common.hpp"


namespace {

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


    struct FrameData {
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hk_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;


    // Range is [0, 1]
    double rand_uniform() { return (double)rand() / RAND_MAX; }

}  // namespace


// Tasks
namespace { namespace task {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const entt::registry& reg,
            const mirinae::IPipelinePair& rp,
            const ::FrameDataArr& frame_data,
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
            cmdbuf_ = cmd_pool_->get(ctxt_->f_index_, tid, *device_);
            if (cmdbuf_ == VK_NULL_HANDLE)
                return;

            mirinae::begin_cmdbuf(cmdbuf_);
            this->record(cmdbuf_, *rp_, *ctxt_, *reg_, *frame_data_);
            mirinae::end_cmdbuf(cmdbuf_);
        }

        static bool record(
            const VkCommandBuffer cmdbuf,
            const mirinae::IPipelinePair& rp,
            const mirinae::RpCtxt& ctxt,
            const entt::registry& reg,
            const ::FrameDataArr& frame_data
        ) {
            auto ocean = mirinae::find_ocean_cpnt(reg);
            if (!ocean)
                return false;

            auto& ocean_entt = *ocean;
            auto& fd = frame_data.at(ctxt.f_index_.get());

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, rp.pipeline()
            );

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
                .layout(rp.pipe_layout())
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
                pc_info.layout(rp.pipe_layout())
                    .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .record(cmdbuf, pc);

                vkCmdDispatch(
                    cmdbuf,
                    mirinae::OCEAN_TEX_DIM / 16,
                    mirinae::OCEAN_TEX_DIM / 16,
                    1
                );
            }

            return true;
        }

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const ::FrameDataArr* frame_data_ = nullptr;
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
            const ::FrameDataArr& frame_data,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(reg, rp, frame_data, cmd_pool, device);
        }

        std::string_view name() const override { return "shadow skinned"; }

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

}}  // namespace ::task


// Ocean Tilde H
namespace {

    class RpStatesOceanTildeH
        : public mirinae::IRpBase
        , public mirinae::IPipelinePair {

    public:
        RpStatesOceanTildeH(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
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

                auto img = rp_res.new_img("ocean_noise", this->names());
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
                        auto img = rp_res.new_img(img_name, this->names());
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
                mirinae::DescLayoutBuilder builder{ names() + ":main" };
                builder
                    .new_binding()  // Cascade 0, 1, 2
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .add_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .set_count(3)
                    .finish_binding()
                    .add_img(VK_SHADER_STAGE_COMPUTE_BIT, 1);
                rp_res.desclays_.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                auto& layout = rp_res.desclays_.get(names() + ":main");

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
                    .desc(rp_res.desclays_.get(names() + ":main").layout())
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
                    rp_res_.free_img(hk->id(), this->names());
                }
                fd.desc_set_ = VK_NULL_HANDLE;
            }

            rp_res_.free_img(noise_textures_->id(), this->names());
            desc_pool_.destroy(device_.logi_device());
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        std::string_view name() const override { return "ocean_tilde_h"; }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<task::RpTask>();
            out->init(
                cosmos_.reg(), *this, frame_data_, rp_res_.cmd_pool_, device_
            );
            return out;
        }

        VkPipeline pipeline() const override { return pipeline_; }
        VkPipelineLayout pipe_layout() const override { return pipe_layout_; }

        std::string names() const { return std::string(this->name()); }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        ::FrameDataArr frame_data_;
        mirinae::HRpImage noise_textures_;
        mirinae::DescPool desc_pool_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        sung::MonotonicRealtimeTimer timer_;
    };

}  // namespace


namespace mirinae::rp::ocean {

    std::unique_ptr<mirinae::IRpBase> create_rp_states_ocean_tilde_h(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesOceanTildeH>(cosmos, rp_res, device);
    }

}  // namespace mirinae::rp::ocean
