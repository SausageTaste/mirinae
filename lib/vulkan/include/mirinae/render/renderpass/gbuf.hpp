#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::gbuf {

    struct U_GbufTerrainPushConst {
        glm::mat4 proj_;
        glm::mat4 view_;
        glm::mat4 model_;
        glm::vec4 tile_index_count_;
        glm::vec4 height_map_size_;
        float height_scale_;
    };


    void create_rp(
        IRenderPassRegistry& reg,
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    );


    class IRpMasterBasic {

    public:
        virtual ~IRpMasterBasic() = default;

        virtual void init() = 0;
        virtual void destroy(VulkanDevice& device) = 0;

        virtual void record(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_exd,
            const DrawSheet& draw_sheet,
            const FrameIndex frame_index,
            const ShainImageIndex image_index,
            const IRenderPassRegistry& rp_pkg
        ) = 0;
    };

    std::unique_ptr<IRpMasterBasic> create_rpm_basic();


    class IRpMasterTerrain {

    public:
        virtual ~IRpMasterTerrain() = default;

        virtual void init(
            mirinae::TextureManager& tex_man,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        ) = 0;

        virtual void destroy(VulkanDevice& device) = 0;

        virtual void record(
            const VkCommandBuffer cmdbuf,
            const glm::mat4& proj_mat,
            const glm::mat4& view_mat,
            const glm::mat4& model_mat,
            const VkExtent2D& fbuf_exd,
            const mirinae::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::IRenderPassRegistry& rp_pkg
        ) = 0;
    };

    std::unique_ptr<IRpMasterTerrain> create_rpm_terrain();

}  // namespace mirinae::rp::gbuf
