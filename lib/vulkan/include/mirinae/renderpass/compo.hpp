#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp::compo {

    std::unique_ptr<IRpBase> create_rps_dlight(RpCreateBundle& cbundle);

    std::unique_ptr<IRpBase> create_rps_slight(RpCreateBundle& cbundle);

    std::unique_ptr<IRpBase> create_rps_envmap(RpCreateBundle& cbundle);

    URpStates create_rps_sky(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

}  // namespace mirinae::rp::compo
