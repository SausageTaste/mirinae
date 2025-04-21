#include "mirinae/renderpass/shadow.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/shadow/bundles.hpp"


// Render pass object
namespace {

    class RpStatesShadowSkinned : public mirinae::IRpStates {

    public:
        RpStatesShadowSkinned(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
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

        const std::string& name() const override {
            static const std::string name = "shadow_skinned";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            namespace cpnt = mirinae::cpnt;
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;
            const auto shadow_maps = dynamic_cast<mirinae::ShadowMapBundle*>(
                rp_res_.shadow_maps_.get()
            );
            MIRINAE_ASSERT(shadow_maps);

            for (uint32_t i = 0; i < shadow_maps->dlights().count(); ++i) {
                auto& shadow = shadow_maps->dlights().at(i);
                if (shadow.entt() == entt::null)
                    continue;
                auto& dlight = reg.get<cpnt::DLight>(shadow.entt());

                mirinae::ImageMemoryBarrier{}
                    .image(shadow.img(ctxt.f_index_))
                    .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .old_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .new_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .set_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_signle_mip_layer()
                    .record_single(
                        ctxt.cmdbuf_,
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    );

                mirinae::RenderPassBeginInfo{}
                    .rp(render_pass_.get())
                    .fbuf(shadow.fbuf(ctxt.f_index_))
                    .wh(shadow.extent2d())
                    .clear_value_count(clear_values_.size())
                    .clear_values(clear_values_.data())
                    .record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_
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

                mirinae::DescSetBindInfo descset_info{ pipe_layout_ };

                for (size_t cascade_i = 0; cascade_i < 4; ++cascade_i) {
                    const auto& cascade = dlight.cascades_.cascades_.at(
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

                    for (auto& pair : ctxt.draw_sheet_->skinned_) {
                        auto& unit = *pair.unit_;
                        unit.record_bind_vert_buf(cmdbuf);

                        for (auto& actor : pair.actors_) {
                            descset_info
                                .set(actor.actor_->get_desc_set(
                                    ctxt.f_index_.get()
                                ))
                                .record(cmdbuf);

                            mirinae::U_ShadowPushConst push_const;
                            push_const.pvm_ = cascade.light_mat_ *
                                              actor.model_mat_;

                            mirinae::PushConstInfo{}
                                .layout(pipe_layout_)
                                .add_stage_vert()
                                .record(cmdbuf, push_const);

                            vkCmdDrawIndexed(
                                cmdbuf, unit.vertex_count(), 1, 0, 0, 0
                            );
                        }
                    }
                }

                vkCmdEndRenderPass(cmdbuf);
            }

            for (size_t i = 0; i < shadow_maps->slight_count(); ++i) {
                auto& shadow = shadow_maps->slights_[i];
                if (shadow.entt_ == entt::null)
                    continue;
                auto& slight = reg.get<cpnt::SLight>(shadow.entt_);
                auto& tform = reg.get<cpnt::Transform>(shadow.entt_);

                mirinae::ImageMemoryBarrier{}
                    .image(shadow.tex_->image())
                    .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .old_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .new_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .set_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_signle_mip_layer()
                    .record_single(
                        ctxt.cmdbuf_,
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    );

                const auto light_mat = slight.make_light_mat(tform);

                mirinae::RenderPassBeginInfo{}
                    .rp(render_pass_.get())
                    .fbuf(shadow.fbuf())
                    .wh(shadow.tex_->extent())
                    .clear_value_count(clear_values_.size())
                    .clear_values(clear_values_.data())
                    .record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_
                );
                vkCmdSetDepthBias(cmdbuf, 0, 0, -6);

                mirinae::Viewport{}
                    .set_wh(shadow.width(), shadow.height())
                    .record_single(cmdbuf);
                mirinae::Rect2D{}
                    .set_wh(shadow.width(), shadow.height())
                    .record_scissor(cmdbuf);

                mirinae::DescSetBindInfo descset_info{ pipe_layout_ };

                for (auto& pair : ctxt.draw_sheet_->skinned_) {
                    auto& unit = *pair.unit_;
                    auto unit_desc = unit.get_desc_set(ctxt.f_index_.get());
                    unit.record_bind_vert_buf(cmdbuf);

                    for (auto& a : pair.actors_) {
                        descset_info
                            .set(a.actor_->get_desc_set(ctxt.f_index_.get()))
                            .record(cmdbuf);

                        mirinae::U_ShadowPushConst push_const;
                        push_const.pvm_ = light_mat * a.model_mat_;

                        mirinae::PushConstInfo{}
                            .layout(pipe_layout_)
                            .add_stage_vert()
                            .record(cmdbuf, push_const);

                        vkCmdDrawIndexed(
                            cmdbuf, unit.vertex_count(), 1, 0, 0, 0
                        );
                    }
                }

                vkCmdEndRenderPass(cmdbuf);
            }

            return;
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
        std::array<VkClearValue, 1> clear_values_;
    };

}  // namespace


namespace mirinae::rp {

    URpStates create_rp_states_shadow_skinned(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<::RpStatesShadowSkinned>(
            rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp
