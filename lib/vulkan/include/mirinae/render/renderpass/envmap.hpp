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
            IRenderPassRegistry& rp_pkg,
            ITextureManager& tex_man,
            DesclayoutManager& desclayouts,
            VulkanDevice& device
        ) = 0;

        virtual void destroy(VulkanDevice& device) = 0;

        virtual size_t init_ren_units(
            CosmosSimulator& cosmos,
            DesclayoutManager& desclayouts,
            IRenderPassRegistry& rp_pkg,
            VulkanDevice& device
        ) = 0;

        virtual void destroy_ren_units(CosmosSimulator& cosmos) = 0;

        virtual VkImageView brdf_lut_view() const = 0;
        virtual VkImageView sky_tex_view() const = 0;
    };

    std::unique_ptr<IRpMaster> create_rp_master();

}  // namespace mirinae::rp::envmap
