#include "mirinae/renderpass/shadow.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/shadow/bundles.hpp"


namespace {

    class DrawSet {

    public:
        struct SkinnedActor {
            const mirinae::RenderUnitSkinned* unit_ = nullptr;
            const mirinae::RenderActorSkinned* actor_ = nullptr;
            glm::dmat4 model_mat_{ 1 };
        };

        void get_all_skinned(const entt::registry& reg) {
            namespace cpnt = mirinae::cpnt;

            for (const auto e : reg.view<cpnt::MdlActorSkinned>()) {
                auto& mactor = reg.get<cpnt::MdlActorSkinned>(e);
                if (!mactor.model_)
                    continue;
                auto renmdl = mactor.get_model<mirinae::RenderModelSkinned>();
                if (!renmdl)
                    continue;
                auto actor = mactor.get_actor<mirinae::RenderActorSkinned>();
                if (!actor)
                    continue;

                glm::dmat4 model_mat(1);
                if (auto tfrom = reg.try_get<cpnt::Transform>(e))
                    model_mat = tfrom->make_model_mat();

                const auto unit_count = renmdl->runits_.size();
                for (size_t i = 0; i < unit_count; ++i) {
                    if (!mactor.visibility_.get(i))
                        continue;

                    auto& dst = skinned_.emplace_back();
                    dst.unit_ = &renmdl->runits_[i];
                    dst.actor_ = actor;
                    dst.model_mat_ = model_mat;
                }

                const auto unit_trs_count = renmdl->runits_alpha_.size();
                for (size_t i = 0; i < unit_trs_count; ++i) {
                    if (!mactor.visibility_.get(i + unit_count))
                        continue;

                    auto& dst = skinned_trs_.emplace_back();
                    dst.unit_ = &renmdl->runits_alpha_[i];
                    dst.actor_ = actor;
                    dst.model_mat_ = model_mat;
                }
            }
        }

        void clear() {
            skinned_.clear();
            skinned_trs_.clear();
        }

    public:
        std::vector<SkinnedActor> skinned_;
        std::vector<SkinnedActor> skinned_trs_;
    };

}  // namespace


// Tasks
namespace { namespace task {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            const mirinae::ShadowMapBundle& shadow_maps
        ) {
            reg_ = &reg;
            rp_ = &rp;
            shadow_maps_ = &shadow_maps;
        }

        void prepare(VkCommandBuffer cmdbuf, const mirinae::RpCtxt& ctxt) {
            cmdbuf_ = cmdbuf;
            ctxt_ = &ctxt;
        }

        enki::ITaskSet& fence() { return fence_; }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            if (cmdbuf_ == VK_NULL_HANDLE)
                return;

            draw_set_.clear();
            draw_set_.get_all_skinned(*reg_);

            mirinae::begin_cmdbuf(cmdbuf_);

            this->record_dlight(
                cmdbuf_, draw_set_, *rp_, *ctxt_, *reg_, shadow_maps_->dlights()
            );

            this->record_slight(
                cmdbuf_, draw_set_, *rp_, *ctxt_, *reg_, *shadow_maps_
            );

            mirinae::end_cmdbuf(cmdbuf_);
        }

        static void record_dlight(
            const VkCommandBuffer cmdbuf,
            const DrawSet& draw_set,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt,
            const entt::registry& reg,
            const mirinae::DlightShadowMapBundle& dlights
        ) {
            for (uint32_t i = 0; i < dlights.count(); ++i) {
                auto& shadow = dlights.at(i);
                auto dlight = reg.try_get<mirinae::cpnt::DLight>(shadow.entt());
                if (!dlight)
                    continue;

                mirinae::ImageMemoryBarrier{}
                    .image(shadow.img(ctxt.f_index_))
                    .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .old_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .new_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .set_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_signle_mip_layer()
                    .record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    );

                mirinae::RenderPassBeginInfo{}
                    .rp(rp.render_pass())
                    .fbuf(shadow.fbuf(ctxt.f_index_))
                    .wh(shadow.extent2d())
                    .clear_value_count(rp.clear_value_count())
                    .clear_values(rp.clear_values())
                    .record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );
                vkCmdSetDepthBias(cmdbuf, -20, 0, -10);

                const auto half_width = shadow.width() / 2.0;
                const auto half_height = shadow.height() / 2.0;
                const std::array<glm::dvec2, 4> offsets{
                    glm::dvec2{ 0, 0 },
                    glm::dvec2{ half_width, 0 },
                    glm::dvec2{ 0, half_height },
                    glm::dvec2{ half_width, half_height },
                };

                mirinae::DescSetBindInfo descset_info{ rp.pipe_layout() };

                for (size_t cascade_i = 0; cascade_i < 4; ++cascade_i) {
                    const auto& cascade = dlight->cascades_.cascades_.at(
                        cascade_i
                    );
                    auto& offset = offsets.at(cascade_i);

                    mirinae::Viewport{}
                        .set_xy(offset)
                        .set_wh(half_width, half_height)
                        .record_single(cmdbuf);
                    mirinae::Rect2D{}
                        .set_xy(offset)
                        .set_wh(half_width, half_height)
                        .record_scissor(cmdbuf);

                    for (auto& pair : draw_set.skinned_) {
                        auto& unit = *pair.unit_;
                        auto& actor = *pair.actor_;

                        unit.record_bind_vert_buf(cmdbuf);

                        descset_info
                            .set(actor.get_desc_set(ctxt.f_index_.get()))
                            .record(cmdbuf);

                        mirinae::U_ShadowPushConst push_const;
                        push_const.pvm_ = cascade.light_mat_ * pair.model_mat_;

                        mirinae::PushConstInfo{}
                            .layout(rp.pipe_layout())
                            .add_stage_vert()
                            .record(cmdbuf, push_const);

                        vkCmdDrawIndexed(
                            cmdbuf, unit.vertex_count(), 1, 0, 0, 0
                        );
                    }
                }

                vkCmdEndRenderPass(cmdbuf);
            }
        }

        static void record_slight(
            const VkCommandBuffer cmdbuf,
            const DrawSet& draw_set,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt,
            const entt::registry& reg,
            const mirinae::ShadowMapBundle& shadow_maps
        ) {
            auto& slights = shadow_maps.slights_;

            for (size_t i = 0; i < slights.size(); ++i) {
                const auto& shadow = slights.at(i);
                const auto e = shadow.entt_;
                auto slight = reg.try_get<mirinae::cpnt::SLight>(e);
                if (!slight)
                    continue;

                auto light_mat = slight->make_proj_mat();
                if (auto tform = reg.try_get<mirinae::cpnt::Transform>(e))
                    light_mat = light_mat * tform->make_view_mat();

                mirinae::ImageMemoryBarrier{}
                    .image(shadow.tex_->image())
                    .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .old_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .new_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .set_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_signle_mip_layer()
                    .record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    );

                mirinae::RenderPassBeginInfo{}
                    .rp(rp.render_pass())
                    .fbuf(shadow.fbuf())
                    .wh(shadow.tex_->extent())
                    .clear_value_count(rp.clear_value_count())
                    .clear_values(rp.clear_values())
                    .record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );
                vkCmdSetDepthBias(cmdbuf, 0, 0, -6);

                mirinae::Viewport{}
                    .set_wh(shadow.width(), shadow.height())
                    .record_single(cmdbuf);
                mirinae::Rect2D{}
                    .set_wh(shadow.width(), shadow.height())
                    .record_scissor(cmdbuf);

                mirinae::DescSetBindInfo descset_info{ rp.pipe_layout() };

                for (auto& pair : draw_set.skinned_) {
                    auto& unit = *pair.unit_;
                    auto& actor = *pair.actor_;

                    unit.record_bind_vert_buf(cmdbuf);

                    descset_info.set(actor.get_desc_set(ctxt.f_index_.get()))
                        .record(cmdbuf);

                    mirinae::U_ShadowPushConst push_const;
                    push_const.pvm_ = light_mat * pair.model_mat_;

                    mirinae::PushConstInfo{}
                        .layout(rp.pipe_layout())
                        .add_stage_vert()
                        .record(cmdbuf, push_const);

                    vkCmdDrawIndexed(cmdbuf, unit.vertex_count(), 1, 0, 0, 0);
                }

                vkCmdEndRenderPass(cmdbuf);
            }
        }

        mirinae::FenceTask fence_;
        DrawSet draw_set_;

        const mirinae::ShadowMapBundle* shadow_maps_ = nullptr;
        const entt::registry* reg_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;
    };


    class RpTaskShadowStatic : public mirinae::IRpTask {

    public:
        RpTaskShadowStatic() {}

        void init(
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            mirinae::ShadowMapBundle& shadow_maps
        ) {
            record_tasks_.init(reg, rp, shadow_maps);
        }

        std::string_view name() const override { return "shadow static"; }

        void prepare(
            VkCommandBuffer cmdbuf, const mirinae::RpCtxt& ctxt
        ) override {
            record_tasks_.prepare(cmdbuf, ctxt);
        }

        enki::ITaskSet* record_task() override { return &record_tasks_; }

        enki::ITaskSet* record_fence() override {
            return &record_tasks_.fence();
        }

    private:
        DrawTasks record_tasks_;
    };

}}  // namespace ::task


// Render pass object
namespace {

    class RpStatesShadowSkinned
        : public mirinae::IRpBase
        , public mirinae::IRenPass {

    public:
        RpStatesShadowSkinned(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
            auto shadow_maps = dynamic_cast<mirinae::ShadowMapBundle*>(
                rp_res_.shadow_maps_.get()
            );
            MIRINAE_ASSERT(shadow_maps);

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(device.img_formats().depth_map())
                    .ini_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .fin_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();

                builder.depth_attach_ref().set(0);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclayouts.get("gbuf:actor_skinned").layout())
                    .add_vertex_flag()
                    .pc<mirinae::U_ShadowPushConst>()
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/shadow_skin_vert.spv")
                    .add_frag(":asset/spv/shadow_basic_frag.spv");

                builder.vertex_input_state().set_skinned();

                builder.rasterization_state()
                    .depth_clamp_enable(device.has_supp_depth_clamp())
                    .depth_bias(0, 1);

                builder.depth_stencil_state()
                    .depth_test_enable(true)
                    .depth_write_enable(true);

                builder.dynamic_state()
                    .add(VK_DYNAMIC_STATE_DEPTH_BIAS)
                    .add_viewport()
                    .add_scissor();

                pipeline_ = builder.build(render_pass_.get(), pipe_layout_);
            }

            // Misc
            {
                clear_values_.at(0).depthStencil = { 0, 0 };
            }

            return;
        }

        ~RpStatesShadowSkinned() override {
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        std::string_view name_sv() const override { return "shadow skinned"; }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            const auto shadow_maps = static_cast<mirinae::ShadowMapBundle*>(
                rp_res_.shadow_maps_.get()
            );

            auto out = std::make_unique<task::RpTaskShadowStatic>();
            out->init(cosmos_.reg(), *this, *shadow_maps);
            return out;
        }

        VkRenderPass render_pass() const override { return render_pass_.get(); }
        VkPipeline pipeline() const override { return pipeline_.get(); }
        VkPipelineLayout pipe_layout() const override {
            return pipe_layout_.get();
        }
        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }
        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
        std::array<VkClearValue, 1> clear_values_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_states_shadow_skinned(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<::RpStatesShadowSkinned>(
            cosmos, rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp
