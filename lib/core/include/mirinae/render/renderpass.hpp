#pragma once

#include "mirinae/render/renderee.hpp"


namespace mirinae {

    class FbufImageBundle {

    public:
        void init(uint32_t width, uint32_t height, mirinae::TextureManager& tex_man) {
            depth_ = tex_man.create_depth(width, height);
            albedo_ = tex_man.create_attachment(width, height, VK_FORMAT_R8G8B8A8_UNORM, mirinae::FbufUsage::color_attachment, "albedo");
            normal_ = tex_man.create_attachment(width, height, VK_FORMAT_R8G8B8A8_UNORM, mirinae::FbufUsage::color_attachment, "normal");
            composition_ = tex_man.create_attachment(width, height, VK_FORMAT_B10G11R11_UFLOAT_PACK32, mirinae::FbufUsage::color_attachment, "composition");
        }

        mirinae::ITexture& depth() { return *depth_; }
        mirinae::ITexture& albedo() { return *albedo_; }
        mirinae::ITexture& normal() { return *normal_; }
        mirinae::ITexture& composition() { return *composition_; }

    private:
        std::unique_ptr<mirinae::ITexture> depth_;
        std::unique_ptr<mirinae::ITexture> albedo_;
        std::unique_ptr<mirinae::ITexture> normal_;
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

    };


    std::unique_ptr<IRenderPassBundle> create_unorthodox(
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );

}