#pragma once

#include <list>
#include <map>

#include <sung/basic/angle.hpp>

#include "mirinae/math/include_glm.hpp"
#include "vkmajorplayers.hpp"


namespace mirinae {

    using Angle = sung::TAngle<double>;


    struct U_GbufModel {
        float roughness;
        float metallic;
    };


    struct U_GbufActor {
        glm::mat4 model;
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


    struct U_EnvmapPushConst {
        glm::mat4 proj_view_;
        glm::vec4 dlight_dir_;
        glm::vec4 dlight_color_;
    };


    struct U_EnvSkyPushConst {
        glm::mat4 proj_view_;
    };


    struct U_EnvdiffusePushConst {
        glm::mat4 proj_view_;
    };


    struct U_EnvSpecularPushConst {
        glm::mat4 proj_view_;
        float roughness_;
    };


    struct U_FillScreenPushConst {
        float exposure_;
        float gamma_;
    };


    class U_CompoMain {

    public:
        template <typename T>
        U_CompoMain& set_proj(const glm::tmat4x4<T>& m) {
            proj_ = m;
            proj_inv_ = glm::inverse(m);
            return *this;
        }

        template <typename T>
        U_CompoMain& set_view(const glm::tmat4x4<T>& m) {
            view_ = m;
            view_inv_ = glm::inverse(m);
            return *this;
        }

        template <typename T>
        U_CompoMain& set_dlight_mat(size_t index, const glm::tmat4x4<T>& m) {
            dlight_mats_[index] = m;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_dlight_dir(const glm::tvec3<T>& x) {
            dlight_dir_.x = x.x;
            dlight_dir_.y = x.y;
            dlight_dir_.z = x.z;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_dlight_color(T r, T g, T b) {
            dlight_color_.x = r;
            dlight_color_.y = g;
            dlight_color_.z = b;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_dlight_color(const glm::tvec3<T>& dlight_color) {
            dlight_color_.x = dlight_color.r;
            dlight_color_.y = dlight_color.g;
            dlight_color_.z = dlight_color.b;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_dlight_cascade_depths(const std::array<T, 4>& depths) {
            for (size_t i = 0; i < depths.size(); ++i)
                dlight_cascade_depths_[i] = depths[i];

            return *this;
        }

        template <typename T>
        U_CompoMain& set_slight_mat(const glm::tmat4x4<T>& m) {
            slight_mat_ = m;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_slight_pos(const glm::tvec3<T>& pos) {
            slight_pos_n_inner_angle.x = pos.x;
            slight_pos_n_inner_angle.y = pos.y;
            slight_pos_n_inner_angle.z = pos.z;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_slight_dir(const glm::tvec3<T>& x) {
            slight_dir_n_outer_angle.x = x.x;
            slight_dir_n_outer_angle.y = x.y;
            slight_dir_n_outer_angle.z = x.z;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_slight_color(const glm::tvec3<T>& v) {
            slight_color_n_max_dist.x = v.r;
            slight_color_n_max_dist.y = v.g;
            slight_color_n_max_dist.z = v.b;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_slight_inner_angle(sung::TAngle<T> angle) {
            const auto v = std::cos(angle.rad());
            slight_pos_n_inner_angle.w = static_cast<float>(v);
            return *this;
        }

        template <typename T>
        U_CompoMain& set_slight_outer_angle(sung::TAngle<T> angle) {
            const auto v = std::cos(angle.rad());
            slight_dir_n_outer_angle.w = static_cast<float>(v);
            return *this;
        }

        template <typename T>
        U_CompoMain& set_slight_max_dist(T max_dist) {
            slight_color_n_max_dist.w = max_dist;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_vpl_pos(size_t i, const glm::tvec3<T>& pos) {
            vpl_pos_n_radius[i].x = pos.x;
            vpl_pos_n_radius[i].y = pos.y;
            vpl_pos_n_radius[i].z = pos.z;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_vpl_color(size_t i, const glm::tvec3<T>& v) {
            vpl_color_n_intensity[i].x = v.r;
            vpl_color_n_intensity[i].y = v.g;
            vpl_color_n_intensity[i].z = v.b;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_fog_color(const glm::tvec3<T>& v) {
            fog_color_density_.x = v.r;
            fog_color_density_.y = v.g;
            fog_color_density_.z = v.b;
            return *this;
        }

        template <typename T>
        U_CompoMain& set_fog_density(T density) {
            fog_color_density_.w = density;
            return *this;
        }

    private:
        glm::mat4 view_;
        glm::mat4 view_inv_;
        glm::mat4 proj_;
        glm::mat4 proj_inv_;

        // Directional light
        glm::mat4 dlight_mats_[4];
        glm::vec4 dlight_dir_;
        glm::vec4 dlight_color_;
        glm::vec4 dlight_cascade_depths_;

        // Spotlight
        glm::mat4 slight_mat_;
        glm::vec4 slight_pos_n_inner_angle;
        glm::vec4 slight_dir_n_outer_angle;
        glm::vec4 slight_color_n_max_dist;

        // Volumetric Point Light
        std::array<glm::vec4, 8> vpl_pos_n_radius;
        std::array<glm::vec4, 8> vpl_color_n_intensity;

        glm::vec4 fog_color_density_;
    };


    using U_TranspFrame = U_CompoMain;


    struct U_DebugMeshPushConst {
        glm::vec4 vertices_[3];
        glm::vec4 color_;
    };


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

}  // namespace mirinae


namespace mirinae {

    class DescSizeInfo {

    public:
        DescSizeInfo operator+(const DescSizeInfo& rhs) const;
        DescSizeInfo& add(VkDescriptorType type, uint32_t cnt);
        void multiply_counts(uint32_t factor);
        std::vector<VkDescriptorPoolSize> create_arr() const;

    private:
        uint32_t get(VkDescriptorType type) const;
        void set(VkDescriptorType type, uint32_t cnt);

        std::map<VkDescriptorType, uint32_t> data_;
    };


    class DescLayout {

    public:
        DescLayout(
            const std::string& name,
            const DescSizeInfo& size_info,
            VkDescriptorSetLayout layout
        )
            : size_info_(size_info), name_(name), layout_(layout) {}

        void destroy(VkDevice logi_device) {
            if (layout_ != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(logi_device, layout_, nullptr);
                layout_ = VK_NULL_HANDLE;
            }
        }

        const DescSizeInfo& size_info() const { return size_info_; }
        const std::string& name() const { return name_; }
        VkDescriptorSetLayout layout() const { return layout_; }

    private:
        DescSizeInfo size_info_;
        std::string name_;
        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    };


    class DescLayoutBuilder {

    public:
        explicit DescLayoutBuilder(const char* name);
        explicit DescLayoutBuilder(const std::string& name);

        // Raw
        DescLayoutBuilder& new_binding();
        DescLayoutBuilder& new_binding(uint32_t idx);

        DescLayoutBuilder& set_type(VkDescriptorType type);
        DescLayoutBuilder& set_count(uint32_t cnt);
        DescLayoutBuilder& set_stage(VkShaderStageFlags stage);
        DescLayoutBuilder& add_stage(VkShaderStageFlags stage);
        DescLayoutBuilder& finish_binding();

        // Uniform buffer
        DescLayoutBuilder& add_ubuf(VkShaderStageFlags, uint32_t cnt);
        DescLayoutBuilder& add_ubuf_frag(uint32_t cnt) {
            return this->add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, cnt);
        }

        // Combined image sampler
        DescLayoutBuilder& add_img(VkShaderStageFlags, uint32_t cnt);
        DescLayoutBuilder& add_img_tesc(uint32_t cnt);
        DescLayoutBuilder& add_img_tese(uint32_t cnt);
        DescLayoutBuilder& add_img_frag(uint32_t cnt);

        // Input attachment
        DescLayoutBuilder& add_input_att(VkShaderStageFlags, uint32_t cnt);

        VkDescriptorSetLayout build(VkDevice logi_device) const;

        auto& name() const { return name_; }
        auto& size_info() const { return size_info_; }

    private:
        std::string name_;
        std::vector<VkDescriptorSetLayoutBinding> bindings_;
        DescSizeInfo size_info_;
    };


    class DesclayoutManager {

    public:
        DesclayoutManager(VulkanDevice& device);
        ~DesclayoutManager();

        VkDescriptorSetLayout add(
            const DescLayoutBuilder& builder, VkDevice logi_device
        );

        const DescLayout& get(const std::string& name) const;

    private:
        std::vector<DescLayout> data_;
        VulkanDevice& device_;
    };


    class DescWriteInfoBuilder {

    public:
        DescWriteInfoBuilder& set_descset(VkDescriptorSet descset);

        DescWriteInfoBuilder& add_ubuf(const mirinae::Buffer& buffer);
        // Combinded image sampler
        DescWriteInfoBuilder& add_img_sampler(VkImageView img, VkSampler sam);
        DescWriteInfoBuilder& add_img_sampler_general(
            VkImageView img, VkSampler sam
        );
        DescWriteInfoBuilder& add_input_attach(VkImageView image_view);
        DescWriteInfoBuilder& add_storage_img(VkImageView image_view);

        void apply_all(VkDevice logi_device);

    private:
        std::vector<VkWriteDescriptorSet> data_;
        std::list<VkDescriptorBufferInfo> buffer_info_;
        std::list<VkDescriptorImageInfo> image_info_;
        VkDescriptorSet descset_ = VK_NULL_HANDLE;
        uint32_t binding_index_ = 0;
    };


    class DescWriter {

    public:
        class BufferInfoView {

        public:
            BufferInfoView(VkDescriptorBufferInfo& info);
            BufferInfoView& set_buffer(VkBuffer buffer);
            BufferInfoView& set_range(VkDeviceSize range);
            BufferInfoView& set_offset(VkDeviceSize offset);

        private:
            VkDescriptorBufferInfo& info() { return *info_; }

            VkDescriptorBufferInfo* info_;
        };

        class ImageInfoView {

        public:
            ImageInfoView(VkDescriptorImageInfo& info);
            ImageInfoView& set_img_view(VkImageView img_view);
            ImageInfoView& set_sampler(VkSampler sam);
            ImageInfoView& set_layout(VkImageLayout layout);

        private:
            VkDescriptorImageInfo& info() { return *info_; }

            VkDescriptorImageInfo* info_;
        };

        BufferInfoView add_buf_info();
        DescWriter& add_buf_info(mirinae::Buffer& buffer);

        ImageInfoView add_img_info();
        DescWriter& add_storage_img_info(VkImageView img_view);

        DescWriter& add_buf_write(VkDescriptorSet, uint32_t binding);

        DescWriter& add_sampled_img_write(VkDescriptorSet, uint32_t binding);
        DescWriter& add_storage_img_write(VkDescriptorSet, uint32_t binding);

        void apply_all(VkDevice logi_device) const;

    private:
        std::vector<VkWriteDescriptorSet> write_info_;
        std::list<std::vector<VkDescriptorBufferInfo>> buffer_info_;
        std::list<std::vector<VkDescriptorImageInfo>> img_info_;
        size_t binding_index_ = 0;
    };


    class DescPool {

    public:
        void init(
            const uint32_t max_sets,
            const uint32_t pool_size_count,
            const VkDescriptorPoolSize* pool_sizes,
            const VkDevice logi_device
        );

        void init(
            const uint32_t max_sets,
            const DescSizeInfo& size_info,
            const VkDevice logi_device
        );

        void destroy(VkDevice logi_device);

        VkDescriptorSet alloc(
            VkDescriptorSetLayout desclayout, VkDevice logi_device
        );

        std::vector<VkDescriptorSet> alloc(
            uint32_t count,
            VkDescriptorSetLayout desclayout,
            VkDevice logi_device
        );

        VkDescriptorPool get() const { return handle_; }

    private:
        VkDescriptorPool handle_ = VK_NULL_HANDLE;
        DescSizeInfo size_info_;
    };

}  // namespace mirinae
