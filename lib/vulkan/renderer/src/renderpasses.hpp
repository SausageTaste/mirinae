#pragma once

#include <functional>

#include "mirinae/vulkan/base/render/uniform.hpp"
#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae {

    HShadowMaps create_shadow_maps_bundle(VulkanDevice&);

    void create_gbuf_desc_layouts(DesclayoutManager&, VulkanDevice&);


    using RpFactoryFunc =
        std::function<std::unique_ptr<IRpBase>(RpCreateBundle&)>;

    std::vector<RpFactoryFunc> get_rp_factories();

}  // namespace mirinae
