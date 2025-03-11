#pragma once

#include <memory>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/envmap.hpp"
#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::envmap {

    struct IEnvmapRenUnitVk : public mirinae::IEnvmapRenUnit {
        virtual VkImageView diffuse_view() = 0;
        virtual VkImageView specular_view() = 0;
    };


    void create_rp(
        IRenderPassRegistry& reg,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    );


    class IRpMaster : public mirinae::IRpStates {

    public:
        virtual ~IRpMaster() = default;

        virtual void init(
            CosmosSimulator& cosmos,
            IRenderPassRegistry& rp_pkg,
            RpResources& rp_res,
            DesclayoutManager& desclayouts,
            VulkanDevice& device
        ) = 0;

        virtual void destroy(VulkanDevice& device) = 0;

        virtual VkImageView brdf_lut_view() const = 0;
    };

    std::unique_ptr<IRpMaster> create_rp_master();

}  // namespace mirinae::rp::envmap
