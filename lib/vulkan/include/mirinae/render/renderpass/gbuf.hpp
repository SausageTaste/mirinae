#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::gbuf {

    void create_rp(
        IRenderPassRegistry& reg,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );


    URpStates create_rp_states_gbuf(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::Swapchain& swapchain,
        mirinae::VulkanDevice& device
    );


    class IRpMasterTerrain {

    public:
        virtual ~IRpMasterTerrain() = default;

        virtual void init(
            ITextureManager& tex_man,
            DesclayoutManager& desclayouts,
            VulkanDevice& device
        ) = 0;

        virtual void init_ren_units(
            CosmosSimulator& cosmos,
            ITextureManager& tex_man,
            DesclayoutManager& desclayouts,
            VulkanDevice& device
        ) = 0;

        virtual void destroy(VulkanDevice& device) = 0;

        virtual void record(
            RpContext& ctxt,
            const VkExtent2D& fbuf_exd,
            const IRenderPassRegistry& rp_pkg
        ) = 0;
    };

    std::unique_ptr<IRpMasterTerrain> create_rpm_terrain();

}  // namespace mirinae::rp::gbuf
