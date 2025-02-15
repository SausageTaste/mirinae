#pragma once

#include "mirinae/cpnt/light.hpp"
#include "mirinae/math/mamath.hpp"
#include "mirinae/render/renderpass/common.hpp"
#include "mirinae/scene/scene.hpp"


namespace mirinae::rp::shadow {

    void create_rp(
        IRenderPassRegistry& reg,
        VkFormat depth_format,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    );


    class RpMaster {

    public:
        class CascadeInfo {

        public:
            struct Cascade {
                std::array<glm::dvec3, 8> frustum_verts_;
                glm::dmat4 light_mat_;
                double near_;
                double far_;
            };

            void update(
                const double ratio,
                const glm::dmat4& view_inv,
                const PerspectiveCamera<double>& pers,
                const cpnt::DLight& dlight,
                const cpnt::DLight::Tform& tform
            );

            std::array<Cascade, 4> cascades_;
            std::array<double, 4> far_depths_;

        private:
            static void make_frustum_vertices(
                const double screen_ratio,
                const double plane_dist,
                const Angle fov,
                const glm::dmat4& view_inv,
                glm::dvec3* const out
            );

            static std::array<double, 5> make_plane_distances(
                const double p_near, const double p_far
            );

            double calc_clip_depth(double z, double n, double f);
        };


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


}  // namespace mirinae::rp::shadow
