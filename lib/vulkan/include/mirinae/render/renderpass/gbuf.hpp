#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::gbuf {

    void create_rp(
        IRenderPassRegistry& reg,
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );


    class RpMaster {

    public:
        void record_static(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_exd,
            const DrawSheet& draw_sheet,
            const FrameIndex frame_index,
            const ShainImageIndex image_index,
            const IRenderPassRegistry& rp_pkg
        );

        void record_skinned(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_exd,
            const DrawSheet& draw_sheet,
            const FrameIndex frame_index,
            const ShainImageIndex image_index,
            const IRenderPassRegistry& rp_pkg
        );
    };

}  // namespace mirinae::rp::gbuf
