#include "mirinae/render/renderpass/gbuf.hpp"

#include <entt/entity/registry.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
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
                    .set_dim(fbuf_bundle.extent());

                for (int i = 0; i < swapchain.views_count(); ++i)
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
                .set_dim(fbuf_bundle.extent());
            for (int i = 0; i < swapchain.views_count(); ++i)
                fbufs_.push_back(fbuf_cinfo.build(device));
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

    private:
        glm::mat4 pvm_;
        glm::mat4 view_;
        glm::mat4 model_;
        glm::vec4 tile_index_count_;
        glm::vec4 height_map_size_fbuf_size_;
        float height_scale_;
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

        builder.input_assembly_state().topology_patch_list();

        builder.tes_state().patch_ctrl_points(4);

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
                          .add_vertex_flag()
                          .add_tesc_flag()
                          .add_tese_flag()
                          .add_frag_flag()
                          .pc<U_GbufTerrainPushConst>(0)
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            mirinae::FbufCinfo fbuf_cinfo;
            fbuf_cinfo.set_rp(renderpass_)
                .add_attach(fbuf_bundle.depth().image_view())
                .add_attach(fbuf_bundle.albedo().image_view())
                .add_attach(fbuf_bundle.normal().image_view())
                .add_attach(fbuf_bundle.material().image_view())
                .set_dim(fbuf_bundle.extent());
            for (int i = 0; i < swapchain.views_count(); ++i)
                fbufs_.push_back(fbuf_cinfo.build(device));
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

    void create_rp(
        IRenderPassRegistry& reg,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        reg.add<::gbuf::RPBundle>(
            "gbuf", fbuf_bundle, desclayouts, swapchain, device
        );
        reg.add<::gbuf_skin::RPBundle>(
            "gbuf_skin", fbuf_bundle, desclayouts, swapchain, device
        );
        reg.add<::gbuf_terrain::RPBundle>(
            "gbuf_terrain", fbuf_bundle, desclayouts, swapchain, device
        );
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
            this->record_static(
                ctxt.cmdbuf_,
                fbuf_exd_,
                *ctxt.draw_sheet_,
                ctxt.f_index_,
                ctxt.i_index_
            );

            this->record_skinned(
                ctxt.cmdbuf_,
                fbuf_exd_,
                *ctxt.draw_sheet_,
                ctxt.f_index_,
                ctxt.i_index_
            );
        }

    private:
        void record_static(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_exd,
            const mirinae::DrawSheet& draw_sheet,
            const mirinae::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index
        ) {
            auto& rp = *rp_gbuf_;

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
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
            const mirinae::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index
        ) {
            auto& rp = *rp_gbuf_skinned_;

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
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

}  // namespace mirinae::rp::gbuf


// RpMasterTerrain
namespace {

    class TerrainRenUnit : public mirinae::ITerrainRenUnit {

    public:
        TerrainRenUnit(
            const mirinae::cpnt::Terrain& src_terr,
            mirinae::ITextureManager& tex,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device) {
            height_map_ = tex.block_for_tex(src_terr.height_map_path_, false);
            albedo_map_ = tex.block_for_tex(src_terr.albedo_map_path_, false);

            auto& layout = desclayouts.get("gbuf_terrain:main");
            desc_pool_.init(3, layout.size_info(), device.logi_device());
            desc_set_ = desc_pool_.alloc(layout.layout(), device.logi_device());

            auto& sam = device.samplers();
            mirinae::DescWriteInfoBuilder{}
                .set_descset(desc_set_)
                .add_img_sampler(height_map_->image_view(), sam.get_heightmap())
                .add_img_sampler(albedo_map_->image_view(), sam.get_linear())
                .apply_all(device.logi_device());
        }

        ~TerrainRenUnit() override {
            desc_pool_.destroy(device_.logi_device());
        }

        mirinae::VulkanDevice& device_;
        std::shared_ptr<mirinae::ITexture> height_map_;
        std::shared_ptr<mirinae::ITexture> albedo_map_;
        mirinae::DescPool desc_pool_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    };


    class RpMasterTerrain : public mirinae::rp::gbuf::IRpMasterTerrain {

    public:
        void init(
            mirinae::ITextureManager& tex_man,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        ) override {
            auto& layout = desclayouts.get("gbuf_terrain:main");
        }

        void init_ren_units(
            mirinae::CosmosSimulator& cosmos,
            mirinae::ITextureManager& tex_man,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        ) {
            namespace cpnt = mirinae::cpnt;
            auto& reg = cosmos.reg();

            for (auto e : reg.view<cpnt::Terrain>()) {
                auto& terr = reg.get<cpnt::Terrain>(e);
                if (terr.ren_unit_)
                    continue;

                terr.ren_unit_ = std::make_unique<TerrainRenUnit>(
                    terr, tex_man, desclayouts, device
                );
            }
        }

        void destroy(mirinae::VulkanDevice& device) override {}

        void record(
            mirinae::RpContext& ctxt,
            const VkExtent2D& fbuf_exd,
            const mirinae::IRenderPassRegistry& rp_pkg
        ) override {
            namespace cpnt = mirinae::cpnt;

            const auto cmdbuf = ctxt.cmdbuf_;
            auto& rp = rp_pkg.get("gbuf_terrain");
            auto& reg = ctxt.cosmos_->reg();

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(ctxt.i_index_.get()))
                .wh(fbuf_exd)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_exd }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_exd }.record_scissor(cmdbuf);

            mirinae::PushConstInfo pc_info;
            pc_info.layout(rp.pipeline_layout())
                .add_stage_vert()
                .add_stage_tesc()
                .add_stage_tese()
                .add_stage_frag();

            for (auto e : reg.view<cpnt::Terrain>()) {
                auto& terr = reg.get<cpnt::Terrain>(e);

                const auto unit = terr.ren_unit<::TerrainRenUnit>();
                if (!unit)
                    continue;

                mirinae::DescSetBindInfo{}
                    .layout(rp.pipeline_layout())
                    .add(unit->desc_set_)
                    .record(cmdbuf);

                glm::dmat4 model_mat(1);
                if (auto tform = reg.try_get<cpnt::Transform>(e))
                    model_mat = tform->make_model_mat();

                ::gbuf_terrain::U_GbufTerrainPushConst pc;
                pc.pvm(ctxt.proj_mat_, ctxt.view_mat_, model_mat)
                    .tile_count(24, 24)
                    .height_map_size(unit->height_map_->extent())
                    .fbuf_size(fbuf_exd)
                    .height_scale(64);

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
        sung::MonotonicRealtimeTimer timer_;
    };

}  // namespace
namespace mirinae::rp::gbuf {

    std::unique_ptr<IRpMasterTerrain> create_rpm_terrain() {
        return std::make_unique<::RpMasterTerrain>();
    }

}  // namespace mirinae::rp::gbuf
