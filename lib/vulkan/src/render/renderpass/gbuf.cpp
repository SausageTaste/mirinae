#include "mirinae/render/renderpass/gbuf.hpp"

#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/renderpass/builder.hpp"
#include "mirinae/render/vkmajorplayers.hpp"


// gbuf
namespace { namespace gbuf {

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "gbuf:model" };
        builder
            .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_GbufModel
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // Albedo map
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // Normal map
        return desclayouts.add(builder, device.logi_device());
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "gbuf:actor" };
        builder.add_ubuf(VK_SHADER_STAGE_VERTEX_BIT, 1);  // U_GbufActor
        return desclayouts.add(builder, device.logi_device());
    }

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
            .add_vert(":asset/spv/gbuf_basic_vert.spv")
            .add_frag(":asset/spv/gbuf_basic_frag.spv");

        builder.vertex_input_state().set_static();

        builder.rasterization_state().cull_mode_back();

        builder.depth_stencil_state()
            .depth_test_enable(true)
            .depth_write_enable(true);

        builder.color_blend_state().add(false, 3);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            uint32_t width,
            uint32_t height,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.depth().format(),
                fbuf_bundle.albedo().format(),
                fbuf_bundle.normal().format(),
                fbuf_bundle.material().format(),
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(2).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(3).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0),
                formats_.at(1),
                formats_.at(2),
                formats_.at(3),
                device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_model(desclayouts, device))
                          .desc(create_desclayout_actor(desclayouts, device))
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            {
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(renderpass_)
                    .add_attach(fbuf_bundle.depth().image_view())
                    .add_attach(fbuf_bundle.albedo().image_view())
                    .add_attach(fbuf_bundle.normal().image_view())
                    .add_attach(fbuf_bundle.material().image_view())
                    .set_dim(width, height);

                for (int i = 0; i < swapchain.views_count(); ++i)
                    fbufs_.push_back(fbuf_cinfo.build(device));
            }
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return fbufs_.at(index);
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 4> formats_;
        std::array<VkClearValue, 4> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images
    };

}}  // namespace ::gbuf


// gbuf skin
namespace { namespace gbuf_skin {

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:model").layout();
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "gbuf:actor_skinned" };
        builder.add_ubuf(VK_SHADER_STAGE_VERTEX_BIT, 1);  // U_GbufActorSkinned
        return desclayouts.add(builder, device.logi_device());
    }

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
            .ini_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .op_pair_load_store();
        builder.attach_desc()
            .add(albedo)
            .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .op_pair_load_store();
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
            .add_vert(":asset/spv/gbuf_skin_vert.spv")
            .add_frag(":asset/spv/gbuf_basic_frag.spv");

        builder.vertex_input_state().set_skinned();

        builder.rasterization_state().cull_mode_back();

        builder.depth_stencil_state()
            .depth_test_enable(true)
            .depth_write_enable(true);

        builder.color_blend_state().add(false, 3);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            uint32_t width,
            uint32_t height,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.depth().format(),
                fbuf_bundle.albedo().format(),
                fbuf_bundle.normal().format(),
                fbuf_bundle.material().format(),
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(2).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(3).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0),
                formats_.at(1),
                formats_.at(2),
                formats_.at(3),
                device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_model(desclayouts, device))
                          .desc(create_desclayout_actor(desclayouts, device))
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            mirinae::FbufCinfo fbuf_cinfo;
            fbuf_cinfo.set_rp(renderpass_)
                .add_attach(fbuf_bundle.depth().image_view())
                .add_attach(fbuf_bundle.albedo().image_view())
                .add_attach(fbuf_bundle.normal().image_view())
                .add_attach(fbuf_bundle.material().image_view())
                .set_dim(width, height);
            for (int i = 0; i < swapchain.views_count(); ++i)
                fbufs_.push_back(fbuf_cinfo.build(device));
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return fbufs_.at(index);
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 4> formats_;
        std::array<VkClearValue, 4> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images
    };

}}  // namespace ::gbuf_skin


// gbuf terrain
namespace { namespace gbuf_terrain {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "gbuf_terrain:main" };
        builder.add_ubuf(
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, 1
        );  // U_GbufTerrainMain
        return desclayouts.add(builder, device.logi_device());
    }

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
            .ini_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .op_pair_load_store();
        builder.attach_desc()
            .add(albedo)
            .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .op_pair_load_store();
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
        VkRenderPass render_pass,
        VkPipelineLayout pipe_layout,
        mirinae::VulkanDevice& device
    ) {
        mirinae::PipelineBuilder builder{ device };

        builder.shader_stages()
            .add_vert(":asset/spv/gbuf_terrain_vert.spv")
            .add_tesc(":asset/spv/gbuf_terrain_tesc.spv")
            .add_tese(":asset/spv/gbuf_terrain_tese.spv")
            .add_frag(":asset/spv/gbuf_terrain_frag.spv");

        builder.vertex_input_state().set_skinned();

        builder.input_assembly_state().topology_patch_list();

        builder.tes_state().patch_ctrl_points(3);

        builder.rasterization_state().cull_mode_back();

        builder.depth_stencil_state()
            .depth_test_enable(true)
            .depth_write_enable(true);

        builder.color_blend_state().add(false, 3);

        builder.dynamic_state()
            .add(VK_DYNAMIC_STATE_LINE_WIDTH)
            .add_viewport()
            .add_scissor();

        return builder.build(render_pass, pipe_layout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            uint32_t width,
            uint32_t height,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.depth().format(),
                fbuf_bundle.albedo().format(),
                fbuf_bundle.normal().format(),
                fbuf_bundle.material().format(),
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(2).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(3).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0),
                formats_.at(1),
                formats_.at(2),
                formats_.at(3),
                device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            mirinae::FbufCinfo fbuf_cinfo;
            fbuf_cinfo.set_rp(renderpass_)
                .add_attach(fbuf_bundle.depth().image_view())
                .add_attach(fbuf_bundle.albedo().image_view())
                .add_attach(fbuf_bundle.normal().image_view())
                .add_attach(fbuf_bundle.material().image_view())
                .set_dim(width, height);
            for (int i = 0; i < swapchain.views_count(); ++i)
                fbufs_.push_back(fbuf_cinfo.build(device));
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return fbufs_.at(index);
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 4> formats_;
        std::array<VkClearValue, 4> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images
    };

}}  // namespace ::gbuf_terrain


namespace mirinae::rp::gbuf {

    void create_rp(
        IRenderPassRegistry& reg,
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        reg.add<::gbuf::RPBundle>(
            "gbuf", width, height, fbuf_bundle, desclayouts, swapchain, device
        );
        reg.add<::gbuf_skin::RPBundle>(
            "gbuf_skin",
            width,
            height,
            fbuf_bundle,
            desclayouts,
            swapchain,
            device
        );
        reg.add<::gbuf_terrain::RPBundle>(
            "gbuf_terrain",
            width,
            height,
            fbuf_bundle,
            desclayouts,
            swapchain,
            device
        );
    }

}  // namespace mirinae::rp::gbuf


// RpMaster
namespace mirinae::rp::gbuf {

    void RpMaster::record_static(
        const VkCommandBuffer cur_cmd_buf,
        const VkExtent2D& fbuf_exd,
        const DrawSheet& draw_sheet,
        const FrameIndex frame_index,
        const ShainImageIndex image_index,
        const IRenderPassRegistry& rp_pkg
    ) {
        auto& rp = rp_pkg.get("gbuf");

        RenderPassBeginInfo{}
            .rp(rp.renderpass())
            .fbuf(rp.fbuf_at(image_index.get()))
            .wh(fbuf_exd)
            .clear_value_count(rp.clear_value_count())
            .clear_values(rp.clear_values())
            .record_begin(cur_cmd_buf);

        vkCmdBindPipeline(
            cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
        );

        Viewport{ fbuf_exd }.record_single(cur_cmd_buf);
        Rect2D{ fbuf_exd }.record_scissor(cur_cmd_buf);

        DescSetBindInfo descset_info{ rp.pipeline_layout() };

        for (auto& pair : draw_sheet.static_pairs_) {
            for (auto& unit : pair.model_->render_units_) {
                descset_info.first_set(0)
                    .set(unit.get_desc_set(frame_index.get()))
                    .record(cur_cmd_buf);

                unit.record_bind_vert_buf(cur_cmd_buf);

                for (auto& actor : pair.actors_) {
                    descset_info.first_set(1)
                        .set(actor.actor_->get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    vkCmdDrawIndexed(
                        cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                    );
                }
            }
        }

        vkCmdEndRenderPass(cur_cmd_buf);
    }

    void RpMaster::record_skinned(
        const VkCommandBuffer cur_cmd_buf,
        const VkExtent2D& fbuf_exd,
        const DrawSheet& draw_sheet,
        const FrameIndex frame_index,
        const ShainImageIndex image_index,
        const IRenderPassRegistry& rp_pkg
    ) {
        auto& rp = rp_pkg.get("gbuf_skin");

        RenderPassBeginInfo{}
            .rp(rp.renderpass())
            .fbuf(rp.fbuf_at(image_index.get()))
            .wh(fbuf_exd)
            .clear_value_count(rp.clear_value_count())
            .clear_values(rp.clear_values())
            .record_begin(cur_cmd_buf);

        vkCmdBindPipeline(
            cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
        );

        Viewport{ fbuf_exd }.record_single(cur_cmd_buf);
        Rect2D{ fbuf_exd }.record_scissor(cur_cmd_buf);

        DescSetBindInfo descset_info{ rp.pipeline_layout() };

        for (auto& pair : draw_sheet.skinned_pairs_) {
            for (auto& unit : pair.model_->runits_) {
                descset_info.first_set(0)
                    .set(unit.get_desc_set(frame_index.get()))
                    .record(cur_cmd_buf);

                unit.record_bind_vert_buf(cur_cmd_buf);

                for (auto& actor : pair.actors_) {
                    descset_info.first_set(1)
                        .set(actor.actor_->get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    vkCmdDrawIndexed(
                        cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                    );
                }
            }
        }

        vkCmdEndRenderPass(cur_cmd_buf);
    }

}  // namespace mirinae::rp::gbuf


// RpMasterTerrain
namespace {

    class RpMasterTerrain : public mirinae::rp::gbuf::IRpMasterTerrain {

    public:
        void init() override {}

        void destroy(mirinae::VulkanDevice& device) override {}

        void record() override {}
    };

}  // namespace
namespace mirinae::rp::gbuf {

    std::unique_ptr<IRpMasterTerrain> create_rpm_terrain() {
        return std::make_unique<::RpMasterTerrain>();
    }

}  // namespace mirinae::rp::gbuf
