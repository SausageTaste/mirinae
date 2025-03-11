#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::gbuf {

    void create_rp(
        IRenderPassRegistry& reg,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );


    URpStates create_rp_states_gbuf(
        RpResources& rp_res,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );

    URpStates create_rp_states_gbuf_terrain(
        RpResources& rp_res,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );

}  // namespace mirinae::rp::gbuf
