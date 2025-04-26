#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/envmap/cubemap.hpp"

#include <entt/entity/entity.hpp>

#include "mirinae/render/cmdbuf.hpp"


// ColorCubeMap
namespace mirinae {

    bool ColorCubeMap::init(
        uint32_t width,
        uint32_t height,
        IEnvmapRpBundle& rp_pkg,
        VulkanDevice& device
    ) {
        ImageCreateInfo cinfo;
        cinfo.set_format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
            .set_dimensions(width, height)
            .set_mip_levels(1)
            .set_arr_layers(6)
            .add_usage_sampled()
            .add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .add_flag(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        img_.init(cinfo.get(), device.mem_alloc());

        ImageViewBuilder iv_builder;
        iv_builder.view_type(VK_IMAGE_VIEW_TYPE_CUBE)
            .format(img_.format())
            .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
            .arr_layers(6)
            .image(img_.image());
        cubemap_view_.reset(iv_builder, device);

        iv_builder.view_type(VK_IMAGE_VIEW_TYPE_2D).arr_layers(1);
        for (uint32_t i = 0; i < 6; i++) {
            iv_builder.base_arr_layer(i);
            face_views_[i].reset(iv_builder, device);
        }

        for (uint32_t i = 0; i < 6; i++) {
            FbufCinfo fbuf_cinfo;
            fbuf_cinfo.set_rp(rp_pkg.rp_diffuse().render_pass())
                .add_attach(face_views_[i].get())
                .set_dim(width, height);
            fbufs_[i].init(fbuf_cinfo.get(), device.logi_device());
        }

        return true;
    }

    void ColorCubeMap::destroy(VulkanDevice& device) {
        for (auto& x : fbufs_) x.destroy(device.logi_device());

        cubemap_view_.destroy(device);
        for (auto& x : face_views_) x.destroy(device);

        img_.destroy(device.mem_alloc());
    }

    uint32_t ColorCubeMap::width() const { return img_.width(); }

    uint32_t ColorCubeMap::height() const { return img_.height(); }

    VkExtent2D ColorCubeMap::extent2d() const { return img_.extent2d(); }

    VkFramebuffer ColorCubeMap::face_fbuf(size_t index) const {
        return fbufs_.at(index).get();
    }

    VkImageView ColorCubeMap::face_view(size_t index) const {
        return face_views_.at(index).get();
    }

    VkImageView ColorCubeMap::cube_view() const { return cubemap_view_.get(); }

    VkImage ColorCubeMap::cube_img() const { return img_.image(); }

}  // namespace mirinae


// ColorCubeMapWithMips
namespace mirinae {

    void ColorCubeMapWithMips::FaceData::destroy(VulkanDevice& device) {
        view_.destroy(device);
        fbuf_.destroy(device.logi_device());
    }


    void ColorCubeMapWithMips::MipData::destroy(VulkanDevice& device) {
        for (auto& x : faces_) x.destroy(device);
    }

    VkExtent2D ColorCubeMapWithMips::MipData::extent2d() const {
        VkExtent2D out;
        out.width = width_;
        out.height = height_;
        return out;
    }


    bool ColorCubeMapWithMips::init(
        uint32_t base_width,
        uint32_t base_height,
        IEnvmapRpBundle& rp_pkg,
        VulkanDevice& device
    ) {
        constexpr uint32_t MAX_MIP_LEVELS = 4;

        ImageCreateInfo cinfo;
        cinfo.set_format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
            .set_dimensions(base_width, base_height)
            .deduce_mip_levels()
            .set_arr_layers(6)
            .add_usage_sampled()
            .add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .add_flag(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        if (cinfo.mip_levels() > MAX_MIP_LEVELS)
            cinfo.set_mip_levels(MAX_MIP_LEVELS);
        img_.init(cinfo.get(), device.mem_alloc());

        ImageViewBuilder iv_builder;
        iv_builder.view_type(VK_IMAGE_VIEW_TYPE_CUBE)
            .format(img_.format())
            .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
            .arr_layers(6)
            .mip_levels(img_.mip_levels())
            .image(img_.image());
        cubemap_view_.reset(iv_builder, device);

        iv_builder.view_type(VK_IMAGE_VIEW_TYPE_2D).arr_layers(1).mip_levels(1);

        mips_.resize(img_.mip_levels());
        for (uint32_t lvl = 0; lvl < img_.mip_levels(); ++lvl) {
            auto& mip = mips_[lvl];

            iv_builder.base_mip_level(lvl);
            mip.roughness_ = static_cast<float>(lvl) / (img_.mip_levels() - 1);
            mip.width_ = img_.width() >> lvl;
            mip.height_ = img_.height() >> lvl;

            for (uint32_t face_i = 0; face_i < 6; ++face_i) {
                auto& face = mip.faces_[face_i];

                iv_builder.base_arr_layer(face_i);
                face.view_.reset(iv_builder, device);

                FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(rp_pkg.rp_diffuse().render_pass())
                    .add_attach(face.view_.get())
                    .set_dim(mip.width_, mip.height_);
                face.fbuf_.init(fbuf_cinfo.get(), device.logi_device());
            }
        }

        return true;
    }

    void ColorCubeMapWithMips::destroy(VulkanDevice& device) {
        for (auto& mip : mips_) mip.destroy(device);

        cubemap_view_.destroy(device);
        img_.destroy(device.mem_alloc());
    }

    VkImage ColorCubeMapWithMips::cube_img() const { return img_.image(); }

    VkImageView ColorCubeMapWithMips::cube_view() const {
        return cubemap_view_.get();
    }

    uint32_t ColorCubeMapWithMips::base_width() const { return img_.width(); }

    uint32_t ColorCubeMapWithMips::base_height() const { return img_.height(); }

    uint32_t ColorCubeMapWithMips::mip_levels() const {
        return static_cast<uint32_t>(mips_.size());
    }

}  // namespace mirinae


// ColorDepthCubeMap
namespace mirinae {

    bool ColorDepthCubeMap::init(
        uint32_t width,
        uint32_t height,
        IEnvmapRpBundle& rp_pkg,
        mirinae::VulkanDevice& device
    ) {
        mirinae::ImageCreateInfo cinfo;
        cinfo.set_format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
            .set_dimensions(width, height)
            .deduce_mip_levels()
            .set_arr_layers(6)
            .add_usage_sampled()
            .add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .add_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            .add_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .add_flag(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
        img_.init(cinfo.get(), device.mem_alloc());

        mirinae::ImageViewBuilder iv_builder;
        iv_builder.view_type(VK_IMAGE_VIEW_TYPE_CUBE)
            .format(img_.format())
            .mip_levels(img_.mip_levels())
            .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
            .arr_layers(6)
            .image(img_.image());
        cubemap_view_.reset(iv_builder, device);

        iv_builder.view_type(VK_IMAGE_VIEW_TYPE_2D).arr_layers(1);
        for (uint32_t i = 0; i < 6; i++) {
            iv_builder.base_arr_layer(i);
            face_views_[i].reset(iv_builder, device);
        }
        iv_builder.mip_levels(1);
        for (uint32_t i = 0; i < 6; i++) {
            iv_builder.base_arr_layer(i);
            fbuf_face_views_[i].reset(iv_builder, device);
        }

        depth_map_ = mirinae::create_tex_depth(
            img_.width(), img_.height(), device
        );

        for (uint32_t i = 0; i < 6; i++) {
            mirinae::FbufCinfo fbuf_cinfo;
            fbuf_cinfo.set_rp(rp_pkg.rp_base().render_pass())
                .add_attach(depth_map_->image_view())
                .add_attach(fbuf_face_views_[i].get())
                .set_dim(img_.width(), img_.height());
            fbufs_[i].init(fbuf_cinfo.get(), device.logi_device());
        }

        return true;
    }

    void ColorDepthCubeMap::destroy(mirinae::VulkanDevice& device) {
        for (auto& x : fbufs_) x.destroy(device.logi_device());

        cubemap_view_.destroy(device);
        for (auto& x : face_views_) x.destroy(device);
        for (auto& x : fbuf_face_views_) x.destroy(device);

        depth_map_.reset();
        img_.destroy(device.mem_alloc());
    }

    uint32_t ColorDepthCubeMap::width() const { return img_.width(); }

    uint32_t ColorDepthCubeMap::height() const { return img_.height(); }

    VkExtent2D ColorDepthCubeMap::extent2d() const { return img_.extent2d(); }

    VkFramebuffer ColorDepthCubeMap::face_fbuf(size_t index) const {
        return fbufs_.at(index).get();
    }

    VkImageView ColorDepthCubeMap::face_view(size_t index) const {
        return face_views_.at(index).get();
    }

    VkImageView ColorDepthCubeMap::cube_view() const {
        return cubemap_view_.get();
    }

}  // namespace mirinae


// CubeMap
namespace mirinae {

    bool CubeMap::init(
        IEnvmapRpBundle& rp_pkg,
        mirinae::DescPool& desc_pool,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        if (!base_.init(256, 256, rp_pkg, device))
            return false;
        if (!diffuse_.init(256, 256, rp_pkg, device))
            return false;
        if (!specular_.init(128, 128, rp_pkg, device))
            return false;

        desc_set_ = desc_pool.alloc(
            desclayouts.get("envdiffuse:main").layout(), device.logi_device()
        );
        auto sampler = device.samplers().get_linear();
        mirinae::DescWriteInfoBuilder write;
        write.set_descset(desc_set_)
            .add_img_sampler(base_.cube_view(), sampler)
            .apply_all(device.logi_device());

        return true;
    }

    void CubeMap::destroy(mirinae::VulkanDevice& device) {
        base_.destroy(device);
        diffuse_.destroy(device);
        specular_.destroy(device);
    }

}  // namespace mirinae


// BrdfLut
namespace mirinae {

    bool BrdfLut::init(
        uint32_t width,
        uint32_t height,
        IEnvmapRpBundle& rp_pkg,
        mirinae::VulkanDevice& device
    ) {
        mirinae::ImageCreateInfo cinfo;
        cinfo.set_format(VK_FORMAT_R16G16_SFLOAT)
            .set_dimensions(width, height)
            .set_mip_levels(1)
            .add_usage_sampled()
            .add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        img_.init(cinfo.get(), device.mem_alloc());

        mirinae::ImageViewBuilder iv_builder;
        iv_builder.view_type(VK_IMAGE_VIEW_TYPE_2D)
            .format(img_.format())
            .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
            .image(img_.image());
        view_.reset(iv_builder, device);

        mirinae::Fbuf fbuf_;
        mirinae::FbufCinfo fbuf_cinfo;
        fbuf_cinfo.set_rp(rp_pkg.rp_brdf_lut().render_pass())
            .add_attach(view_.get())
            .set_dim(width, height);
        fbuf_.init(fbuf_cinfo.get(), device.logi_device());

        mirinae::CommandPool pool;
        pool.init(device);
        const auto cmdbuf = pool.begin_single_time(device);
        this->record_drawing(cmdbuf, fbuf_, rp_pkg);
        pool.end_single_time(cmdbuf, device);
        device.wait_idle();
        pool.destroy(device.logi_device());
        fbuf_.destroy(device.logi_device());

        return true;
    }

    void BrdfLut::destroy(mirinae::VulkanDevice& device) {
        view_.destroy(device);
        img_.destroy(device.mem_alloc());
    }

    VkImageView BrdfLut::view() const { return view_.get(); }

    void BrdfLut::record_drawing(
        const VkCommandBuffer cmdbuf,
        const mirinae::Fbuf& fbuf,
        const IEnvmapRpBundle& rp_pkg
    ) {
        auto& rp = rp_pkg.rp_brdf_lut();

        mirinae::RenderPassBeginInfo{}
            .rp(rp.render_pass())
            .fbuf(fbuf.get())
            .wh(img_.width(), img_.height())
            .clear_value_count(rp.clear_value_count())
            .clear_values(rp.clear_values())
            .record_begin(cmdbuf);

        vkCmdBindPipeline(
            cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
        );

        mirinae::Viewport{}
            .set_wh(img_.width(), img_.height())
            .record_single(cmdbuf);
        mirinae::Rect2D{}
            .set_wh(img_.width(), img_.height())
            .record_scissor(cmdbuf);

        vkCmdDraw(cmdbuf, 6, 1, 0, 0);
        vkCmdEndRenderPass(cmdbuf);
    }

}  // namespace mirinae


namespace mirinae {

    EnvmapBundle::Item::Item() : entity_(entt::null) {}

    glm::dvec3 EnvmapBundle::Item::world_pos() const { return -world_mat_[3]; }

}  // namespace mirinae


// EnvmapBundle
namespace mirinae {

    EnvmapBundle::EnvmapBundle(
        IEnvmapRpBundle& rp_pkg, mirinae::VulkanDevice& device
    )
        : device_(device) {
        brdf_lut_.init(512, 512, rp_pkg, device_);
    }

    EnvmapBundle::~EnvmapBundle() { this->destroy(); }

    uint32_t EnvmapBundle::count() const {
        return static_cast<uint32_t>(items_.size());
    }

    glm::dvec3 EnvmapBundle::pos_at(uint32_t index) const {
        return items_.at(index).world_pos();
    }

    VkImageView EnvmapBundle::diffuse_at(uint32_t index) const {
        return items_.at(index).cube_map_.diffuse().cube_view();
    }

    VkImageView EnvmapBundle::specular_at(uint32_t index) const {
        return items_.at(index).cube_map_.specular().cube_view();
    }

    VkImageView EnvmapBundle::brdf_lut() const { return brdf_lut_.view(); }

    void EnvmapBundle::add(
        IEnvmapRpBundle& rp_pkg,
        mirinae::DescPool& desc_pool,
        mirinae::DesclayoutManager& desclayouts
    ) {
        items_.emplace_back();
        items_.back().cube_map_.init(rp_pkg, desc_pool, desclayouts, device_);
    }

    void EnvmapBundle::destroy() {
        for (auto& item : items_) item.cube_map_.destroy(device_);
        items_.clear();
        brdf_lut_.destroy(device_);
    }

    bool EnvmapBundle::has_entt(entt::entity e) const {
        for (const auto& x : items_) {
            if (x.entity_ == e)
                return true;
        }

        return false;
    }

    EnvmapBundle::Item* EnvmapBundle::choose_to_update() {
        double min_elapsed = std::numeric_limits<double>::max();
        Item* chosen = nullptr;

        for (auto& item : items_) {
            if (entt::null == item.entity_)
                continue;

            if (item.timer_.elapsed() < min_elapsed) {
                min_elapsed = item.timer_.elapsed();
                chosen = &item;
            }
        }

        return chosen;
    }

}  // namespace mirinae
