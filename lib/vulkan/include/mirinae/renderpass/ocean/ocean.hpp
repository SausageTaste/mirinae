#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp::ocean {

    std::unique_ptr<mirinae::IRpBase> create_rp_states_ocean_tilde_h(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::VulkanDevice& device
    );

    std::unique_ptr<mirinae::IRpBase> create_rp_states_ocean_tilde_hkt(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::VulkanDevice& device
    );

    std::unique_ptr<mirinae::IRpBase> create_rp_states_ocean_butterfly(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::VulkanDevice& device
    );

    URpStates create_rp_states_ocean_naive_ift(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

    std::unique_ptr<mirinae::IRpBase> create_rp_states_ocean_finalize(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::VulkanDevice& device
    );

    URpStates create_rp_states_ocean_tess(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::VulkanDevice& device
    );

}  // namespace mirinae::rp::ocean
