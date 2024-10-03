#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::shadow {

    void create_rp(
        IRenderPassRegistry& reg,
        VkFormat depth_format,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    );

}  // namespace mirinae::rp::shadow
