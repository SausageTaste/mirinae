#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae {

    std::unique_ptr<IRenPass> create_rp_base(
        DesclayoutManager& desclayouts, VulkanDevice& device
    );

    std::unique_ptr<IRenPass> create_rp_diffuse(
        DesclayoutManager& desclayouts, VulkanDevice& device
    );

    std::unique_ptr<IRenPass> create_rp_specular(
        DesclayoutManager& desclayouts, VulkanDevice& device
    );

    std::unique_ptr<IRenPass> create_rp_sky(
        DesclayoutManager& desclayouts, VulkanDevice& device
    );

    std::unique_ptr<IRenPass> create_rp_brdf_lut(
        DesclayoutManager& desclayouts, VulkanDevice& device
    );

}
