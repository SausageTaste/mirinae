#pragma once

#include <memory>

#include "mirinae/cosmos.hpp"
#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::envmap {

    void create_rp(
        IRenderPassRegistry& reg,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    );


    class IRpMaster {

    public:
        virtual ~IRpMaster() = default;

        virtual void init(
            IRenderPassRegistry& rp_pkg,
            TextureManager& tex_man,
            DesclayoutManager& desclayouts,
            VulkanDevice& device
        ) = 0;

        virtual void destroy(VulkanDevice& device) = 0;

        virtual void record(
            const VkCommandBuffer cur_cmd_buf,
            const DrawSheet& draw_sheet,
            const FrameIndex frame_index,
            const CosmosSimulator& cosmos,
            const ShainImageIndex image_index,
            const IRenderPassRegistry& rp_pkg
        ) = 0;

        virtual VkImageView diffuse_view(size_t index) const = 0;
        virtual VkImageView specular_view(size_t index) const = 0;
        virtual VkImageView brdf_lut_view() const = 0;
        virtual VkImageView sky_tex_view() const = 0;

        virtual glm::dvec3& envmap_pos(size_t index) = 0;
    };

    std::unique_ptr<IRpMaster> create_rp_master();

}  // namespace mirinae::rp::envmap
