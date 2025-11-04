#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/shadow/bundles.hpp"

#include <entt/entity/entity.hpp>

#include "mirinae/render/mem_cinfo.hpp"
#include "mirinae/render/texture.hpp"


// ImageFbufPair
namespace mirinae {

    class ImageFbufPair {

    public:
        ImageFbufPair() = default;

        ImageFbufPair(ImageFbufPair&& rhs) {
            std::swap(layers_, rhs.layers_);
            std::swap(img_, rhs.img_);
            std::swap(view_, rhs.view_);
        }

        ImageFbufPair& operator=(ImageFbufPair&& rhs) {
            std::swap(layers_, rhs.layers_);
            std::swap(img_, rhs.img_);
            std::swap(view_, rhs.view_);
            return *this;
        }

        void init_img(uint32_t w, uint32_t h, VulkanDevice& device) {
            this->destroy(device);

            mirinae::ImageCreateInfo img_info;
            img_info.set_dimensions(w, h)
                .set_format(device.img_formats().depth_map())
                .add_usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                .add_usage_sampled()
                .set_arr_layers(4);
            img_.init(img_info.get(), device.mem_alloc());

            mirinae::ImageViewBuilder iv_builder;
            iv_builder.image(img_.image())
                .format(device.img_formats().depth_map())
                .view_type(VK_IMAGE_VIEW_TYPE_2D_ARRAY)
                .aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .base_arr_layer(0)
                .arr_layers(layers_.size());
            view_.reset(iv_builder, device);

            for (uint32_t i = 0; i < layers_.size(); ++i) {
                iv_builder.view_type(VK_IMAGE_VIEW_TYPE_2D)
                    .base_arr_layer(i)
                    .arr_layers(1);
                layers_.at(i).view_.reset(iv_builder, device);
            }
        }

        void init_fbuf(VkRenderPass render_pass, VulkanDevice& device) {
            FbufCinfo fbuf_info;
            fbuf_info.set_rp(render_pass).set_dim(img_.width(), img_.height());

            for (auto& layer : layers_) {
                fbuf_info.clear_attach().add_attach(layer.view_.get());
                layer.fbuf_.init(fbuf_info.get(), device.logi_device());
            }
        }

        void destroy(VulkanDevice& device) {
            for (auto& layer : layers_) {
                layer.fbuf_.destroy(device.logi_device());
                layer.view_.destroy(device);
            }

            img_.destroy(device.mem_alloc());
            view_.destroy(device);
        }

        VkImage img() const { return img_.image(); }

        VkImageView view_whole() const { return view_.get(); }

        VkImageView view_layer(uint32_t layer) const {
            return layers_.at(layer).view_.get();
        }

        VkFramebuffer fbuf_layer(uint32_t layer) const {
            return layers_.at(layer).fbuf_.get();
        }

        uint32_t width() const { return img_.width(); }
        uint32_t height() const { return img_.height(); }

    private:
        struct Layer {
            ImageView view_;
            Fbuf fbuf_;
        };

        std::array<Layer, 4> layers_;
        Image img_;
        ImageView view_;
    };

}  // namespace mirinae


// DlightShadowMap
namespace mirinae {

    DlightShadowMap::DlightShadowMap() : entt_(entt::null) {}

    DlightShadowMap::~DlightShadowMap() {}

    DlightShadowMap::DlightShadowMap(DlightShadowMap&& rhs) {
        std::swap(images_, rhs.images_);
        std::swap(entt_, rhs.entt_);
    }

    DlightShadowMap& DlightShadowMap::operator=(DlightShadowMap&& rhs) {
        std::swap(images_, rhs.images_);
        std::swap(entt_, rhs.entt_);
        return *this;
    }

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

    VkImageView DlightShadowMap::view_whole(FrameIndex f_idx) const {
        return images_.at(f_idx).view_whole();
    }

    VkImageView DlightShadowMap::view_layer(
        FrameIndex f_idx, uint32_t layer
    ) const {
        return images_.at(f_idx).view_layer(layer);
    }

    VkFramebuffer DlightShadowMap::fbuf(
        FrameIndex f_idx, uint32_t layer
    ) const {
        return images_.at(f_idx).fbuf_layer(layer);
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

    entt::entity ShadowMapBundle::slight_entt_at(size_t idx) const {
        return slights_.at(idx).entt_;
    }

    VkImage ShadowMapBundle::slight_img_at(size_t idx) const {
        return slights_.at(idx).tex_->image();
    }

    VkImageView ShadowMapBundle::slight_view_at(size_t idx) const {
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


namespace mirinae::rp {

    HShadowMaps create_shadow_maps_bundle(mirinae::VulkanDevice& device) {
        return std::make_shared<ShadowMapBundle>(device);
    }

}  // namespace mirinae::rp
