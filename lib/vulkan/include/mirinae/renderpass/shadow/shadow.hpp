#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp {

    HShadowMaps create_shadow_maps_bundle(mirinae::VulkanDevice& device);

    std::unique_ptr<IRpBase> create_rp_shadow_static(RpCreateBundle& cbundle);

    std::unique_ptr<IRpBase> create_rp_shadow_skinned(RpCreateBundle& cbundle);

    std::unique_ptr<IRpBase> create_rp_shadow_static_trs(RpCreateBundle& cbundle);

    std::unique_ptr<IRpBase> create_rp_shadow_skinned_trs(
        RpCreateBundle& cbundle
    );

    std::unique_ptr<IRpBase> create_rp_shadow_terrain(RpCreateBundle& cbundle);

}  // namespace mirinae::rp
