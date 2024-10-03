#pragma once

#include <map>

#include "mirinae/render/renderee.hpp"
#include "mirinae/render/vkdevice.hpp"


namespace mirinae {

    class FbufImageBundle {

    public:
        void init(
            uint32_t width, uint32_t height, mirinae::TextureManager& tex_man
        ) {
            depth_ = tex_man.create_depth(width, height);
            albedo_ = tex_man.create_attachment(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "albedo"
            );
            normal_ = tex_man.create_attachment(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "normal"
            );
            material_ = tex_man.create_attachment(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "material"
            );
            compo_ = tex_man.create_attachment(
                width,
                height,
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                mirinae::FbufUsage::color_attachment,
                "compo"
            );
        }

        uint32_t width() const { return depth_->width(); }
        uint32_t height() const { return depth_->height(); }
        VkExtent2D extent() const { return { this->width(), this->height() }; }

        mirinae::ITexture& depth() { return *depth_; }
        mirinae::ITexture& albedo() { return *albedo_; }
        mirinae::ITexture& normal() { return *normal_; }
        mirinae::ITexture& material() { return *material_; }
        mirinae::ITexture& compo() { return *compo_; }

    private:
        std::unique_ptr<mirinae::ITexture> depth_;
        std::unique_ptr<mirinae::ITexture> albedo_;
        std::unique_ptr<mirinae::ITexture> normal_;
        std::unique_ptr<mirinae::ITexture> material_;
        std::unique_ptr<mirinae::ITexture> compo_;
    };


    class IRenderPassBundle {

    public:
        IRenderPassBundle(mirinae::VulkanDevice& device) : device_(device) {}

        virtual ~IRenderPassBundle() = default;
        virtual void destroy() = 0;

        virtual VkFramebuffer fbuf_at(uint32_t index) const = 0;
        virtual const VkClearValue* clear_values() const = 0;
        virtual uint32_t clear_value_count() const = 0;

        VkRenderPass renderpass() const { return renderpass_; }
        VkPipeline pipeline() const { return pipeline_; }
        VkPipelineLayout pipeline_layout() const { return layout_; }

    protected:
        mirinae::VulkanDevice& device_;
        VkRenderPass renderpass_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
    };


    class IRenderPassRegistry {

    public:
        virtual ~IRenderPassRegistry() = default;

        virtual void add(
            const std::string& name, std::unique_ptr<IRenderPassBundle>&& rp
        ) = 0;

        template <typename T, typename... Args>
        void add(const std::string& name, Args&&... args) {
            this->add(name, std::make_unique<T>(std::forward<Args>(args)...));
        }
    };

}  // namespace mirinae
