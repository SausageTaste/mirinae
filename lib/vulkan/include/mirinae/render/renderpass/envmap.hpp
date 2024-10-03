#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::envmap {

    void create_rp(
        IRenderPassRegistry& reg,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    );

}  // namespace mirinae::rp::envmap
