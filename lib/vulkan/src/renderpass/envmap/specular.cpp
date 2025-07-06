#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/envmap/envmap.hpp"

#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/envmap/cubemap.hpp"


// Tasks
namespace {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() {
            fence_.succeed(this);
            timer_.set_min();
        }

        void init(
            const mirinae::IRenPass& rp,
            mirinae::EnvmapBundle& envmaps,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            rp_ = &rp;
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
            this->record(cmdbuf_, *envmaps_->begin(), *rp_);
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const mirinae::EnvmapBundle::Item& env_item,
            const mirinae::IRenPass& rp
        ) {
            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.render_pass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = glm::perspectiveRH_ZO(
                sung::to_radians(90.0), 1.0, 1000.0, 0.1
            );

            mirinae::DescSetBindInfo descset_info{ rp.pipe_layout() };

            auto& cube_map = env_item.cube_map_;
            auto& specular = cube_map.specular();

            for (auto& mip : specular.mips()) {
                const mirinae::Rect2D scissor{ mip.extent2d() };
                const mirinae::Viewport viewport{ scissor.extent2d() };
                rp_info.wh(scissor.extent2d());

                for (int i = 0; i < 6; ++i) {
                    auto& face = mip.faces_[i];

                    rp_info.fbuf(face.fbuf_.get()).record_begin(cmdbuf);

                    vkCmdBindPipeline(
                        cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                    );

                    viewport.record_single(cmdbuf);
                    scissor.record_scissor(cmdbuf);

                    descset_info.set(cube_map.desc_set()).record(cmdbuf);

                    mirinae::U_EnvSpecularPushConst push_const;
                    push_const.proj_view_ = proj_mat *
                                            mirinae::CUBE_VIEW_MATS[i];
                    push_const.roughness_ = mip.roughness_;

                    mirinae::PushConstInfo{}
                        .layout(rp.pipe_layout())
                        .add_stage_vert()
                        .add_stage_frag()
                        .record(cmdbuf, push_const);

                    vkCmdDraw(cmdbuf, 36, 1, 0, 0);
                    vkCmdEndRenderPass(cmdbuf);
                }
            }

            mirinae::ImageMemoryBarrier barrier;
            barrier.image(specular.cube_img())
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_src_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .mip_base(0)
                .mip_count(specular.mip_levels())
                .layer_base(0)
                .layer_count(6);
            barrier.record_single(
                cmdbuf,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            );
        }

        const mirinae::DebugLabel DEBUG_LABEL{
            "Envmap Specular", 0.5, 0.78, 0.52
        };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;
        sung::MonotonicRealtimeTimer timer_;

        const mirinae::EnvmapBundle* envmaps_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        RpTask() {}

        void init(
            const mirinae::IRenPass& rp,
            mirinae::EnvmapBundle& envmaps,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(rp, envmaps, cmd_pool, device);
        }

        std::string_view name() const override { return "env specular"; }

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

    class RpBase
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<1> {

    public:
        RpBase(mirinae::RpCreateBundle& cbundle)
            : device_(cbundle.device_), rp_res_(cbundle.rp_res_) {
            // Clear values
            {
                clear_values_.at(0).color = { 0, 0, 0, 1 };
            }

            auto& desclay = rp_res_.desclays_.get("env diffuse:main");

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
                    .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_clear_store();

                builder.color_attach_ref().add_color_attach(0);

                builder.subpass_dep().add().preset_single();

                render_pass_.reset(
                    builder.build(device_.logi_device()), device_
                );
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclay.layout())
                    .add_vertex_flag()
                    .add_frag_flag()
                    .pc<mirinae::U_EnvSpecularPushConst>()
                    .build(pipe_layout_, device_);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device_ };

                builder.shader_stages()
                    .add_vert(":asset/spv/env_specular_vert.spv")
                    .add_frag(":asset/spv/env_specular_frag.spv");

                builder.color_blend_state().add(false, 1);

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_.reset(
                    builder.build(render_pass_, pipe_layout_), device_
                );
            }
        }

        ~RpBase() override { this->destroy_render_pass_elements(device_); }

        std::string_view name() const override { return "env specular"; }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<::RpTask>();
            out->init(
                *this,
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

    std::unique_ptr<IRpBase> create_rp_env_specular(RpCreateBundle& cbundle) {
        return std::make_unique<RpBase>(cbundle);
    }

}  // namespace mirinae::rp
