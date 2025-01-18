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

        virtual bool init(RpCreateParams& params) = 0;
        virtual void destroy(RpResources& rp_res, VulkanDevice& device) = 0;
        virtual void record(const RpContext& context) = 0;

        virtual const std::string& name() const = 0;
    };

    std::unique_ptr<IRpStates> create_rp_states_ocean_test();

}  // namespace mirinae::rp::ocean
