#pragma once

#include <memory>

#include "mirinae/cosmos.hpp"
#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::envmap {

    void create_rp(
        IRenderPassRegistry& reg,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    );


    URpStates create_rp_states_envmap(
        CosmosSimulator& cosmos,
        IRenderPassRegistry& rp_pkg,
        RpResources& rp_res,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    );

}  // namespace mirinae::rp::envmap
