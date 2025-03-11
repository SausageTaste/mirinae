#pragma once

#include "mirinae/cosmos.hpp"
#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::envmap {

    URpStates create_rp_states_envmap(
        CosmosSimulator& cosmos,
        RpResources& rp_res,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    );

}  // namespace mirinae::rp::envmap
