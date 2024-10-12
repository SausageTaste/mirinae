#pragma once

#include "mirinae/render/renderpass/common.hpp"


namespace mirinae::rp::gbuf {

    class U_GbufTerrainPushConst {

    public:
        U_GbufTerrainPushConst& pvm(
            const glm::dmat4& proj,
            const glm::dmat4& view,
            const glm::dmat4& model
        ) {
            pvm_ = proj * view * model;
            view_ = view;
            model_ = model;
            return *this;
        }

        U_GbufTerrainPushConst& tile_index(int x, int y) {
            tile_index_count_.x = static_cast<float>(x);
            tile_index_count_.y = static_cast<float>(y);
            return *this;
        }

        U_GbufTerrainPushConst& tile_count(int x, int y) {
            tile_index_count_.z = static_cast<float>(x);
            tile_index_count_.w = static_cast<float>(y);
            return *this;
        }

        U_GbufTerrainPushConst& height_map_size(uint32_t x, uint32_t y) {
            height_map_size_fbuf_size_.x = static_cast<float>(x);
            height_map_size_fbuf_size_.y = static_cast<float>(y);
            return *this;
        }

        U_GbufTerrainPushConst& fbuf_size(const VkExtent2D& x) {
            height_map_size_fbuf_size_.z = static_cast<float>(x.width);
            height_map_size_fbuf_size_.w = static_cast<float>(x.height);
            return *this;
        }

        U_GbufTerrainPushConst& height_scale(float x) {
            height_scale_ = x;
            return *this;
        }

    private:
        glm::mat4 pvm_;
        glm::mat4 view_;
        glm::mat4 model_;
        glm::vec4 tile_index_count_;
        glm::vec4 height_map_size_fbuf_size_;
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
            const glm::dmat4& proj_mat,
            const glm::dmat4& view_mat,
            const VkExtent2D& fbuf_exd,
            const mirinae::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::IRenderPassRegistry& rp_pkg
        ) = 0;
    };

    std::unique_ptr<IRpMasterTerrain> create_rpm_terrain();

}  // namespace mirinae::rp::gbuf
