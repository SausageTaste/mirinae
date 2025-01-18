#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::ocean {

    struct RpCreateParams {
        mirinae::VulkanDevice* device_;
        mirinae::RpResources* rp_res_;
        mirinae::DesclayoutManager* desclayouts_;
    };


    struct RpContext {
        mirinae::FrameIndex f_index;
        mirinae::ShainImageIndex i_index;
        VkCommandBuffer cmdbuf;
    };


    struct IRpStates {
        virtual ~IRpStates() = default;
        virtual void record(const RpContext& context) = 0;
        virtual const std::string& name() const = 0;
    };

    std::unique_ptr<IRpStates> create_rp_states_ocean_test(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

    std::unique_ptr<IRpStates> create_rp_states_ocean_tess(
        size_t swapchain_count,
        mirinae::FbufImageBundle& fbuf_bundle,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

}  // namespace mirinae::rp::ocean
