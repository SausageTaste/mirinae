#include "rp.hpp"

#include "mirinae/vulkan/base/renderpass/builder.hpp"


// env_base
namespace { namespace env_base {

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:model").layout();
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor").layout();
    }

    VkRenderPass create_renderpass(
        VkFormat depth, VkFormat color, VkDevice logi_device
    ) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(depth)
            .ini_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .op_pair_clear_store();
        builder.attach_desc()
            .add(color)
            .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .op_pair_load_store();

        builder.color_attach_ref().add_color_attach(1);

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
            .add_vert(":asset/spv/env_build_base_vert.spv")
            .add_frag(":asset/spv/env_build_base_frag.spv");

        builder.vertex_input_state()
            .clear_bindings()
            .clear_attribs()
            .add_binding<mirinae::VertexStatic>()
            .add_attrib_vec3(offsetof(mirinae::VertexStatic, pos_))
            .add_attrib_vec3(offsetof(mirinae::VertexStatic, normal_))
            .add_attrib_vec2(offsetof(mirinae::VertexStatic, texcoord_));

        builder.rasterization_state().cull_mode(VK_CULL_MODE_FRONT_BIT);

        builder.depth_stencil_state()
            .depth_test_enable(true)
            .depth_write_enable(true);

        builder.color_blend_state().add(false, 1);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                device.img_formats().depth_map(),
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            };

            clear_values_.at(0).depthStencil = { 0, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), formats_.at(1), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_model(desclayouts, device))
                          .desc(create_desclayout_actor(desclayouts, device))
                          .add_vertex_flag()
                          .pc<mirinae::U_EnvmapPushConst>()
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
        std::array<VkFormat, 2> formats_;
        std::array<VkClearValue, 2> clear_values_;
    };

}}  // namespace ::env_base


namespace mirinae {

    std::unique_ptr<IRenPass> create_rp_base(
        DesclayoutManager& desclayouts, VulkanDevice& device
    ) {
        return std::make_unique<env_base::RPBundle>(desclayouts, device);
    }

}  // namespace mirinae
