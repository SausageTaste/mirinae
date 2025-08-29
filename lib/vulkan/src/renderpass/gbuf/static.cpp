#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/gbuf/gbuf.hpp"

#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/draw_set.hpp"
#include "mirinae/renderpass/builder.hpp"


namespace {

    struct FrameData {
        mirinae::Fbuf fbuf_;
        VkExtent2D fbuf_size_;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;

}  // namespace


namespace {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const ::FrameDataArr& frame_data,
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            rp_ = &rp;
            reg_ = &reg;
            cmd_pool_ = &cmd_pool;
            device_ = &device;
            frame_data_ = &frame_data;
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

            draw_set_.clear();
            draw_set_.fetch(*reg_);

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            this->record(
                cmdbuf_,
                frame_data_->at(ctxt_->f_index_.get()),
                draw_set_,
                *rp_,
                *ctxt_
            );
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const ::FrameData& fd,
            const mirinae::DrawSetStatic& draw_set,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt
        ) {
            mirinae::RenderPassBeginInfo{}
                .rp(rp.render_pass())
                .fbuf(fd.fbuf_.get())
                .wh(fd.fbuf_size_)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fd.fbuf_size_ }.record_single(cmdbuf);
            mirinae::Rect2D{ fd.fbuf_size_ }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo descset_info{ rp.pipe_layout() };

            for (auto& pair : draw_set.opa()) {
                auto& unit = *pair.unit_;
                auto& actor = *pair.actor_;

                descset_info.first_set(0)
                    .set(unit.get_desc_set(ctxt.f_index_.get()))
                    .record(cmdbuf);

                unit.record_bind_vert_buf(cmdbuf);

                descset_info.first_set(1)
                    .set(actor.get_desc_set(ctxt.f_index_.get()))
                    .record(cmdbuf);

                vkCmdDrawIndexed(cmdbuf, unit.vertex_count(), 1, 0, 0, 0);
            }

            for (auto& pair : draw_set.skin_opa()) {
                auto& unit = *pair.unit_;
                auto& actor = *pair.actor_;
                auto& ac_unit = actor.get_runit(pair.runit_idx_);

                descset_info.first_set(0)
                    .set(unit.get_desc_set(ctxt.f_index_.get()))
                    .record(cmdbuf);

                const auto vertex_buffers =
                    ac_unit.vertex_buf(ctxt.f_index_).buffer();
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmdbuf, 0, 1, &vertex_buffers, offsets);
                vkCmdBindIndexBuffer(
                    cmdbuf,
                    unit.vk_buffers().idx().buffer(),
                    0,
                    VK_INDEX_TYPE_UINT32
                );

                descset_info.first_set(1)
                    .set(actor.get_descset(ctxt.f_index_))
                    .record(cmdbuf);

                vkCmdDrawIndexed(cmdbuf, unit.vertex_count(), 1, 0, 0, 0);
            }

            vkCmdEndRenderPass(cmdbuf);
        }

        const mirinae::DebugLabel DEBUG_LABEL{
            "G-buffer Static", 0.12, 0.58, 0.95
        };

        mirinae::FenceTask fence_;
        mirinae::DrawSetStatic draw_set_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const ::FrameDataArr* frame_data_ = nullptr;
        const entt::registry* reg_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        void init(
            const ::FrameDataArr& frame_data,
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(frame_data, reg, rp, cmd_pool, device);
        }

        std::string_view name() const override { return "gbuf static"; }

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

    VkRenderPass create_renderpass(
        VkFormat depth,
        VkFormat albedo,
        VkFormat normal,
        VkFormat material,
        VkDevice logi_device
    ) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(depth)
            .fin_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .op_pair_clear_store();
        builder.attach_desc()
            .add(albedo)
            .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .op_pair_clear_store();
        builder.attach_desc().dup(normal);
        builder.attach_desc().dup(material);

        builder.color_attach_ref()
            .add_color_attach(1)   // albedo
            .add_color_attach(2)   // normal
            .add_color_attach(3);  // material

        builder.depth_attach_ref().set(0);

        builder.subpass_dep().add().preset_single();

        return builder.build(logi_device);
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        mirinae::PipelineBuilder builder{ device };

        builder.shader_stages()
            .add_vert(":asset/spv/gbuf_static_vert.spv")
            .add_frag(":asset/spv/gbuf_static_frag.spv");

        builder.vertex_input_state().set_static();

        builder.rasterization_state().cull_mode_back();

        builder.depth_stencil_state()
            .depth_test_enable(true)
            .depth_write_enable(true);

        builder.color_blend_state().add(false, 3);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RpBase
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<4> {

    public:
        RpBase(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : cosmos_(cosmos), rp_res_(rp_res), device_(device) {
            clear_values_.at(0).depthStencil = { 0, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(2).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(3).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            render_pass_ = ::create_renderpass(
                rp_res.gbuf_.depth_format(),
                rp_res.gbuf_.albedo_format(),
                rp_res.gbuf_.normal_format(),
                rp_res.gbuf_.material_format(),
                device.logi_device()
            );

            mirinae::PipelineLayoutBuilder{}
                .desc(rp_res.desclays_.get("gbuf:model").layout())
                .desc(rp_res.desclays_.get("gbuf:actor").layout())
                .build(pipe_layout_, device);

            pipeline_ = ::create_pipeline(render_pass_, pipe_layout_, device);

            this->recreate_fbuf(fdata_);
        }

        ~RpBase() {
            for (auto& fd : fdata_) {
                fd.fbuf_.destroy(device_.logi_device());
            }

            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const { return "gbuf static"; }

        void on_resize(uint32_t width, uint32_t height) override {
            this->recreate_fbuf(fdata_);
        }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto task = std::make_unique<RpTask>();
            task->init(
                fdata_, cosmos_.reg(), *this, rp_res_.cmd_pool_, device_
            );
            return task;
        }

    private:
        void recreate_fbuf(::FrameDataArr& fdata) const {
            for (int i = 0; i < fdata.size(); ++i) {
                auto& fd = fdata.at(i);

                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(render_pass_)
                    .add_attach(rp_res_.gbuf_.depth(i).image_view())
                    .add_attach(rp_res_.gbuf_.albedo(i).image_view())
                    .add_attach(rp_res_.gbuf_.normal(i).image_view())
                    .add_attach(rp_res_.gbuf_.material(i).image_view())
                    .set_dim(rp_res_.gbuf_.extent());

                fd.fbuf_.init(fbuf_cinfo.get(), device_.logi_device());
                fd.fbuf_size_ = rp_res_.gbuf_.extent();
            }
        }

        ::FrameDataArr fdata_;

        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;
        mirinae::VulkanDevice& device_;
    };

}  // namespace


namespace mirinae::rp::gbuf {

    std::unique_ptr<IRpBase> create_rp_gbuf_static(RpCreateBundle& cbundle) {
        return std::make_unique<RpBase>(
            cbundle.cosmos_, cbundle.rp_res_, cbundle.device_
        );
    }

}  // namespace mirinae::rp::gbuf
