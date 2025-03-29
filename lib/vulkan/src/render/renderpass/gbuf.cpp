#include "mirinae/render/renderpass/gbuf.hpp"

#include <entt/entity/registry.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/renderee/terrain.hpp"
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
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // Normal map
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // ORM map
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
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.depth_format(),
                fbuf_bundle.albedo_format(),
                fbuf_bundle.normal_format(),
                fbuf_bundle.material_format(),
            };

            clear_values_.at(0).depthStencil = { 0, 0 };
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

            for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(renderpass_)
                    .add_attach(fbuf_bundle.depth(i).image_view())
                    .add_attach(fbuf_bundle.albedo(i).image_view())
                    .add_attach(fbuf_bundle.normal(i).image_view())
                    .add_attach(fbuf_bundle.material(i).image_view())
                    .set_dim(fbuf_bundle.extent());

                fbufs_.push_back(fbuf_cinfo.build(device));
            }
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);

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
            .fin_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .op_pair_load_store();
        builder.attach_desc()
            .add(albedo)
            .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
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
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.depth_format(),
                fbuf_bundle.albedo_format(),
                fbuf_bundle.normal_format(),
                fbuf_bundle.material_format(),
            };

            clear_values_.at(0).depthStencil = { 0, 0 };
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

            for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(renderpass_)
                    .add_attach(fbuf_bundle.depth(i).image_view())
                    .add_attach(fbuf_bundle.albedo(i).image_view())
                    .add_attach(fbuf_bundle.normal(i).image_view())
                    .add_attach(fbuf_bundle.material(i).image_view())
                    .set_dim(fbuf_bundle.extent());
                fbufs_.push_back(fbuf_cinfo.build(device));
            }
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);

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

    class U_GbufTerrainPushConst {

    public:
        U_GbufTerrainPushConst& pvm(
            const glm::dmat4& proj,
            const glm::dmat4& view,
            const glm::dmat4& model
        ) {
            pvm_ = proj * view * model;
            view_ = view;
            model_ = model;
            return *this;
        }

        U_GbufTerrainPushConst& tile_index(int x, int y) {
            tile_index_count_.x = static_cast<float>(x);
            tile_index_count_.y = static_cast<float>(y);
            return *this;
        }

        U_GbufTerrainPushConst& tile_count(int x, int y) {
            tile_index_count_.z = static_cast<float>(x);
            tile_index_count_.w = static_cast<float>(y);
            return *this;
        }

        U_GbufTerrainPushConst& height_map_size(uint32_t x, uint32_t y) {
            height_map_size_fbuf_size_.x = static_cast<float>(x);
            height_map_size_fbuf_size_.y = static_cast<float>(y);
            return *this;
        }

        U_GbufTerrainPushConst& height_map_size(const VkExtent2D& e) {
            height_map_size_fbuf_size_.x = static_cast<float>(e.width);
            height_map_size_fbuf_size_.y = static_cast<float>(e.height);
            return *this;
        }

        U_GbufTerrainPushConst& fbuf_size(const VkExtent2D& x) {
            height_map_size_fbuf_size_.z = static_cast<float>(x.width);
            height_map_size_fbuf_size_.w = static_cast<float>(x.height);
            return *this;
        }

        U_GbufTerrainPushConst& height_scale(float x) {
            height_scale_ = x;
            return *this;
        }

        U_GbufTerrainPushConst& tess_factor(float x) {
            tess_factor_ = x;
            return *this;
        }

    private:
        glm::mat4 pvm_;
        glm::mat4 view_;
        glm::mat4 model_;
        glm::vec4 tile_index_count_;
        glm::vec4 height_map_size_fbuf_size_;
        float height_scale_;
        float tess_factor_;
    };


    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "gbuf_terrain:main" };
        builder
            .add_img(
                VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                1
            )
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // Albedo map
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
            .fin_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .op_pair_load_store();
        builder.attach_desc()
            .add(albedo)
            .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
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

        builder.input_assembly_state().topology_patch_list();

        builder.tes_state().patch_ctrl_points(4);

        builder.rasterization_state().cull_mode_back();
        builder.rasterization_state().polygon_mode_line();

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
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.depth_format(),
                fbuf_bundle.albedo_format(),
                fbuf_bundle.normal_format(),
                fbuf_bundle.material_format(),
            };

            clear_values_.at(0).depthStencil = { 0, 0 };
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
                          .add_vertex_flag()
                          .add_tesc_flag()
                          .add_tese_flag()
                          .add_frag_flag()
                          .pc<U_GbufTerrainPushConst>(0)
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(renderpass_)
                    .add_attach(fbuf_bundle.depth(i).image_view())
                    .add_attach(fbuf_bundle.albedo(i).image_view())
                    .add_attach(fbuf_bundle.normal(i).image_view())
                    .add_attach(fbuf_bundle.material(i).image_view())
                    .set_dim(fbuf_bundle.extent());
                fbufs_.push_back(fbuf_cinfo.build(device));
            }
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);

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

    void create_desc_layouts(
        DesclayoutManager& desclayouts, VulkanDevice& device
    ) {
        ::gbuf::create_desclayout_actor(desclayouts, device);
        ::gbuf::create_desclayout_model(desclayouts, device);
        ::gbuf_skin::create_desclayout_actor(desclayouts, device);
        ::gbuf_skin::create_desclayout_model(desclayouts, device);
        ::gbuf_terrain::create_desclayout_main(desclayouts, device);
    }

}  // namespace mirinae::rp::gbuf


// RpMasterBasic
namespace {

    class RpMasterBasic : public mirinae::IRpStates {

    public:
        RpMasterBasic(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : device_(device) {
            rp_gbuf_ = std::make_unique<::gbuf::RPBundle>(
                rp_res.gbuf_, desclayouts, swapchain, device
            );

            rp_gbuf_skinned_ = std::make_unique<::gbuf_skin::RPBundle>(
                rp_res.gbuf_, desclayouts, swapchain, device
            );

            fbuf_exd_ = rp_res.gbuf_.extent();
        }

        const std::string& name() const override {
            static const std::string name = "gbuf";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            auto& gbufs = ctxt.rp_res_->gbuf_;

            this->record_static(
                ctxt.cmdbuf_, fbuf_exd_, *ctxt.draw_sheet_, ctxt.f_index_
            );

            mirinae::ImageMemoryBarrier{}
                .image(gbufs.depth(ctxt.f_index_.get()).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .old_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .set_src_access(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                .set_signle_mip_layer()
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                );

            mirinae::ImageMemoryBarrier color_barrier{};
            color_barrier.set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .set_src_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_signle_mip_layer();

            color_barrier.image(gbufs.albedo(ctxt.f_index_.get()).image())
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );

            color_barrier.image(gbufs.normal(ctxt.f_index_.get()).image())
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );

            color_barrier.image(gbufs.material(ctxt.f_index_.get()).image())
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );

            this->record_skinned(
                ctxt.cmdbuf_, fbuf_exd_, *ctxt.draw_sheet_, ctxt.f_index_
            );
        }

    private:
        void record_static(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_exd,
            const mirinae::DrawSheet& draw_sheet,
            const mirinae::FrameIndex frame_index
        ) {
            auto& rp = *rp_gbuf_;

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(frame_index.get()))
                .wh(fbuf_exd)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_exd }.record_single(cur_cmd_buf);
            mirinae::Rect2D{ fbuf_exd }.record_scissor(cur_cmd_buf);

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (auto& pair : draw_sheet.static_) {
                auto& unit = *pair.unit_;
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

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        void record_skinned(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_exd,
            const mirinae::DrawSheet& draw_sheet,
            const mirinae::FrameIndex frame_index
        ) {
            auto& rp = *rp_gbuf_skinned_;

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(frame_index.get()))
                .wh(fbuf_exd)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_exd }.record_single(cur_cmd_buf);
            mirinae::Rect2D{ fbuf_exd }.record_scissor(cur_cmd_buf);

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (auto& pair : draw_sheet.skinned_) {
                auto& unit = *pair.unit_;
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

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        mirinae::VulkanDevice& device_;
        std::unique_ptr<::gbuf::RPBundle> rp_gbuf_;
        std::unique_ptr<::gbuf_skin::RPBundle> rp_gbuf_skinned_;
        VkExtent2D fbuf_exd_;
    };

}  // namespace


// RpMasterTerrain
namespace {

    class RpMasterTerrain : public mirinae::IRpStates {

    public:
        RpMasterTerrain(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res), desclayouts_(desclayouts) {
            fbuf_exd_ = rp_res.gbuf_.extent();
            rp_ = std::make_unique<::gbuf_terrain::RPBundle>(
                rp_res.gbuf_, desclayouts, swapchain, device
            );
        }

        const std::string& name() const override {
            static const std::string name = "gbuf_terrain";
            return name;
        }

        void record(mirinae::RpContext& ctxt) override {
            namespace cpnt = mirinae::cpnt;

            const auto cmdbuf = ctxt.cmdbuf_;
            auto& rp = *rp_;
            auto& reg = ctxt.cosmos_->reg();
            auto& gbufs = ctxt.rp_res_->gbuf_;

            mirinae::ImageMemoryBarrier{}
                .image(gbufs.depth(ctxt.f_index_.get()).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .old_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .set_src_access(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                .set_signle_mip_layer()
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                );

            mirinae::ImageMemoryBarrier color_barrier{};
            color_barrier.set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .set_src_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_signle_mip_layer();

            color_barrier.image(gbufs.albedo(ctxt.f_index_.get()).image())
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );

            color_barrier.image(gbufs.normal(ctxt.f_index_.get()).image())
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );

            color_barrier.image(gbufs.material(ctxt.f_index_.get()).image())
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(ctxt.f_index_.get()))
                .wh(fbuf_exd_)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_exd_ }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_exd_ }.record_scissor(cmdbuf);

            mirinae::PushConstInfo pc_info;
            pc_info.layout(rp.pipeline_layout())
                .add_stage_vert()
                .add_stage_tesc()
                .add_stage_tese()
                .add_stage_frag();

            for (auto e : reg.view<cpnt::Terrain>()) {
                auto& terr = reg.get<cpnt::Terrain>(e);

                auto unit = terr.ren_unit<mirinae::RenUnitTerrain>();
                if (!unit) {
                    terr.ren_unit_ = std::make_unique<mirinae::RenUnitTerrain>(
                        terr, *rp_res_.tex_man_, desclayouts_, device_
                    );
                    unit = terr.ren_unit<::mirinae::RenUnitTerrain>();
                }

                mirinae::DescSetBindInfo{}
                    .layout(rp.pipeline_layout())
                    .add(unit->desc_set())
                    .record(cmdbuf);

                glm::dmat4 model_mat(1);
                if (auto tform = reg.try_get<cpnt::Transform>(e))
                    model_mat = tform->make_model_mat();

                ::gbuf_terrain::U_GbufTerrainPushConst pc;
                pc.pvm(ctxt.proj_mat_, ctxt.view_mat_, model_mat)
                    .tile_count(24, 24)
                    .height_map_size(unit->height_map_size())
                    .fbuf_size(fbuf_exd_)
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

            vkCmdEndRenderPass(cmdbuf);
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;
        mirinae::DesclayoutManager& desclayouts_;
        std::unique_ptr<::gbuf_terrain::RPBundle> rp_;
        VkExtent2D fbuf_exd_;
        sung::MonotonicRealtimeTimer timer_;
    };

}  // namespace


namespace {

    class RpImplGbufStatic : public mirinae::rg::IRenderPassImpl {

    public:
        bool init(
            const std::string& pass_name,
            mirinae::rg::IRenderGraph& rg,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        ) override {
            auto src_pass = rg.get_pass(pass_name);
            if (!src_pass) {
                SPDLOG_ERROR("Failed to get pass: {}", pass_name);
                return false;
            }

            // Pipeline layout
            mirinae::PipelineLayoutBuilder{}
                .desc(desclayouts.get("gbuf:model").layout())
                .desc(desclayouts.get("gbuf:actor").layout())
                .build(pipe_layout_, device);

            // Pipeline
            {
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

                pipeline_ = builder.build(src_pass->rp(), pipe_layout_);
            }

            return true;
        }

        void destroy(mirinae::VulkanDevice& device) override {
            pipeline_.destroy(device);
            pipe_layout_.destroy(device);
        }

        void record(
            mirinae::rg::IRenderGraph::IRenderPass& pass,
            const mirinae::RpContext& ctxt
        ) override {
            const auto cmdbuf = ctxt.cmdbuf_;
            const auto fbuf_extent = pass.fbuf_extent();

            mirinae::RenderPassBeginInfo{}
                .rp(pass.rp())
                .fbuf(pass.fbuf_at(ctxt.f_index_))
                .wh(fbuf_extent)
                .clear_value_count(pass.clear_value_count())
                .clear_values(pass.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_
            );

            mirinae::Viewport{ fbuf_extent }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_extent }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo descset_info{ pipe_layout_ };

            for (auto& pair : ctxt.draw_sheet_->static_) {
                auto& unit = *pair.unit_;
                descset_info.first_set(0)
                    .set(unit.get_desc_set(ctxt.f_index_.get()))
                    .record(cmdbuf);

                unit.record_bind_vert_buf(cmdbuf);

                for (auto& actor : pair.actors_) {
                    descset_info.first_set(1)
                        .set(actor.actor_->get_desc_set(ctxt.f_index_.get()))
                        .record(cmdbuf);

                    vkCmdDrawIndexed(cmdbuf, unit.vertex_count(), 1, 0, 0, 0);
                }
            }

            vkCmdEndRenderPass(cmdbuf);
        }

    private:
        mirinae::RpPipeLayout pipe_layout_;
        mirinae::RpPipeline pipeline_;
    };

}  // namespace


namespace mirinae::rp::gbuf {

    URpStates create_rp_states_gbuf(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::Swapchain& swapchain,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<::RpMasterBasic>(
            rp_res, desclayouts, swapchain, device
        );
    }

    URpStates create_rp_states_gbuf_terrain(
        RpResources& rp_res,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        return std::make_unique<RpMasterTerrain>(
            rp_res, desclayouts, swapchain, device
        );
    }

}  // namespace mirinae::rp::gbuf


namespace mirinae::rp {

    rg::URpImpl create_rpimpl_gbuf_static() {
        return std::make_unique<RpImplGbufStatic>();
    }

    rg::URpImpl create_rpimpl_gbuf_skinned() { return nullptr; }

    rg::URpImpl create_rpimpl_gbuf_terrain() { return nullptr; }

}  // namespace mirinae::rp
