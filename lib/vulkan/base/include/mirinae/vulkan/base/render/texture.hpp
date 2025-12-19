#pragma once

#include <dal/filesys/res_mgr.hpp>
#include <dal/img/img.hpp>
#include <sung/basic/expected.hpp>
#include <sung/basic/threading.hpp>

#include "mirinae/vulkan/base/render/vkdevice.hpp"


namespace mirinae {

    // Copy staging buffer to image and transition image layout, generate
    // mipmaps.
    void record_img_buf_copy_mip(
        const VkCommandBuffer cmdbuf,
        const uint32_t width,
        const uint32_t height,
        const uint32_t mip_levels,
        const VkImage dst_image,
        const VkBuffer src_buffer
    );


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
        virtual VkImage image() const = 0;
        virtual VkImageView image_view() const = 0;

        virtual uint32_t width() const = 0;
        virtual uint32_t height() const = 0;

        virtual void free_img_data() {}
        virtual const dal::IImage* img_data() const { return nullptr; }

        VkExtent2D extent() const {
            return VkExtent2D{ this->width(), this->height() };
        }
    };


    std::unique_ptr<ITexture> create_tex_depth(
        uint32_t width, uint32_t height, VulkanDevice& device
    );


    struct ITextureManager {
        virtual ~ITextureManager() = default;

        virtual dal::ReqResult request(const dal::path& res_id, bool srgb) = 0;

        virtual std::shared_ptr<ITexture> get(const dal::path& res_id) = 0;
        virtual std::shared_ptr<ITexture> missing_tex() = 0;

        virtual std::unique_ptr<ITexture> create_image(
            const std::string& id, const dal::IImage2D& image, bool srgb
        ) = 0;

        bool request_blck(const dal::path& res_id, bool srgb) {
            while (true) {
                const auto res = this->request(res_id, srgb);
                switch (res) {
                    case dal::ReqResult::loading:
                        continue;
                    case dal::ReqResult::ready:
                        return true;
                    default:
                        return false;
                }
            }
        }

        std::shared_ptr<ITexture> block_for_tex(
            const dal::path& res_id, bool srgb
        ) {
            if (this->request_blck(res_id, srgb))
                return this->get(res_id);
            else
                return nullptr;
        }
    };

    using HTexMgr = std::shared_ptr<ITextureManager>;
    HTexMgr create_tex_mgr(sung::HTaskSche task_sche, VulkanDevice& device);

}  // namespace mirinae
