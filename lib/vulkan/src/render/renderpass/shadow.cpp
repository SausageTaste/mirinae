#include "mirinae/render/renderpass/shadow.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/renderpass/builder.hpp"


// shadowmap
namespace { namespace shadowmap {

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor").layout();
    }

    VkRenderPass create_renderpass(VkFormat depth, VkDevice logi_device) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(depth)
            .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .fin_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .op_pair_clear_store();

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
            .add_vert(":asset/spv/shadow_basic_vert.spv")
            .add_frag(":asset/spv/shadow_basic_frag.spv");

        builder.vertex_input_state().set_static();

        builder.rasterization_state()
            .depth_clamp_enable(device.has_supp_depth_clamp())
            .depth_bias(80, 8);

        builder.depth_stencil_state()
            .depth_test_enable(true)
            .depth_write_enable(true);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            VkFormat depth_format,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                depth_format,
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_actor(desclayouts, device))
                          .add_vertex_flag()
                          .pc<mirinae::U_ShadowPushConst>()
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return VK_NULL_HANDLE;
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
    };

}}  // namespace ::shadowmap


// shadowmap skin
namespace { namespace shadowmap_skin {

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor_skinned").layout();
    }

    VkRenderPass create_renderpass(VkFormat depth, VkDevice logi_device) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(depth)
            .ini_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .op_pair_load_store();

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
            .add_vert(":asset/spv/shadow_skin_vert.spv")
            .add_frag(":asset/spv/shadow_basic_frag.spv");

        builder.vertex_input_state().set_skinned();

        builder.rasterization_state()
            .depth_clamp_enable(device.has_supp_depth_clamp())
            .depth_bias(80, 8);

        builder.depth_stencil_state()
            .depth_test_enable(true)
            .depth_write_enable(true);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            VkFormat depth_format,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                depth_format,
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_actor(desclayouts, device))
                          .add_vertex_flag()
                          .pc<mirinae::U_ShadowPushConst>()
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return VK_NULL_HANDLE;
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
    };

}}  // namespace ::shadowmap_skin


// shadowmap skin transp
namespace { namespace shadowmap_skin_transp {

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:model").layout();
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor_skinned").layout();
    }

    VkRenderPass create_renderpass(VkFormat depth, VkDevice logi_device) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(depth)
            .ini_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .op_pair_load_store();

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
            .add_vert(":asset/spv/shadow_skin_vert.spv")
            .add_frag(":asset/spv/shadow_transp_frag.spv");

        builder.vertex_input_state().set_skinned();

        builder.rasterization_state()
            .depth_clamp_enable(device.has_supp_depth_clamp())
            .depth_bias(80, 8);

        builder.depth_stencil_state()
            .depth_test_enable(true)
            .depth_write_enable(true);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            VkFormat depth_format,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                depth_format,
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_actor(desclayouts, device))
                          .desc(create_desclayout_model(desclayouts, device))
                          .add_vertex_flag()
                          .pc<mirinae::U_ShadowPushConst>()
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return VK_NULL_HANDLE;
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
    };

}}  // namespace ::shadowmap_skin_transp


namespace mirinae::rp::shadow {

    void create_rp(
        IRenderPassRegistry& reg,
        VkFormat depth_format,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    ) {
        reg.add<::shadowmap::RPBundle>(
            "shadowmap", depth_format, desclayouts, device
        );
        reg.add<::shadowmap_skin::RPBundle>(
            "shadowmap_skin", depth_format, desclayouts, device
        );
        reg.add<::shadowmap_skin_transp::RPBundle>(
            "shadowmap_skin_transp", depth_format, desclayouts, device
        );
    }

}  // namespace mirinae::rp::shadow


// ShadowMapPool
namespace mirinae::rp::shadow {

#define CLS RpMaster::ShadowMapPool

    CLS::Item& CLS::at(size_t index) { return shadow_maps_.at(index); }

    VkImageView CLS::get_img_view_at(size_t index) const {
        return shadow_maps_.at(index).tex_->image_view();
    }

    void CLS::add(uint32_t width, uint32_t height, VulkanDevice& device) {
        auto& added = shadow_maps_.emplace_back();
        added.tex_ = create_tex_depth(width, height, device);
    }

    void CLS::recreate_fbufs(
        const IRenderPassBundle& rp, VulkanDevice& device
    ) {
        for (auto& x : shadow_maps_) {
            FbufCinfo fbuf_info;
            fbuf_info.set_rp(rp.renderpass())
                .add_attach(x.tex_->image_view())
                .set_dim(x.width(), x.height());
            x.fbuf_.init(fbuf_info.get(), device.logi_device());
        }
    }

    void CLS::destroy_fbufs(VulkanDevice& device) {
        for (auto& x : shadow_maps_) {
            x.fbuf_.destroy(device.logi_device());
        }
    }

#undef CLS

}  // namespace mirinae::rp::shadow


// RpMaster
namespace mirinae::rp::shadow {

    void RpMaster::record(
        const VkCommandBuffer cur_cmd_buf,
        const DrawSheet& draw_sheet,
        const FrameIndex frame_index,
        const IRenderPassRegistry& rp_pkg
    ) {
        this->record_static(cur_cmd_buf, draw_sheet, frame_index, rp_pkg);
        this->record_skinned(cur_cmd_buf, draw_sheet, frame_index, rp_pkg);
        this->record_skin_transp(cur_cmd_buf, draw_sheet, frame_index, rp_pkg);
    }

    void RpMaster::record_static(
        const VkCommandBuffer cur_cmd_buf,
        const DrawSheet& draw_sheet,
        const FrameIndex frame_index,
        const IRenderPassRegistry& rp_pkg
    ) {
        auto& rp = rp_pkg.get("shadowmap");

        assert(shadow_maps_.size() == 2);

        {
            auto& shadow = shadow_maps_.at(0);

            RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(shadow.fbuf())
                .wh(shadow.tex_->extent())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            const auto half_width = shadow.width() / 2.0;
            const auto half_height = shadow.height() / 2.0;
            const std::array<glm::dvec2, 4> offsets{
                glm::dvec2{ 0, 0 },
                glm::dvec2{ half_width, 0 },
                glm::dvec2{ 0, half_height },
                glm::dvec2{ half_width, half_height },
            };

            DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (size_t cascade_i = 0; cascade_i < 4; ++cascade_i) {
                const auto& cascade = cascade_info_.cascades_.at(cascade_i);
                auto& offset = offsets.at(cascade_i);

                Viewport{}
                    .set_xy(offset)
                    .set_wh(half_width, half_height)
                    .record_single(cur_cmd_buf);
                Rect2D{}
                    .set_xy(offset)
                    .set_wh(half_width, half_height)
                    .record_scissor(cur_cmd_buf);

                for (auto& pair : draw_sheet.static_) {
                    auto& unit = *pair.unit_;
                    unit.record_bind_vert_buf(cur_cmd_buf);

                    for (auto& actor : pair.actors_) {
                        descset_info
                            .set(actor.actor_->get_desc_set(frame_index.get()))
                            .record(cur_cmd_buf);

                        U_ShadowPushConst push_const;
                        push_const.pvm_ = cascade.light_mat_ * actor.model_mat_;

                        PushConstInfo{}
                            .layout(rp.pipeline_layout())
                            .add_stage_vert()
                            .record(cur_cmd_buf, push_const);

                        vkCmdDrawIndexed(
                            cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                        );
                    }
                }
            }

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        {
            auto& shadow = shadow_maps_.at(1);

            RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(shadow.fbuf())
                .wh(shadow.tex_->extent())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            Viewport{}
                .set_wh(shadow.width(), shadow.height())
                .record_single(cur_cmd_buf);
            Rect2D{}.set_wh(shadow.tex_->extent()).record_scissor(cur_cmd_buf);

            DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (auto& pair : draw_sheet.static_) {
                auto& unit = *pair.unit_;
                auto unit_desc = unit.get_desc_set(frame_index.get());
                unit.record_bind_vert_buf(cur_cmd_buf);

                for (auto& actor : pair.actors_) {
                    descset_info
                        .set(actor.actor_->get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    U_ShadowPushConst push_const;
                    push_const.pvm_ = shadow.mat_ * actor.model_mat_;

                    PushConstInfo{}
                        .layout(rp.pipeline_layout())
                        .add_stage_vert()
                        .record(cur_cmd_buf, push_const);

                    vkCmdDrawIndexed(
                        cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                    );
                }
            }
            vkCmdEndRenderPass(cur_cmd_buf);
        }
    }

    void RpMaster::record_skinned(
        const VkCommandBuffer cur_cmd_buf,
        const DrawSheet& draw_sheet,
        const FrameIndex frame_index,
        const IRenderPassRegistry& rp_pkg
    ) {
        auto& rp = rp_pkg.get("shadowmap_skin");

        {
            auto& shadow = shadow_maps_.at(0);

            RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(shadow.fbuf())
                .wh(shadow.tex_->extent())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            const auto half_width = shadow.width() / 2.0;
            const auto half_height = shadow.height() / 2.0;
            const std::array<glm::dvec2, 4> offsets{
                glm::dvec2{ 0, 0 },
                glm::dvec2{ half_width, 0 },
                glm::dvec2{ 0, half_height },
                glm::dvec2{ half_width, half_height },
            };

            DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (size_t cascade_i = 0; cascade_i < 4; ++cascade_i) {
                const auto& cascade = cascade_info_.cascades_.at(cascade_i);
                auto& offset = offsets.at(cascade_i);

                Viewport{}
                    .set_xy(offset)
                    .set_wh(half_width, half_height)
                    .record_single(cur_cmd_buf);
                Rect2D{}
                    .set_xy(offset)
                    .set_wh(half_width, half_height)
                    .record_scissor(cur_cmd_buf);

                for (auto& pair : draw_sheet.skinned_) {
                    auto& unit = *pair.unit_;
                    auto unit_desc = unit.get_desc_set(frame_index.get());
                    unit.record_bind_vert_buf(cur_cmd_buf);

                    for (auto& actor : pair.actors_) {
                        descset_info
                            .set(actor.actor_->get_desc_set(frame_index.get()))
                            .record(cur_cmd_buf);

                        U_ShadowPushConst push_const;
                        push_const.pvm_ = cascade.light_mat_ * actor.model_mat_;

                        mirinae::PushConstInfo{}
                            .layout(rp.pipeline_layout())
                            .add_stage_vert()
                            .record(cur_cmd_buf, push_const);

                        vkCmdDrawIndexed(
                            cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                        );
                    }
                }
            }

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        {
            auto& shadow = shadow_maps_.at(1);

            RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(shadow.fbuf())
                .wh(shadow.tex_->extent())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            Viewport{}
                .set_wh(shadow.width(), shadow.height())
                .record_single(cur_cmd_buf);
            Rect2D{}
                .set_wh(shadow.width(), shadow.height())
                .record_scissor(cur_cmd_buf);

            DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (auto& pair : draw_sheet.skinned_) {
                auto& unit = *pair.unit_;
                auto unit_desc = unit.get_desc_set(frame_index.get());
                unit.record_bind_vert_buf(cur_cmd_buf);

                for (auto& actor : pair.actors_) {
                    descset_info
                        .set(actor.actor_->get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    U_ShadowPushConst push_const;
                    push_const.pvm_ = shadow.mat_ * actor.model_mat_;

                    PushConstInfo{}
                        .layout(rp.pipeline_layout())
                        .add_stage_vert()
                        .record(cur_cmd_buf, push_const);

                    vkCmdDrawIndexed(
                        cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                    );
                }
            }
            vkCmdEndRenderPass(cur_cmd_buf);
        }
    }

    void RpMaster::record_skin_transp(
        const VkCommandBuffer cur_cmd_buf,
        const DrawSheet& draw_sheet,
        const FrameIndex frame_index,
        const IRenderPassRegistry& rp_pkg
    ) {
        auto& rp = rp_pkg.get("shadowmap_skin_transp");

        {
            auto& shadow = shadow_maps_.at(0);

            RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(shadow.fbuf())
                .wh(shadow.tex_->extent())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            const auto half_width = shadow.width() / 2.0;
            const auto half_height = shadow.height() / 2.0;
            const std::array<glm::dvec2, 4> offsets{
                glm::dvec2{ 0, 0 },
                glm::dvec2{ half_width, 0 },
                glm::dvec2{ 0, half_height },
                glm::dvec2{ half_width, half_height },
            };

            DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (size_t cascade_i = 0; cascade_i < 4; ++cascade_i) {
                const auto& cascade = cascade_info_.cascades_.at(cascade_i);
                auto& offset = offsets.at(cascade_i);

                Viewport{}
                    .set_xy(offset)
                    .set_wh(half_width, half_height)
                    .record_single(cur_cmd_buf);
                Rect2D{}
                    .set_xy(offset)
                    .set_wh(half_width, half_height)
                    .record_scissor(cur_cmd_buf);

                for (auto& pair : draw_sheet.skinned_trs_) {
                    auto& unit = *pair.unit_;
                    unit.record_bind_vert_buf(cur_cmd_buf);

                    descset_info.first_set(1)
                        .set(unit.get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    for (auto& actor : pair.actors_) {
                        descset_info.first_set(0)
                            .set(actor.actor_->get_desc_set(frame_index.get()))
                            .record(cur_cmd_buf);

                        U_ShadowPushConst push_const;
                        push_const.pvm_ = cascade.light_mat_ * actor.model_mat_;

                        mirinae::PushConstInfo{}
                            .layout(rp.pipeline_layout())
                            .add_stage_vert()
                            .record(cur_cmd_buf, push_const);

                        vkCmdDrawIndexed(
                            cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                        );
                    }
                }
            }

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        {
            auto& shadow = shadow_maps_.at(1);

            RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(shadow.fbuf())
                .wh(shadow.tex_->extent())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            Viewport{}
                .set_wh(shadow.width(), shadow.height())
                .record_single(cur_cmd_buf);
            Rect2D{}
                .set_wh(shadow.width(), shadow.height())
                .record_scissor(cur_cmd_buf);

            DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (auto& pair : draw_sheet.skinned_trs_) {
                auto& unit = *pair.unit_;
                unit.record_bind_vert_buf(cur_cmd_buf);

                descset_info.first_set(1)
                    .set(unit.get_desc_set(frame_index.get()))
                    .record(cur_cmd_buf);

                for (auto& actor : pair.actors_) {
                    descset_info.first_set(0)
                        .set(actor.actor_->get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    U_ShadowPushConst push_const;
                    push_const.pvm_ = shadow.mat_ * actor.model_mat_;

                    PushConstInfo{}
                        .layout(rp.pipeline_layout())
                        .add_stage_vert()
                        .record(cur_cmd_buf, push_const);

                    vkCmdDrawIndexed(
                        cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                    );
                }
            }
            vkCmdEndRenderPass(cur_cmd_buf);
        }
    }

}  // namespace mirinae::rp::shadow


namespace {

    class ShadowMapBundle : public mirinae::IShadowMapBundle {

    public:
        struct Item {
            auto width() const { return tex_->width(); }
            auto height() const { return tex_->height(); }
            VkFramebuffer fbuf() { return fbuf_.get(); }

            std::unique_ptr<mirinae::ITexture> tex_;
            mirinae::Fbuf fbuf_;
            entt::entity entt_ = entt::null;
        };

    public:
        ShadowMapBundle(mirinae::VulkanDevice& device) : device_(device) {}

        ~ShadowMapBundle() {
            for (auto& x : dlights_) {
                x.fbuf_.destroy(device_.logi_device());
            }

            for (auto& x : slights_) {
                x.fbuf_.destroy(device_.logi_device());
            }
        }

        uint32_t dlight_count() const override { return dlights_.size(); }

        entt::entity dlight_entt_at(size_t idx) override {
            return dlights_.at(idx).entt_;
        }

        VkImageView dlight_view_at(size_t idx) override {
            return dlights_.at(idx).tex_->image_view();
        }

        uint32_t slight_count() const override { return slights_.size(); }

        entt::entity slight_entt_at(size_t idx) override {
            return slights_.at(idx).entt_;
        }

        VkImageView slight_view_at(size_t idx) override {
            return slights_.at(idx).tex_->image_view();
        }

        void recreate_fbufs(
            const VkRenderPass rp, mirinae::VulkanDevice& device
        ) {
            mirinae::FbufCinfo fbuf_info;
            fbuf_info.set_rp(rp);

            for (auto& x : dlights_) {
                fbuf_info.clear_attach()
                    .add_attach(x.tex_->image_view())
                    .set_dim(x.width(), x.height());
                x.fbuf_.init(fbuf_info.get(), device.logi_device());
            }

            for (auto& x : slights_) {
                fbuf_info.clear_attach()
                    .add_attach(x.tex_->image_view())
                    .set_dim(x.width(), x.height());
                x.fbuf_.init(fbuf_info.get(), device.logi_device());
            }
        }

    public:
        mirinae::VulkanDevice& device_;

        std::vector<Item> dlights_;
        std::vector<Item> slights_;
        VkFormat depth_format_ = VK_FORMAT_UNDEFINED;
    };

}  // namespace


// Shadow static
namespace {

    class RpStatesShadowStatic : public mirinae::IRpStates {

    public:
        RpStatesShadowStatic(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            auto shadow_maps = dynamic_cast<::ShadowMapBundle*>(
                rp_res_.shadow_maps_.get()
            );
            MIRINAE_ASSERT(shadow_maps);

            // Depth format
            {
                shadow_maps->depth_format_ = device_.img_formats().depth_map();
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(shadow_maps->depth_format_)
                    .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .fin_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                    )
                    .op_pair_clear_store();

                builder.depth_attach_ref().set(0);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                auto& desclayout = desclayouts.get("gbuf:actor");

                mirinae::PipelineLayoutBuilder{}
                    .desc(desclayout.layout())
                    .add_vertex_flag()
                    .pc<mirinae::U_ShadowPushConst>()
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/shadow_basic_vert.spv")
                    .add_frag(":asset/spv/shadow_basic_frag.spv");

                builder.vertex_input_state().set_static();

                builder.rasterization_state()
                    .depth_clamp_enable(device.has_supp_depth_clamp())
                    .depth_bias(0, 1);

                builder.depth_stencil_state()
                    .depth_test_enable(true)
                    .depth_write_enable(true)
                    .depth_compare_op(VK_COMPARE_OP_GREATER);

                builder.dynamic_state()
                    .add(VK_DYNAMIC_STATE_DEPTH_BIAS)
                    .add_viewport()
                    .add_scissor();

                pipeline_ = builder.build(render_pass_.get(), pipe_layout_);
            }

            // Misc
            {
                shadow_maps->dlights_.emplace_back().tex_ =
                    mirinae::create_tex_depth(4096, 4096, device);
                shadow_maps->dlights_.emplace_back().tex_ =
                    mirinae::create_tex_depth(4096, 4096, device);
                shadow_maps->slights_.emplace_back().tex_ =
                    mirinae::create_tex_depth(512, 512, device);
                shadow_maps->slights_.emplace_back().tex_ =
                    mirinae::create_tex_depth(512, 512, device);
                shadow_maps->slights_.emplace_back().tex_ =
                    mirinae::create_tex_depth(512, 512, device);
                shadow_maps->recreate_fbufs(render_pass_.get(), device);

                clear_values_.at(0).depthStencil = { 0, 0 };
            }

            return;
        }

        ~RpStatesShadowStatic() override {
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "shadow_static";
            return name;
        }

        void record(mirinae::RpContext& ctxt) override {
            namespace cpnt = mirinae::cpnt;
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;
            const auto shadow_maps = dynamic_cast<::ShadowMapBundle*>(
                rp_res_.shadow_maps_.get()
            );
            MIRINAE_ASSERT(shadow_maps);

            size_t i = 0;
            for (const auto e : reg.view<cpnt::DLight>()) {
                const auto light_idx = i++;
                if (light_idx >= shadow_maps->dlights_.size())
                    break;

                auto& dlight = reg.get<cpnt::DLight>(e);
                auto& shadow = shadow_maps->dlights_.at(light_idx);
                shadow.entt_ = e;

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
                vkCmdSetDepthBias(cmdbuf, -10, 0, -5);

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

                    for (auto& pair : ctxt.draw_sheet_->static_) {
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

            i = 0;
            for (const auto e : reg.view<cpnt::SLight>()) {
                const auto light_idx = i++;
                if (light_idx >= shadow_maps->slights_.size())
                    break;

                auto& light = reg.get<cpnt::SLight>(e);
                auto& shadow = shadow_maps->slights_.at(light_idx);
                shadow.entt_ = e;

                auto light_mat = light.make_proj_mat();
                if (auto tform = reg.try_get<cpnt::Transform>(e))
                    light_mat = light_mat * tform->make_view_mat();

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
                vkCmdSetDepthBias(cmdbuf, 0, 0, 3);

                mirinae::Viewport{}
                    .set_wh(shadow.width(), shadow.height())
                    .record_single(cmdbuf);
                mirinae::Rect2D{}
                    .set_wh(shadow.width(), shadow.height())
                    .record_scissor(cmdbuf);

                mirinae::DescSetBindInfo descset_info{ pipe_layout_ };

                for (auto& pair : ctxt.draw_sheet_->static_) {
                    auto& unit = *pair.unit_;
                    auto unit_desc = unit.get_desc_set(ctxt.f_index_.get());
                    unit.record_bind_vert_buf(cmdbuf);

                    for (auto& actor : pair.actors_) {
                        descset_info
                            .set(actor.actor_->get_desc_set(ctxt.f_index_.get())
                            )
                            .record(cmdbuf);

                        mirinae::U_ShadowPushConst push_const;
                        push_const.pvm_ = light_mat * actor.model_mat_;

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


// Shadow skinned
namespace {

    class RpStatesShadowSkinned : public mirinae::IRpStates {

    public:
        RpStatesShadowSkinned(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            auto shadow_maps = dynamic_cast<::ShadowMapBundle*>(
                rp_res_.shadow_maps_.get()
            );
            MIRINAE_ASSERT(shadow_maps);

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(shadow_maps->depth_format_)
                    .ini_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                    )
                    .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
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
                    .depth_write_enable(true)
                    .depth_compare_op(VK_COMPARE_OP_GREATER);

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

        void record(mirinae::RpContext& ctxt) override {
            namespace cpnt = mirinae::cpnt;
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;
            const auto shadow_maps = dynamic_cast<::ShadowMapBundle*>(
                rp_res_.shadow_maps_.get()
            );
            MIRINAE_ASSERT(shadow_maps);

            for (size_t i = 0; i < shadow_maps->dlight_count(); ++i) {
                auto& shadow = shadow_maps->dlights_[i];
                if (shadow.entt_ == entt::null)
                    continue;
                auto& dlight = reg.get<cpnt::DLight>(shadow.entt_);

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
                vkCmdSetDepthBias(cmdbuf, 0, 0, 6);

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


namespace mirinae::rp::shadow {

    HShadowMaps create_shadow_maps_bundle(mirinae::VulkanDevice& device) {
        return std::make_shared<ShadowMapBundle>(device);
    }

    URpStates create_rp_states_shadow_static(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<::RpStatesShadowStatic>(
            rp_res, desclayouts, device
        );
    }

    URpStates create_rp_states_shadow_skinned(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<::RpStatesShadowSkinned>(
            rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp::shadow
