#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp {

    URpStates create_rp_states_transp_skinned(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

}  // namespace mirinae::rp
