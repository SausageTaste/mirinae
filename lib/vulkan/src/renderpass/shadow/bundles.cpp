#include "mirinae/renderpass/shadow/bundles.hpp"

#include <entt/entity/entity.hpp>

#include "mirinae/render/texture.hpp"


// ImageFbufPair
namespace mirinae {

    class ImageFbufPair {

    public:
        void init_img(uint32_t w, uint32_t h, VulkanDevice& device) {
            tex_ = create_tex_depth(w, h, device);
        }

        void init_fbuf(VkRenderPass render_pass, VulkanDevice& device) {
            FbufCinfo fbuf_info;
            fbuf_info.set_rp(render_pass)
                .clear_attach()
                .add_attach(tex_->image_view())
                .set_dim(tex_->width(), tex_->height());
            fbuf_.init(fbuf_info.get(), device.logi_device());
        }

        void destroy(VulkanDevice& device) {
            fbuf_.destroy(device.logi_device());
            tex_.reset();
        }

        VkImage img() const { return tex_->image(); }
        VkImageView view() const { return tex_->image_view(); }
        VkFramebuffer fbuf() const { return fbuf_.get(); }

        uint32_t width() const { return tex_->width(); }
        uint32_t height() const { return tex_->height(); }

    private:
        std::unique_ptr<ITexture> tex_;
        Fbuf fbuf_;
    };

}  // namespace mirinae


// DlightShadowMap
namespace mirinae {

    DlightShadowMap::DlightShadowMap() : entt_(entt::null) {}

    void DlightShadowMap::init_images(
        const uint32_t w,
        const uint32_t h,
        const size_t frames_in_flight,
        VulkanDevice& device
    ) {
        images_.resize(frames_in_flight);
        for (auto& x : images_) {
            x.init_img(w, h, device);
        }
    }

    void DlightShadowMap::init_fbufs(
        const VkRenderPass render_pass, VulkanDevice& device
    ) {
        for (auto& x : images_) {
            x.init_fbuf(render_pass, device);
        }
    }

    void DlightShadowMap::destroy(VulkanDevice& device) {
        for (auto& x : images_) {
            x.destroy(device);
        }
        images_.clear();
        entt_ = entt::null;
    }

    VkImage DlightShadowMap::img(FrameIndex f_idx) const {
        return images_.at(f_idx).img();
    }

    VkImageView DlightShadowMap::view(FrameIndex f_idx) const {
        return images_.at(f_idx).view();
    }

    VkFramebuffer DlightShadowMap::fbuf(FrameIndex f_idx) const {
        return images_.at(f_idx).fbuf();
    }

    entt::entity DlightShadowMap::entt() const { return entt_; }

    void DlightShadowMap::set_entt(entt::entity entt) { entt_ = entt; }

    uint32_t DlightShadowMap::width() const { return images_.at(0).width(); }

    uint32_t DlightShadowMap::height() const { return images_.at(0).height(); }

    VkExtent2D DlightShadowMap::extent2d() const {
        return { this->width(), this->height() };
    }

}  // namespace mirinae


// DlightShadowMapBundle
namespace mirinae {

    void DlightShadowMapBundle::init_images(
        const size_t shadow_count,
        const size_t frames_in_flight,
        VulkanDevice& device
    ) {
        for (auto& x : dlights_) {
            x.destroy(device);
        }

        dlights_.clear();
        dlights_.resize(shadow_count);

        for (auto& x : dlights_) {
            x.init_images(2 << 11, 2 << 11, frames_in_flight, device);
        }
    }

    void DlightShadowMapBundle::init_fbufs(
        const VkRenderPass render_pass, VulkanDevice& device
    ) {
        for (auto& x : dlights_) {
            x.init_fbufs(render_pass, device);
        }
    }

    void DlightShadowMapBundle::destroy(VulkanDevice& device) {
        for (auto& x : dlights_) {
            x.destroy(device);
        }
    }

    uint32_t DlightShadowMapBundle::count() const {
        return static_cast<uint32_t>(dlights_.size());
    }

    DlightShadowMap& DlightShadowMapBundle::at(uint32_t index) {
        return dlights_.at(index);
    }

    const DlightShadowMap& DlightShadowMapBundle::at(uint32_t index) const {
        return dlights_.at(index);
    }

}  // namespace mirinae


// ShadowMapBundle :: Item
namespace mirinae {

    ShadowMapBundle::Item::Item() : entt_(entt::null) {}

    uint32_t ShadowMapBundle::Item::width() const { return tex_->width(); }

    uint32_t ShadowMapBundle::Item::height() const { return tex_->height(); }

    VkFramebuffer ShadowMapBundle::Item::fbuf() const { return fbuf_.get(); }
}  // namespace mirinae


// ShadowMapBundle
namespace mirinae {

    ShadowMapBundle::ShadowMapBundle(VulkanDevice& device) : device_(device) {
        dlights_.init_images(2, MAX_FRAMES_IN_FLIGHT, device);

        slights_.emplace_back().tex_ = create_tex_depth(512, 512, device);
        slights_.emplace_back().tex_ = create_tex_depth(512, 512, device);
        slights_.emplace_back().tex_ = create_tex_depth(512, 512, device);
    }

    ShadowMapBundle::~ShadowMapBundle() {
        dlights_.destroy(device_);

        for (auto& x : slights_) {
            x.fbuf_.destroy(device_.logi_device());
        }
    }

    DlightShadowMapBundle& ShadowMapBundle::dlights() { return dlights_; }

    const DlightShadowMapBundle& ShadowMapBundle::dlights() const {
        return dlights_;
    }

    uint32_t ShadowMapBundle::slight_count() const {
        return static_cast<uint32_t>(slights_.size());
    }

    entt::entity ShadowMapBundle::slight_entt_at(size_t idx) {
        return slights_.at(idx).entt_;
    }

    VkImage ShadowMapBundle::slight_img_at(size_t idx) {
        return slights_.at(idx).tex_->image();
    }

    VkImageView ShadowMapBundle::slight_view_at(size_t idx) {
        return slights_.at(idx).tex_->image_view();
    }

    void ShadowMapBundle::recreate_fbufs(
        const VkRenderPass rp, VulkanDevice& device
    ) {
        dlights_.init_fbufs(rp, device);

        FbufCinfo fbuf_info;
        fbuf_info.set_rp(rp);

        for (auto& x : slights_) {
            fbuf_info.clear_attach()
                .add_attach(x.tex_->image_view())
                .set_dim(x.width(), x.height());
            x.fbuf_.init(fbuf_info.get(), device.logi_device());
        }
    }

}  // namespace mirinae
