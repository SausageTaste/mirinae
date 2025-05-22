#pragma once

#include "mirinae/renderpass/common.hpp"


namespace mirinae {

    struct IEnvmapRpBundle {
        virtual ~IEnvmapRpBundle() = default;
        virtual const IRenPass& rp_base() const = 0;
        virtual const IRenPass& rp_sky() const = 0;
        virtual const IRenPass& rp_diffuse() const = 0;
        virtual const IRenPass& rp_specular() const = 0;
        virtual const IRenPass& rp_brdf_lut() const = 0;
    };


    class ColorCubeMap {

    public:
        bool init(
            uint32_t width,
            uint32_t height,
            IEnvmapRpBundle& rp_pkg,
            VulkanDevice& device
        );

        void destroy(VulkanDevice& device);

        uint32_t width() const;
        uint32_t height() const;
        VkExtent2D extent2d() const;

        VkFramebuffer face_fbuf(size_t index) const;
        VkImageView face_view(size_t index) const;
        VkImageView cube_view() const;
        VkImage cube_img() const;

    private:
        Image img_;
        ImageView cubemap_view_;
        std::array<ImageView, 6> face_views_;
        std::array<Fbuf, 6> fbufs_;
    };


    class ColorCubeMapWithMips {

    public:
        struct FaceData {
            void destroy(VulkanDevice& device);

            ImageView view_;
            Fbuf fbuf_;
        };

        struct MipData {
            void destroy(VulkanDevice& device);
            VkExtent2D extent2d() const;

            std::array<FaceData, 6> faces_;
            float roughness_ = 0.0;
            uint32_t width_ = 0;
            uint32_t height_ = 0;
        };

    public:
        bool init(
            uint32_t base_width,
            uint32_t base_height,
            IEnvmapRpBundle& rp_pkg,
            VulkanDevice& device
        );

        void destroy(VulkanDevice& device);

        VkImage cube_img() const;
        VkImageView cube_view() const;
        uint32_t base_width() const;
        uint32_t base_height() const;
        uint32_t mip_levels() const;
        const std::vector<MipData>& mips() const { return mips_; }

    private:
        Image img_;
        ImageView cubemap_view_;
        std::vector<MipData> mips_;
    };


    class ColorDepthCubeMap {

    public:
        bool init(
            uint32_t width,
            uint32_t height,
            IEnvmapRpBundle& rp_pkg,
            VulkanDevice& device
        );
        void destroy(VulkanDevice& device);

        uint32_t width() const;
        uint32_t height() const;
        VkExtent2D extent2d() const;

        VkFramebuffer face_fbuf(size_t index) const;
        VkImageView face_view(size_t index) const;
        VkImageView cube_view() const;

    public:
        Image img_;
        Semaphore semaphores_;

    private:
        std::unique_ptr<ITexture> depth_map_;
        ImageView cubemap_view_;
        std::array<ImageView, 6> face_views_;
        std::array<ImageView, 6> fbuf_face_views_;
        std::array<Fbuf, 6> fbufs_;
    };


    class CubeMap {

    public:
        bool init(
            IEnvmapRpBundle& rp_pkg,
            DescPool& desc_pool,
            DesclayoutManager& desclayouts,
            VulkanDevice& device
        );
        void destroy(VulkanDevice& device);

        const ColorDepthCubeMap& base() const { return base_; }
        const ColorCubeMap& diffuse() const { return diffuse_; }
        const ColorCubeMapWithMips& specular() const { return specular_; }
        VkDescriptorSet desc_set() const { return desc_set_; }

        glm::dvec3 world_pos_;

    private:
        ColorDepthCubeMap base_;
        ColorCubeMap diffuse_;
        ColorCubeMapWithMips specular_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    };


    class BrdfLut {

    public:
        bool init(
            uint32_t width,
            uint32_t height,
            IEnvmapRpBundle& rp_pkg,
            VulkanDevice& device
        );

        void destroy(VulkanDevice& device);
        VkImageView view() const;

    private:
        void record_drawing(
            const VkCommandBuffer cmdbuf,
            const Fbuf& fbuf,
            const IEnvmapRpBundle& rp_pkg
        );

        Image img_;
        ImageView view_;
    };


    class EnvmapBundle : public IEnvmapBundle {

    public:
        struct Item {
            Item();
            glm::dvec3 world_pos() const;

            CubeMap cube_map_;
            sung::MonotonicRealtimeTimer timer_;
            glm::dmat4 world_mat_;
            entt::entity entity_;
        };

        EnvmapBundle(IEnvmapRpBundle& rp_pkg, VulkanDevice& device);
        ~EnvmapBundle() override;

        uint32_t count() const override;
        glm::dvec3 pos_at(uint32_t index) const override;
        VkImageView diffuse_at(uint32_t index) const override;
        VkImageView specular_at(uint32_t index) const override;
        VkImageView brdf_lut() const override;

        void add(
            IEnvmapRpBundle& rp_pkg,
            DescPool& desc_pool,
            DesclayoutManager& desclayouts
        );
        void destroy();

        auto begin() { return items_.begin(); }
        auto begin() const { return items_.begin(); }
        auto end() { return items_.end(); }
        auto end() const { return items_.end(); }

        bool has_entt(entt::entity e) const;
        const Item* choose_to_update() const;

    private:
        VulkanDevice& device_;
        std::vector<Item> items_;
        BrdfLut brdf_lut_;
    };

}  // namespace mirinae
