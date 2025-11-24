#include "renderpass/envmap/envmap.hpp"

#include "mirinae/lightweight/task.hpp"

#include "render/cmdbuf.hpp"
#include "renderpass/builder.hpp"
#include "renderpass/envmap/cubemap.hpp"


// Tasks
namespace {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() {
            fence_.succeed(this);
            timer_.set_min();
        }

        void init(
            mirinae::EnvmapBundle& envmaps,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            envmaps_ = &envmaps;
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
            if (!timer_.check_if_elapsed(mirinae::ENVMAP_UPDATE_INTERVAL))
                return;

            cmdbuf_ = cmd_pool_->get(ctxt_->f_index_, tid, *device_);
            if (cmdbuf_ == VK_NULL_HANDLE)
                return;

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            this->record(cmdbuf_, *envmaps_->begin());
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const mirinae::EnvmapBundle::Item& env_item
        ) {
            namespace cpnt = mirinae::cpnt;

            const auto proj_mat = glm::perspectiveRH_ZO(
                sung::to_radians(90.0), 1.0, 1000.0, 0.1
            );

            auto& cube_map = env_item.cube_map_;
            auto& base_cube = cube_map.base();

            mirinae::Viewport{}
                .set_wh(base_cube.extent2d())
                .record_single(cmdbuf);
            mirinae::Rect2D{}
                .set_wh(base_cube.extent2d())
                .record_scissor(cmdbuf);

            auto& img = cube_map.base().img_;
            mirinae::ImageMemoryBarrier{}
                .image(img.image())
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                .set_src_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_TRANSFER_READ_BIT)
                .mip_base(0)
                .mip_count(1)
                .layer_base(0)
                .layer_count(6)
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                );

            for (uint32_t i = 1; i < img.mip_levels(); ++i) {
                mirinae::ImageMemoryBarrier barrier;
                barrier.image(img.image())
                    .set_src_access(VK_ACCESS_NONE)
                    .set_dst_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .new_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .mip_base(i)
                    .mip_count(1)
                    .layer_base(0)
                    .layer_count(6);
                barrier.record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                );

                mirinae::ImageBlit blit;
                blit.set_src_offsets_full(
                    img.width() >> (i - 1), img.height() >> (i - 1)
                );
                blit.set_dst_offsets_full(img.width() >> i, img.height() >> i);
                blit.src_subres()
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .mip_level(i - 1)
                    .layer_base(0)
                    .layer_count(6);
                blit.dst_subres()
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .mip_level(i)
                    .layer_base(0)
                    .layer_count(6);

                vkCmdBlitImage(
                    cmdbuf,
                    img.image(),
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    img.image(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &blit.get(),
                    VK_FILTER_LINEAR
                );

                barrier.image(img.image())
                    .set_src_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                    .set_dst_access(VK_ACCESS_TRANSFER_READ_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                    .new_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                barrier.record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                );
            }

            mirinae::ImageMemoryBarrier barrier;
            barrier.image(img.image())
                .set_src_access(VK_ACCESS_TRANSFER_READ_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .mip_base(0)
                .mip_count(img.mip_levels())
                .layer_base(0)
                .layer_count(6);
            barrier.record_single(
                cmdbuf,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            );
        }

        const mirinae::DebugLabel DEBUG_LABEL{
            "Envmap Mip Chain", 0.5, 0.78, 0.52
        };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;
        sung::MonotonicRealtimeTimer timer_;

        const mirinae::EnvmapBundle* envmaps_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        RpTask() {}

        void init(
            mirinae::EnvmapBundle& envmaps,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(envmaps, cmd_pool, device);
        }

        std::string_view name() const override { return "env mips"; }

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

}  // namespace


namespace {

    class RpBase : public mirinae::IRpBase {

    public:
        RpBase(mirinae::RpCreateBundle& cbundle)
            : device_(cbundle.device_), rp_res_(cbundle.rp_res_) {}

        ~RpBase() override {}

        std::string_view name() const override { return "env mips"; }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<::RpTask>();
            out->init(
                *static_cast<mirinae::EnvmapBundle*>(rp_res_.envmaps_.get()),
                rp_res_.cmd_pool_,
                device_
            );
            return out;
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_env_mip_chain(RpCreateBundle& cbundle) {
        return std::make_unique<RpBase>(cbundle);
    }

}  // namespace mirinae::rp
