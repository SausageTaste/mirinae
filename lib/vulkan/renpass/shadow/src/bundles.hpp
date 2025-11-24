#pragma once

#include <vector>

#include <entt/fwd.hpp>

#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae {

    class ITexture;
    class ImageFbufPair;


    class DlightShadowMap : public IShadowMapBundle::IDlightShadowMap {

    public:
        DlightShadowMap();
        ~DlightShadowMap();

        DlightShadowMap(DlightShadowMap&& rhs);
        DlightShadowMap& operator=(DlightShadowMap&& rhs);

        void init_images(
            const uint32_t w,
            const uint32_t h,
            const size_t frames_in_flight,
            VulkanDevice& device
        );

        void init_fbufs(const VkRenderPass render_pass, VulkanDevice& device);
        void destroy(VulkanDevice& device);

        VkImage img(FrameIndex f_idx) const override;
        VkImageView view_whole(FrameIndex f_idx) const override;
        VkImageView view_layer(FrameIndex f_idx, uint32_t layer) const override;
        VkFramebuffer fbuf(FrameIndex f_idx, uint32_t layer) const override;

        entt::entity entt() const override;
        void set_entt(entt::entity entt);

        uint32_t width() const;
        uint32_t height() const;
        VkExtent2D extent2d() const;

    private:
        std::vector<ImageFbufPair> images_;
        entt::entity entt_;
    };


    class DlightShadowMapBundle
        : public IShadowMapBundle::IDlightShadowMapBundle {

    public:
        void init_images(
            const size_t shadow_count,
            const size_t frames_in_flight,
            VulkanDevice& device
        );

        void init_fbufs(const VkRenderPass render_pass, VulkanDevice& device);
        void destroy(VulkanDevice& device);

        uint32_t count() const override;
        DlightShadowMap& at(uint32_t index) override;
        const DlightShadowMap& at(uint32_t index) const override;

    private:
        std::vector<DlightShadowMap> dlights_;
    };


    class ShadowMapBundle : public IShadowMapBundle {

    public:
        struct Item {
            Item();
            uint32_t width() const;
            uint32_t height() const;
            VkFramebuffer fbuf() const;

            std::unique_ptr<ITexture> tex_;
            Fbuf fbuf_;
            entt::entity entt_;
        };

    public:
        ShadowMapBundle(VulkanDevice& device);
        ~ShadowMapBundle();

        DlightShadowMapBundle& dlights() override;
        const DlightShadowMapBundle& dlights() const override;

        uint32_t slight_count() const override;
        entt::entity slight_entt_at(size_t idx) const override;
        VkImage slight_img_at(size_t idx) const override;
        VkImageView slight_view_at(size_t idx) const override;

        void recreate_fbufs(const VkRenderPass rp, VulkanDevice& device);

    private:
        VulkanDevice& device_;
        DlightShadowMapBundle dlights_;

    public:
        std::vector<Item> slights_;
    };


}  // namespace mirinae
