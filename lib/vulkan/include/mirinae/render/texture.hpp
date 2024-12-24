#pragma once

#include <daltools/filesys/res_mgr.hpp>

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


    std::unique_ptr<ITexture> create_tex_depth(
        uint32_t width, uint32_t height, VulkanDevice& device
    );

    std::unique_ptr<ITexture> create_tex_attach(
        uint32_t width,
        uint32_t height,
        VkFormat,
        FbufUsage,
        const char* name,
        VulkanDevice& device
    );


    struct ITextureManager {
        virtual ~ITextureManager() = default;

        virtual dal::ReqResult request(const respath_t& res_id, bool srgb) = 0;

        virtual std::shared_ptr<ITexture> get(const respath_t& res_id) = 0;

        virtual std::unique_ptr<ITexture> create_image(
            const std::string& id, const dal::IImage2D& image, bool srgb
        ) = 0;

        bool request_blck(const respath_t& res_id, bool srgb) {
            while (true) {
                const auto res = this->request(res_id, srgb);
                switch (res) {
                    case dal::ReqResult::loading:
                        break;
                    case dal::ReqResult::ready:
                        return true;
                    default:
                        return false;
                }
            }
        }

        std::shared_ptr<ITexture> block_for_tex(
            const respath_t& res_id, bool srgb
        ) {
            if (this->request_blck(res_id, srgb))
                return this->get(res_id);
            else
                return nullptr;
        }
    };

    using HTexMgr = std::shared_ptr<ITextureManager>;
    HTexMgr create_tex_mgr(
        std::shared_ptr<dal::IResourceManager> res_mgr, VulkanDevice& device
    );

}  // namespace mirinae
