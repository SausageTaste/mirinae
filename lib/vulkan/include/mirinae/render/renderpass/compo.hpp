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


    URpStates create_rps_sky(
        VkImageView sky_tex,
        mirinae::FbufImageBundle& fbuf_bundle,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

}  // namespace mirinae::rp::compo
