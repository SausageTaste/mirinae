#include "mirinae/renderpass/shadow.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderee/terrain.hpp"
#include "mirinae/renderpass/builder.hpp"


// shadowmap skin transp
/*
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

            clear_values_.at(0).depthStencil = { 0, 0 };

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
*/


namespace {

    using IShadowBundle = mirinae::IShadowMapBundle;


    class ImageFbufPair {

    public:
        void init_img(uint32_t w, uint32_t h, mirinae::VulkanDevice& device) {
            tex_ = mirinae::create_tex_depth(w, h, device);
        }

        void init_fbuf(
            VkRenderPass render_pass, mirinae::VulkanDevice& device
        ) {
            mirinae::FbufCinfo fbuf_info;
            fbuf_info.set_rp(render_pass)
                .clear_attach()
                .add_attach(tex_->image_view())
                .set_dim(tex_->width(), tex_->height());
            fbuf_.init(fbuf_info.get(), device.logi_device());
        }

        void destroy(mirinae::VulkanDevice& device) {
            fbuf_.destroy(device.logi_device());
            tex_.reset();
        }

        VkImage img() const { return tex_->image(); }
        VkImageView view() const { return tex_->image_view(); }
        VkFramebuffer fbuf() const { return fbuf_.get(); }

        uint32_t width() const { return tex_->width(); }
        uint32_t height() const { return tex_->height(); }

    private:
        std::unique_ptr<mirinae::ITexture> tex_;
        mirinae::Fbuf fbuf_;
    };


    class DlightShadowMap : public IShadowBundle::IDlightShadowMap {

    public:
        void init_images(
            const uint32_t w,
            const uint32_t h,
            const size_t frames_in_flight,
            mirinae::VulkanDevice& device
        ) {
            images_.resize(frames_in_flight);
            for (auto& x : images_) {
                x.init_img(w, h, device);
            }
        }

        void init_fbufs(
            const VkRenderPass render_pass, mirinae::VulkanDevice& device
        ) {
            for (auto& x : images_) {
                x.init_fbuf(render_pass, device);
            }
        }

        void destroy(mirinae::VulkanDevice& device) {
            for (auto& x : images_) {
                x.destroy(device);
            }
            images_.clear();
            entt_ = entt::null;
        }

        VkImage img(mirinae::FrameIndex f_idx) const override {
            return images_.at(f_idx).img();
        }

        VkImageView view(mirinae::FrameIndex f_idx) const override {
            return images_.at(f_idx).view();
        }

        VkFramebuffer fbuf(mirinae::FrameIndex f_idx) const override {
            return images_.at(f_idx).fbuf();
        }

        entt::entity entt() const override { return entt_; }

        void set_entt(entt::entity entt) { entt_ = entt; }

        uint32_t width() const { return images_.at(0).width(); }

        uint32_t height() const { return images_.at(0).height(); }

        VkExtent2D extent2d() const {
            return { this->width(), this->height() };
        }

    private:
        std::vector<::ImageFbufPair> images_;
        entt::entity entt_ = entt::null;
    };


    class DlightShadowMapBundle : public IShadowBundle::IDlightShadowMapBundle {

    public:
        void init_images(
            const size_t shadow_count,
            const size_t frames_in_flight,
            mirinae::VulkanDevice& device
        ) {
            for (auto& x : dlights_) {
                x.destroy(device);
            }

            dlights_.clear();
            dlights_.resize(shadow_count);

            for (auto& x : dlights_) {
                x.init_images(2 << 11, 2 << 11, frames_in_flight, device);
            }
        }

        void init_fbufs(
            const VkRenderPass render_pass, mirinae::VulkanDevice& device
        ) {
            for (auto& x : dlights_) {
                x.init_fbufs(render_pass, device);
            }
        }

        void destroy(mirinae::VulkanDevice& device) {
            for (auto& x : dlights_) {
                x.destroy(device);
            }
        }

        uint32_t count() const override {
            return static_cast<uint32_t>(dlights_.size());
        }

        ::DlightShadowMap& at(uint32_t index) override {
            return dlights_.at(index);
        }

    private:
        std::vector<::DlightShadowMap> dlights_;
    };


    class ShadowMapBundle : public IShadowBundle {

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
        ShadowMapBundle(mirinae::VulkanDevice& device) : device_(device) {
            dlights_.init_images(2, mirinae::MAX_FRAMES_IN_FLIGHT, device);

            slights_.emplace_back().tex_ = mirinae::create_tex_depth(
                512, 512, device
            );
            slights_.emplace_back().tex_ = mirinae::create_tex_depth(
                512, 512, device
            );
            slights_.emplace_back().tex_ = mirinae::create_tex_depth(
                512, 512, device
            );
        }

        ~ShadowMapBundle() {
            dlights_.destroy(device_);

            for (auto& x : slights_) {
                x.fbuf_.destroy(device_.logi_device());
            }
        }

        ::DlightShadowMapBundle& dlights() override { return dlights_; }

        const ::DlightShadowMapBundle& dlights() const override {
            return dlights_;
        }

        uint32_t slight_count() const override {
            return static_cast<uint32_t>(slights_.size());
        }

        entt::entity slight_entt_at(size_t idx) override {
            return slights_.at(idx).entt_;
        }

        VkImage slight_img_at(size_t idx) override {
            return slights_.at(idx).tex_->image();
        }

        VkImageView slight_view_at(size_t idx) override {
            return slights_.at(idx).tex_->image_view();
        }

        void recreate_fbufs(
            const VkRenderPass rp, mirinae::VulkanDevice& device
        ) {
            dlights_.init_fbufs(rp, device);

            mirinae::FbufCinfo fbuf_info;
            fbuf_info.set_rp(rp);

            for (auto& x : slights_) {
                fbuf_info.clear_attach()
                    .add_attach(x.tex_->image_view())
                    .set_dim(x.width(), x.height());
                x.fbuf_.init(fbuf_info.get(), device.logi_device());
            }
        }

    private:
        mirinae::VulkanDevice& device_;
        ::DlightShadowMapBundle dlights_;

    public:
        std::vector<Item> slights_;
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

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(device.img_formats().depth_map())
                    .ini_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .fin_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
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
                    .depth_write_enable(true);

                builder.dynamic_state()
                    .add(VK_DYNAMIC_STATE_DEPTH_BIAS)
                    .add_viewport()
                    .add_scissor();

                pipeline_ = builder.build(render_pass_.get(), pipe_layout_);
            }

            // Misc
            {
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
            auto& dlights = shadow_maps->dlights();

            uint32_t i = 0;
            for (const auto e : reg.view<cpnt::DLight>()) {
                const auto light_idx = i++;
                if (light_idx >= dlights.count())
                    break;

                auto& dlight = reg.get<cpnt::DLight>(e);
                auto& shadow = dlights.at(light_idx);
                shadow.set_entt(e);

                mirinae::ImageMemoryBarrier{}
                    .image(shadow.img(ctxt.f_index_))
                    .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .old_lay(VK_IMAGE_LAYOUT_UNDEFINED)
                    .new_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .set_src_acc(0)
                    .set_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_signle_mip_layer()
                    .record_single(
                        ctxt.cmdbuf_,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
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
                vkCmdSetDepthBias(cmdbuf, -10, 0, -5);

                const auto half_width = shadow.width() * 0.5;
                const auto half_height = shadow.height() * 0.5;
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

                mirinae::ImageMemoryBarrier{}
                    .image(shadow.tex_->image())
                    .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .old_lay(VK_IMAGE_LAYOUT_UNDEFINED)
                    .new_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .set_src_acc(0)
                    .set_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_signle_mip_layer()
                    .record_single(
                        ctxt.cmdbuf_,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    );

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
                vkCmdSetDepthBias(cmdbuf, 0, 0, -3);

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

        void record(mirinae::RpContext& ctxt) override {
            namespace cpnt = mirinae::cpnt;
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;
            const auto shadow_maps = dynamic_cast<::ShadowMapBundle*>(
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


// Shadow skinned transp
namespace {

    class RpStatesShadowSkinnedTransp : public mirinae::IRpStates {

    public:
        RpStatesShadowSkinnedTransp(
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
                    .desc(desclayouts.get("gbuf:model").layout())
                    .add_vertex_flag()
                    .pc<mirinae::U_ShadowPushConst>()
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/shadow_skin_vert.spv")
                    .add_frag(":asset/spv/shadow_transp_frag.spv");

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

        ~RpStatesShadowSkinnedTransp() override {
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "shadow_skinned_transp";
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

                    for (auto& pair : ctxt.draw_sheet_->skinned_trs_) {
                        auto& unit = *pair.unit_;
                        unit.record_bind_vert_buf(cmdbuf);

                        descset_info.first_set(1)
                            .set(unit.get_desc_set(ctxt.f_index_.get()))
                            .record(cmdbuf);

                        for (auto& actor : pair.actors_) {
                            descset_info.first_set(0)
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

                for (auto& pair : ctxt.draw_sheet_->skinned_trs_) {
                    auto& unit = *pair.unit_;
                    auto unit_desc = unit.get_desc_set(ctxt.f_index_.get());
                    unit.record_bind_vert_buf(cmdbuf);

                    descset_info.first_set(1)
                        .set(unit.get_desc_set(ctxt.f_index_.get()))
                        .record(cmdbuf);

                    for (auto& a : pair.actors_) {
                        descset_info.first_set(0)
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


namespace {

    class U_ShadowTerrainPushConst {

    public:
        U_ShadowTerrainPushConst& pvm(const glm::mat4& pvm) {
            pvm_ = pvm;
            return *this;
        }

        U_ShadowTerrainPushConst& tile_index(int x, int y) {
            tile_index_count_.x = static_cast<float>(x);
            tile_index_count_.y = static_cast<float>(y);
            return *this;
        }

        U_ShadowTerrainPushConst& tile_count(int x, int y) {
            tile_index_count_.z = static_cast<float>(x);
            tile_index_count_.w = static_cast<float>(y);
            return *this;
        }

        U_ShadowTerrainPushConst& height_map_size(uint32_t x, uint32_t y) {
            height_map_size_fbuf_size_.x = static_cast<float>(x);
            height_map_size_fbuf_size_.y = static_cast<float>(y);
            return *this;
        }

        U_ShadowTerrainPushConst& height_map_size(const VkExtent2D& e) {
            height_map_size_fbuf_size_.x = static_cast<float>(e.width);
            height_map_size_fbuf_size_.y = static_cast<float>(e.height);
            return *this;
        }

        template <typename T>
        U_ShadowTerrainPushConst& fbuf_size(T x, T y) {
            height_map_size_fbuf_size_.z = static_cast<float>(x);
            height_map_size_fbuf_size_.w = static_cast<float>(y);
            return *this;
        }

        U_ShadowTerrainPushConst& fbuf_size(const VkExtent2D& x) {
            height_map_size_fbuf_size_.z = static_cast<float>(x.width);
            height_map_size_fbuf_size_.w = static_cast<float>(x.height);
            return *this;
        }

        U_ShadowTerrainPushConst& height_scale(float x) {
            height_scale_ = x;
            return *this;
        }

        U_ShadowTerrainPushConst& tess_factor(float x) {
            tess_factor_ = x;
            return *this;
        }

    private:
        glm::mat4 pvm_;
        glm::vec4 tile_index_count_;
        glm::vec4 height_map_size_fbuf_size_;
        float height_scale_;
        float tess_factor_;
    };


    class RpStatesShadowTerrain : public mirinae::IRpStates {

    public:
        RpStatesShadowTerrain(
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
                    .desc(desclayouts.get("gbuf_terrain:main").layout())
                    .add_vertex_flag()
                    .add_tesc_flag()
                    .add_tese_flag()
                    .pc<U_ShadowTerrainPushConst>(0)
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/shadow_terrain_vert.spv")
                    .add_tesc(":asset/spv/shadow_terrain_tesc.spv")
                    .add_tese(":asset/spv/shadow_terrain_tese.spv")
                    .add_frag(":asset/spv/shadow_basic_frag.spv");

                builder.input_assembly_state().topology_patch_list();

                builder.tes_state().patch_ctrl_points(4);

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

        ~RpStatesShadowTerrain() override {
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "shadow_terrain";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            namespace cpnt = mirinae::cpnt;

            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;
            const auto shadow_maps = dynamic_cast<::ShadowMapBundle*>(
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

                    mirinae::PushConstInfo pc_info;
                    pc_info.layout(pipe_layout_)
                        .add_stage_vert()
                        .add_stage_tesc()
                        .add_stage_tese();

                    for (auto e : reg.view<cpnt::Terrain>()) {
                        auto& terr = reg.get<cpnt::Terrain>(e);

                        auto unit = terr.ren_unit<mirinae::RenUnitTerrain>();
                        if (!unit)
                            continue;
                        if (!unit->is_ready())
                            continue;

                        mirinae::DescSetBindInfo{}
                            .layout(pipe_layout_)
                            .add(unit->desc_set())
                            .record(cmdbuf);

                        glm::dmat4 model_mat(1);
                        if (auto tform = reg.try_get<cpnt::Transform>(e))
                            model_mat = tform->make_model_mat();

                        ::U_ShadowTerrainPushConst pc;
                        pc.pvm(cascade.light_mat_ * model_mat)
                            .tile_count(24, 24)
                            .height_map_size(unit->height_map_size())
                            .fbuf_size(half_width, half_height)
                            .height_scale(64)
                            .tess_factor(terr.tess_factor_);

                        for (int x = 0; x < 24; ++x) {
                            for (int y = 0; y < 24; ++y) {
                                pc.tile_index(x, y);
                                pc_info.record(cmdbuf, pc);
                                vkCmdDraw(cmdbuf, 4, 1, 0, 0);
                            }
                        }
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

    URpStates create_rp_states_shadow_skinned_transp(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<::RpStatesShadowSkinnedTransp>(
            rp_res, desclayouts, device
        );
    }

    URpStates create_rp_states_shadow_terrain(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<::RpStatesShadowTerrain>(
            rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp
