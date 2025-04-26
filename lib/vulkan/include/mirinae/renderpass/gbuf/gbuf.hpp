#pragma once

#include "mirinae/render/render_graph.hpp"
#include "mirinae/renderpass/common.hpp"


namespace mirinae::rp::gbuf {

    void create_desc_layouts(
        DesclayoutManager& desclayouts, VulkanDevice& device
    );


    std::unique_ptr<IRpBase> create_rp_gbuf_static(RpCreateBundle& cbundle);
    std::unique_ptr<IRpBase> create_rp_gbuf_skinned(RpCreateBundle& cbundle);

    URpStates create_rp_states_gbuf_terrain(
        RpResources& rp_res,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );

}  // namespace mirinae::rp::gbuf


namespace mirinae::rp {

    rg::URpImpl create_rpimpl_gbuf_static();

    rg::URpImpl create_rpimpl_gbuf_skinned();

    rg::URpImpl create_rpimpl_gbuf_terrain();

}  // namespace mirinae::rp
