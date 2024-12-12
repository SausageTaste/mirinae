#pragma once

#include <daltools/img/img2d.hpp>

#include "mirinae/render/vkdevice.hpp"


namespace mirinae {

    enum class FbufUsage {
        color_attachment,
        depth_attachment,
        depth_stencil_attachment,
        depth_map,
    };


    class ITexture {

    public:
        virtual ~ITexture() = default;
        virtual VkFormat format() const = 0;
        virtual VkImageView image_view() = 0;

        virtual uint32_t width() const = 0;
        virtual uint32_t height() const = 0;

        VkExtent2D extent() const {
            return VkExtent2D{ this->width(), this->height() };
        }
    };


    class TextureManager {

    public:
        TextureManager(VulkanDevice& device);
        ~TextureManager();

        std::shared_ptr<ITexture> request(const respath_t& res_id, bool srgb);
        std::unique_ptr<ITexture> create_image(
            const std::string& id, const dal::IImage2D& image, bool srgb
        );
        std::unique_ptr<ITexture> create_depth(uint32_t width, uint32_t height);
        std::unique_ptr<ITexture> create_attachment(
            uint32_t width,
            uint32_t height,
            VkFormat,
            FbufUsage,
            const char* name
        );

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> pimpl_;
    };

}  // namespace mirinae
