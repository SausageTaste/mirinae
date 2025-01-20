#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::ocean {

    struct RpCreateParams {
        mirinae::VulkanDevice* device_;
        mirinae::RpResources* rp_res_;
        mirinae::DesclayoutManager* desclayouts_;
    };


    struct RpContext {
        mirinae::FrameIndex f_index_;
        mirinae::ShainImageIndex i_index_;
        glm::dmat4 proj_mat_;
        glm::dmat4 view_mat_;
        VkCommandBuffer cmdbuf_;
    };


    struct IRpStates {
        virtual ~IRpStates() = default;
        virtual void record(const RpContext& context) = 0;
        virtual const std::string& name() const = 0;
    };

    using URpStates = std::unique_ptr<IRpStates>;


    URpStates create_rp_states_ocean_tilde_h(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
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

    URpStates create_rp_states_ocean_tess(
        size_t swapchain_count,
        mirinae::FbufImageBundle& fbuf_bundle,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

}  // namespace mirinae::rp::ocean
