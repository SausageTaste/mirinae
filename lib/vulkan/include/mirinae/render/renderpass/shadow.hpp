#pragma once

#include "mirinae/cpnt/light.hpp"
#include "mirinae/math/mamath.hpp"
#include "mirinae/render/renderpass/common.hpp"
#include "mirinae/scene/scene.hpp"


namespace mirinae::rp::shadow {

    /*
    void create_rp(
        IRenderPassRegistry& reg,
        VkFormat depth_format,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    );
    */


    class RpMaster {

    public:
        class ShadowMapPool {

        public:
            struct Item {
                auto width() const { return tex_->width(); }
                auto height() const { return tex_->height(); }
                VkFramebuffer fbuf() { return fbuf_.get(); }

                std::unique_ptr<ITexture> tex_;
                Fbuf fbuf_;
                glm::dmat4 mat_;
            };

            size_t size() const { return shadow_maps_.size(); }

            auto begin() { return shadow_maps_.begin(); }
            auto end() { return shadow_maps_.end(); }

            Item& at(size_t index);
            VkImageView get_img_view_at(size_t index) const;

            void add(uint32_t width, uint32_t height, VulkanDevice& device);

            void recreate_fbufs(
                const IRenderPassBundle& rp, VulkanDevice& device
            );
            void destroy_fbufs(VulkanDevice& device);

        private:
            std::vector<Item> shadow_maps_;
        };


        void record(
            const VkCommandBuffer cur_cmd_buf,
            const DrawSheet& draw_sheet,
            const FrameIndex frame_index,
            const IRenderPassRegistry& rp_pkg
        );

        ShadowMapPool& pool() { return shadow_maps_; }
        CascadeInfo& cascade() { return cascade_info_; }

    private:
        void record_static(
            const VkCommandBuffer cur_cmd_buf,
            const DrawSheet& draw_sheet,
            const FrameIndex frame_index,
            const IRenderPassRegistry& rp_pkg
        );

        void record_skinned(
            const VkCommandBuffer cur_cmd_buf,
            const DrawSheet& draw_sheet,
            const FrameIndex frame_index,
            const IRenderPassRegistry& rp_pkg
        );

        void record_skin_transp(
            const VkCommandBuffer cur_cmd_buf,
            const DrawSheet& draw_sheet,
            const FrameIndex frame_index,
            const IRenderPassRegistry& rp_pkg
        );

        ShadowMapPool shadow_maps_;
        CascadeInfo cascade_info_;
    };


    HShadowMaps create_shadow_maps_bundle(mirinae::VulkanDevice& device);

    URpStates create_rp_states_shadow_static(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

    URpStates create_rp_states_shadow_skinned(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    );

}  // namespace mirinae::rp::shadow
