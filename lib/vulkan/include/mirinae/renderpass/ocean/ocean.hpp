#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp::ocean {

    struct RpCreateParams {
        mirinae::VulkanDevice* device_;
        mirinae::RpResources* rp_res_;
        mirinae::DesclayoutManager* desclayouts_;
    };


    std::unique_ptr<mirinae::IRpBase> create_rp_states_ocean_tilde_h(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::VulkanDevice& device
    );

    URpStates create_rp_states_ocean_tilde_hkt(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

    URpStates create_rp_states_ocean_butterfly(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

    URpStates create_rp_states_ocean_naive_ift(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

    URpStates create_rp_states_ocean_finalize(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

    URpStates create_rp_states_ocean_tess(
        size_t swapchain_count,
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

}  // namespace mirinae::rp::ocean
