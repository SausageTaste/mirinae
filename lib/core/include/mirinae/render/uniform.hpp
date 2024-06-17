#pragma once

#include <list>

#include <sung/general/angle.hpp>

#include "mirinae/util/include_glm.hpp"
#include "vkmajorplayers.hpp"


namespace mirinae {

    using Angle = sung::TAngle<double>;


    struct U_GbufModel {
        float roughness;
        float metallic;
    };


    struct U_GbufActor {
        glm::mat4 view_model;
        glm::mat4 pvm;
    };


    constexpr uint32_t MAX_JOINTS = 256;

    struct U_GbufActorSkinned {
        glm::mat4 joint_transforms_[MAX_JOINTS];
        glm::mat4 view_model;
        glm::mat4 pvm;
    };

    static_assert(sizeof(U_GbufActorSkinned) < 65536);


    struct U_ShadowPushConst {
        glm::mat4 pvm_;
    };


    class U_CompoMain {

    public:
        void set_proj_inv(const glm::mat4& m) { proj_inv_ = m; }
        void set_view_inv(const glm::mat4& m) { view_inv_ = m; }

        // It automatically normalize the input vector.
        void set_dlight_mat(const glm::mat4& m) { dlight_mat_ = m; }
        void set_dlight_dir(const glm::vec3& dlight_dir) {
            const auto d = glm::normalize(dlight_dir);
            dlight_dir_.x = d.x;
            dlight_dir_.y = d.y;
            dlight_dir_.z = d.z;
        }
        void set_dlight_color(float r, float g, float b) {
            dlight_color_.x = r;
            dlight_color_.y = g;
            dlight_color_.z = b;
        }
        void set_dlight_color(float v) {
            dlight_color_.x = v;
            dlight_color_.y = v;
            dlight_color_.z = v;
        }
        void set_dlight_color(const glm::vec3& dlight_color) {
            dlight_color_.x = dlight_color.r;
            dlight_color_.y = dlight_color.g;
            dlight_color_.z = dlight_color.b;
        }

        void set_slight_mat(const glm::mat4& m) { slight_mat_ = m; }
        void set_slight_pos(const glm::vec3& pos) {
            slight_pos_n_inner_angle.x = pos.x;
            slight_pos_n_inner_angle.y = pos.y;
            slight_pos_n_inner_angle.z = pos.z;
        }
        void set_slight_dir(const glm::vec3& dir) {
            const auto d = glm::normalize(dir);
            slight_dir_n_outer_angle.x = d.x;
            slight_dir_n_outer_angle.y = d.y;
            slight_dir_n_outer_angle.z = d.z;
        }
        void set_slight_color(const glm::vec3& v) {
            slight_color_n_max_dist.x = v.r;
            slight_color_n_max_dist.y = v.g;
            slight_color_n_max_dist.z = v.b;
        }
        void set_slight_color(float v) {
            slight_color_n_max_dist.x = v;
            slight_color_n_max_dist.y = v;
            slight_color_n_max_dist.z = v;
        }
        void set_slight_inner_angle(Angle angle) {
            const auto v = std::cos(angle.rad());
            slight_pos_n_inner_angle.w = static_cast<float>(v);
        }
        void set_slight_outer_angle(Angle angle) {
            const auto v = std::cos(angle.rad());
            slight_dir_n_outer_angle.w = static_cast<float>(v);
        }
        void set_slight_max_dist(float max_dist) {
            slight_color_n_max_dist.w = max_dist;
        }

    private:
        glm::mat4 proj_inv_;
        glm::mat4 view_inv_;

        // Directional light
        glm::mat4 dlight_mat_;
        glm::vec4 dlight_dir_;
        glm::vec4 dlight_color_;

        // Spotlight
        glm::mat4 slight_mat_;
        glm::vec4 slight_pos_n_inner_angle;
        glm::vec4 slight_dir_n_outer_angle;
        glm::vec4 slight_color_n_max_dist;
    };


    using U_TranspFrame = U_CompoMain;


    struct U_OverlayPushConst {
        glm::vec4 color{ 1, 1, 1, 1 };
        glm::vec2 pos_offset;
        glm::vec2 pos_scale;
        glm::vec2 uv_offset;
        glm::vec2 uv_scale;
    };


    class U_OverlayMain {

    public:
        template <typename T>
        void set(T x_offset, T y_offset, T x_size, T y_size) {
            offset_.x = static_cast<float>(x_offset);
            offset_.y = static_cast<float>(y_offset);
            size_.x = static_cast<float>(x_size);
            size_.y = static_cast<float>(y_size);
        }

    private:
        glm::vec2 size_;
        glm::vec2 offset_;
    };
    static_assert(sizeof(U_OverlayMain) == 16);


    class DesclayoutManager {

    public:
        DesclayoutManager(VulkanDevice& device);
        ~DesclayoutManager();

        void add(const std::string& name, VkDescriptorSetLayout handle);
        VkDescriptorSetLayout get(const std::string& name);

    private:
        class Item;
        std::vector<Item> data_;
        VulkanDevice& device_;
    };


    class DescWriteInfoBuilder {

    public:
        DescWriteInfoBuilder& set_descset(VkDescriptorSet descset);

        DescWriteInfoBuilder& add_ubuf(const mirinae::Buffer& buffer);
        // Combinded image sampler
        DescWriteInfoBuilder& add_img_sampler(VkImageView img, VkSampler sam);
        DescWriteInfoBuilder& add_input_attach(VkImageView image_view);

        void apply_all(VkDevice logi_device);

    private:
        std::vector<VkWriteDescriptorSet> data_;
        std::list<VkDescriptorBufferInfo> buffer_info_;
        std::list<VkDescriptorImageInfo> image_info_;
        VkDescriptorSet descset_ = VK_NULL_HANDLE;
        uint32_t binding_index_ = 0;
    };


    class DescriptorPool {

    public:
        void init(uint32_t pool_size, VkDevice logi_device);
        void destroy(VkDevice logi_device);

        std::vector<VkDescriptorSet> alloc(
            uint32_t count,
            VkDescriptorSetLayout desclayout,
            VkDevice logi_device
        );

    private:
        VkDescriptorPool handle_ = VK_NULL_HANDLE;
    };

}  // namespace mirinae
