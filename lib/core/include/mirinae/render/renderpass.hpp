#pragma once

#include "mirinae/render/renderee.hpp"


namespace mirinae {

    class FbufImageBundle {

    public:
        void init(uint32_t width, uint32_t height, mirinae::TextureManager& tex_man) {
            depth_ = tex_man.create_depth(width, height);
            albedo_ = tex_man.create_attachment(width, height, VK_FORMAT_R8G8B8A8_UNORM, mirinae::FbufUsage::color_attachment, "albedo");
            normal_ = tex_man.create_attachment(width, height, VK_FORMAT_R8G8B8A8_UNORM, mirinae::FbufUsage::color_attachment, "normal");
            material_ = tex_man.create_attachment(width, height, VK_FORMAT_R8G8B8A8_UNORM, mirinae::FbufUsage::color_attachment, "material");
            composition_ = tex_man.create_attachment(width, height, VK_FORMAT_B10G11R11_UFLOAT_PACK32, mirinae::FbufUsage::color_attachment, "composition");
        }

        uint32_t width() const { return depth_->width(); }
        uint32_t height() const { return depth_->height(); }
        VkExtent2D extent() const { return { this->width(), this->height() }; }

        mirinae::ITexture& depth() { return *depth_; }
        mirinae::ITexture& albedo() { return *albedo_; }
        mirinae::ITexture& normal() { return *normal_; }
        mirinae::ITexture& material() { return *material_; }
        mirinae::ITexture& composition() { return *composition_; }

    private:
        std::unique_ptr<mirinae::ITexture> depth_;
        std::unique_ptr<mirinae::ITexture> albedo_;
        std::unique_ptr<mirinae::ITexture> normal_;
        std::unique_ptr<mirinae::ITexture> material_;
        std::unique_ptr<mirinae::ITexture> composition_;

    };


    class IRenderPassBundle {

    public:
        virtual ~IRenderPassBundle() = default;
        virtual void destroy() = 0;

        virtual VkRenderPass renderpass() = 0;
        virtual VkPipeline pipeline() = 0;
        virtual VkPipelineLayout pipeline_layout() = 0;
        virtual VkFramebuffer fbuf_at(uint32_t index) = 0;

        virtual const VkClearValue* clear_values() const = 0;
        virtual uint32_t clear_value_count() const = 0;

    };


    std::unique_ptr<IRenderPassBundle> create_gbuf(
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );

    std::unique_ptr<IRenderPassBundle> create_composition(
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );

    std::unique_ptr<IRenderPassBundle> create_transparent(
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );

    std::unique_ptr<IRenderPassBundle> create_fillscreen(
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );

    std::unique_ptr<IRenderPassBundle> create_overlay(
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );

}
