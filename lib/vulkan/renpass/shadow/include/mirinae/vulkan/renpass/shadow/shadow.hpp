#pragma once

#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae::rp {

    HShadowMaps create_shadow_maps_bundle(mirinae::VulkanDevice&);

    std::unique_ptr<IRpBase> create_rp_shadow_static(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rp_shadow_static_trs(RpCreateBundle&);
    std::unique_ptr<IRpBase> create_rp_shadow_terrain(RpCreateBundle&);

}  // namespace mirinae::rp
