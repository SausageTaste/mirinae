#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/gbuf/gbuf.hpp"

#include "mirinae/renderpass/builder.hpp"


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

}}  // namespace ::gbuf_skin


// gbuf terrain
namespace { namespace gbuf_terrain {

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


/*
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
*/


namespace mirinae::rp {

    rg::URpImpl create_rpimpl_gbuf_static() { return nullptr; }

    rg::URpImpl create_rpimpl_gbuf_skinned() { return nullptr; }

    rg::URpImpl create_rpimpl_gbuf_terrain() { return nullptr; }

}  // namespace mirinae::rp
