#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::compo {

    void create_rp(
        IRenderPassRegistry& reg,
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );


    class RpMasterBasic {

    public:
        void init(
            DesclayoutManager& desclayouts,
            FbufImageBundle& fbufs,
            VkImageView dlight_shadowmap,
            VkImageView slight_shadowmap,
            VkImageView env_diffuse,
            VkImageView env_specular,
            VkImageView env_lut,
            VulkanDevice& device
        );

        void destroy(VulkanDevice& device);

        void record(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_ext,
            const FrameIndex frame_index,
            const ShainImageIndex image_index,
            const IRenderPassRegistry& rp_pkg
        );

        DescPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<Buffer> ubufs_;
    };


    class RpMasterSky {

    public:
        void init(
            VkImageView sky_texture,
            IRenderPassRegistry& rp_pkg,
            DesclayoutManager& desclayouts,
            FbufImageBundle& fbufs,
            Swapchain& shain,
            VulkanDevice& device
        );

        void destroy(VulkanDevice& device);

        void record(
            const VkCommandBuffer cur_cmd_buf,
            const glm::mat4 proj_inv,
            const glm::mat4 view_inv,
            const VkExtent2D& fbuf_ext,
            const FrameIndex frame_index,
            const ShainImageIndex image_index,
            const IRenderPassRegistry& rp_pkg
        );

    private:
        DescPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<VkFramebuffer> fbufs_;
    };

}  // namespace mirinae::rp::compo
