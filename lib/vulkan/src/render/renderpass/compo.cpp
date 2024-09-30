#include "mirinae/render/renderpass/common.hpp"


// compo
namespace { namespace compo {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "compo:main" };
        builder
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // depth
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // albedo
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // normal
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // material
            .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_CompoMain
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // dlight shadowmap
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // slight shadowmap
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env diffuse
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env specular
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // env lut
        return desclayouts.add(builder, device.logi_device());
    }

    VkRenderPass create_renderpass(
        VkFormat compo_format, VkDevice logi_device
    ) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(compo_format)
            .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .op_pair_clear_store();

        builder.color_attach_ref().add_color_attach(0);

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
            .add_vert(":asset/spv/compo_vert.spv")
            .add_frag(":asset/spv/compo_frag.spv");

        builder.rasterization_state().cull_mode_back();

        builder.color_blend_state().add(false, 1);

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
                fbuf_bundle.compo().format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            mirinae::FbufCinfo fbuf_cinfo;
            fbuf_cinfo.set_rp(renderpass_)
                .set_dim(width, height)
                .add_attach(fbuf_bundle.compo().image_view());
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
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images
    };

}}  // namespace ::compo


namespace mirinae {

    void create_rp_compo(
        RpMap& out,
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        out["compo"] = std::make_unique<::compo::RPBundle>(
            width, height, fbuf_bundle, desclayouts, swapchain, device
        );
    }

}  // namespace mirinae
