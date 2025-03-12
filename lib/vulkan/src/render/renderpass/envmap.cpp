#include "mirinae/render/renderpass/envmap.hpp"

#include <set>

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/envmap.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/renderpass/builder.hpp"


namespace {

    struct LocalRpReg {
        std::unique_ptr<mirinae::IRenderPassBundle> base_;
        std::unique_ptr<mirinae::IRenderPassBundle> diffuse_;
        std::unique_ptr<mirinae::IRenderPassBundle> specular_;
        std::unique_ptr<mirinae::IRenderPassBundle> sky_;
        std::unique_ptr<mirinae::IRenderPassBundle> brdf_lut_;
    };


    class ColorCubeMap {

    public:
        bool init(
            uint32_t width,
            uint32_t height,
            LocalRpReg& rp_pkg,
            mirinae::VulkanDevice& device
        ) {
            mirinae::ImageCreateInfo cinfo;
            cinfo.set_format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
                .set_dimensions(width, height)
                .set_mip_levels(1)
                .set_arr_layers(6)
                .add_usage_sampled()
                .add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                .add_flag(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
            img_.init(cinfo.get(), device.mem_alloc());

            mirinae::ImageViewBuilder iv_builder;
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
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(rp_pkg.diffuse_->renderpass())
                    .add_attach(face_views_[i].get())
                    .set_dim(width, height);
                fbufs_[i].init(fbuf_cinfo.get(), device.logi_device());
            }

            return true;
        }

        void destroy(mirinae::VulkanDevice& device) {
            for (auto& x : fbufs_) x.destroy(device.logi_device());

            cubemap_view_.destroy(device);
            for (auto& x : face_views_) x.destroy(device);

            img_.destroy(device.mem_alloc());
        }

        uint32_t width() const { return img_.width(); }
        uint32_t height() const { return img_.height(); }
        VkExtent2D extent2d() const { return img_.extent2d(); }

        VkFramebuffer face_fbuf(size_t index) const {
            return fbufs_.at(index).get();
        }
        VkImageView face_view(size_t index) const {
            return face_views_.at(index).get();
        }
        VkImageView cube_view() const { return cubemap_view_.get(); }

    private:
        mirinae::Image img_;
        mirinae::ImageView cubemap_view_;
        std::array<mirinae::ImageView, 6> face_views_;
        std::array<mirinae::Fbuf, 6> fbufs_;
    };


    class ColorCubeMapWithMips {

    public:
        bool init(
            uint32_t base_width,
            uint32_t base_height,
            ::LocalRpReg& rp_pkg,
            mirinae::VulkanDevice& device
        ) {
            constexpr uint32_t MAX_MIP_LEVELS = 4;

            mirinae::ImageCreateInfo cinfo;
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

            mirinae::ImageViewBuilder iv_builder;
            iv_builder.view_type(VK_IMAGE_VIEW_TYPE_CUBE)
                .format(img_.format())
                .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .arr_layers(6)
                .mip_levels(img_.mip_levels())
                .image(img_.image());
            cubemap_view_.reset(iv_builder, device);

            iv_builder.view_type(VK_IMAGE_VIEW_TYPE_2D)
                .arr_layers(1)
                .mip_levels(1);

            mips_.resize(img_.mip_levels());
            for (uint32_t lvl = 0; lvl < img_.mip_levels(); ++lvl) {
                auto& mip = mips_[lvl];

                iv_builder.base_mip_level(lvl);
                mip.roughness_ = static_cast<float>(lvl) /
                                 (img_.mip_levels() - 1);
                mip.width_ = img_.width() >> lvl;
                mip.height_ = img_.height() >> lvl;

                for (uint32_t face_i = 0; face_i < 6; ++face_i) {
                    auto& face = mip.faces_[face_i];

                    iv_builder.base_arr_layer(face_i);
                    face.view_.reset(iv_builder, device);

                    mirinae::FbufCinfo fbuf_cinfo;
                    fbuf_cinfo.set_rp(rp_pkg.diffuse_->renderpass())
                        .add_attach(face.view_.get())
                        .set_dim(mip.width_, mip.height_);
                    face.fbuf_.init(fbuf_cinfo.get(), device.logi_device());
                }
            }

            return true;
        }

        void destroy(mirinae::VulkanDevice& device) {
            for (auto& mip : mips_) mip.destroy(device);

            cubemap_view_.destroy(device);
            img_.destroy(device.mem_alloc());
        }

        VkImageView cube_view() const { return cubemap_view_.get(); }

        auto base_width() const { return img_.width(); }
        auto base_height() const { return img_.height(); }

        auto& mips() const { return mips_; }

    private:
        struct FaceData {
            void destroy(mirinae::VulkanDevice& device) {
                view_.destroy(device);
                fbuf_.destroy(device.logi_device());
            }

            mirinae::ImageView view_;
            mirinae::Fbuf fbuf_;
        };

        struct MipData {
            void destroy(mirinae::VulkanDevice& device) {
                for (auto& x : faces_) x.destroy(device);
            }

            VkExtent2D extent2d() const {
                VkExtent2D out;
                out.width = width_;
                out.height = height_;
                return out;
            }

            std::array<FaceData, 6> faces_;
            float roughness_ = 0.0;
            uint32_t width_ = 0;
            uint32_t height_ = 0;
        };

        mirinae::Image img_;
        mirinae::ImageView cubemap_view_;
        std::vector<MipData> mips_;
    };


    class ColorDepthCubeMap {

    public:
        bool init(
            uint32_t width,
            uint32_t height,
            ::LocalRpReg& rp_pkg,
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
                fbuf_cinfo.set_rp(rp_pkg.base_->renderpass())
                    .add_attach(depth_map_->image_view())
                    .add_attach(fbuf_face_views_[i].get())
                    .set_dim(img_.width(), img_.height());
                fbufs_[i].init(fbuf_cinfo.get(), device.logi_device());
            }

            return true;
        }

        void destroy(mirinae::VulkanDevice& device) {
            for (auto& x : fbufs_) x.destroy(device.logi_device());

            cubemap_view_.destroy(device);
            for (auto& x : face_views_) x.destroy(device);
            for (auto& x : fbuf_face_views_) x.destroy(device);

            depth_map_.reset();
            img_.destroy(device.mem_alloc());
        }

        uint32_t width() const { return img_.width(); }
        uint32_t height() const { return img_.height(); }
        VkExtent2D extent2d() const { return img_.extent2d(); }

        VkFramebuffer face_fbuf(size_t index) const {
            return fbufs_.at(index).get();
        }
        VkImageView face_view(size_t index) const {
            return face_views_.at(index).get();
        }
        VkImageView cube_view() const { return cubemap_view_.get(); }

    public:
        mirinae::Image img_;
        mirinae::Semaphore semaphores_;

    private:
        std::unique_ptr<mirinae::ITexture> depth_map_;
        mirinae::ImageView cubemap_view_;
        std::array<mirinae::ImageView, 6> face_views_;
        std::array<mirinae::ImageView, 6> fbuf_face_views_;
        std::array<mirinae::Fbuf, 6> fbufs_;
    };


    class CubeMap {

    public:
        bool init(
            ::LocalRpReg& rp_pkg,
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
                desclayouts.get("envdiffuse:main").layout(),
                device.logi_device()
            );
            auto sampler = device.samplers().get_linear();
            mirinae::DescWriteInfoBuilder write;
            write.set_descset(desc_set_)
                .add_img_sampler(base_.cube_view(), sampler)
                .apply_all(device.logi_device());

            return true;
        }

        void destroy(mirinae::VulkanDevice& device) {
            base_.destroy(device);
            diffuse_.destroy(device);
            specular_.destroy(device);
        }

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
            ::LocalRpReg& rp_pkg,
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
            fbuf_cinfo.set_rp(rp_pkg.brdf_lut_->renderpass())
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

        void destroy(mirinae::VulkanDevice& device) {
            view_.destroy(device);
            img_.destroy(device.mem_alloc());
        }

        VkImageView view() const { return view_.get(); }

    private:
        void record_drawing(
            const VkCommandBuffer cmdbuf,
            const mirinae::Fbuf& fbuf,
            const ::LocalRpReg& rp_pkg
        ) {
            auto& rp = *rp_pkg.brdf_lut_;

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
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

        mirinae::Image img_;
        mirinae::ImageView view_;
    };

}  // namespace


// env_sky
namespace { namespace env_sky {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "env_sky:main" };
        builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // Equirectangular
        return desclayouts.add(builder, device.logi_device());
    }

    VkRenderPass create_renderpass(
        VkFormat depth, VkFormat color, VkDevice logi_device
    ) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(depth)
            .ini_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        builder.attach_desc()
            .add(color)
            .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .op_pair_clear_store();

        builder.color_attach_ref().add_color_attach(1);

        builder.depth_attach_ref().set(0);

        builder.subpass_dep().add().preset_single();

        return builder.build(logi_device);
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        mirinae::PipelineBuilder builder{ device };

        builder.shader_stages()
            .add_vert(":asset/spv/env_sky_vert.spv")
            .add_frag(":asset/spv/env_sky_frag.spv");

        builder.color_blend_state().add(false, 1);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                device.img_formats().depth_map(),
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            };

            clear_values_.at(0).depthStencil = { 0, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), formats_.at(1), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .add_vertex_flag()
                          .pc<mirinae::U_EnvSkyPushConst>()
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return VK_NULL_HANDLE;
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 2> formats_;
        std::array<VkClearValue, 2> clear_values_;
    };

}}  // namespace ::env_sky


// env_base
namespace { namespace env_base {

    VkDescriptorSetLayout create_desclayout_model(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:model").layout();
    }

    VkDescriptorSetLayout create_desclayout_actor(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("gbuf:actor").layout();
    }

    VkRenderPass create_renderpass(
        VkFormat depth, VkFormat color, VkDevice logi_device
    ) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(depth)
            .ini_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .op_pair_clear_store();
        builder.attach_desc()
            .add(color)
            .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            .op_pair_load_store();

        builder.color_attach_ref().add_color_attach(1);

        builder.depth_attach_ref().set(0);

        builder.subpass_dep().add().preset_single();

        return builder.build(logi_device);
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        mirinae::PipelineBuilder builder{ device };

        builder.shader_stages()
            .add_vert(":asset/spv/env_base_vert.spv")
            .add_frag(":asset/spv/env_base_frag.spv");

        builder.vertex_input_state().set_static();

        builder.rasterization_state().cull_mode(VK_CULL_MODE_FRONT_BIT);

        builder.depth_stencil_state()
            .depth_test_enable(true)
            .depth_write_enable(true);

        builder.color_blend_state().add(false, 1);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                device.img_formats().depth_map(),
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            };

            clear_values_.at(0).depthStencil = { 0, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), formats_.at(1), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_model(desclayouts, device))
                          .desc(create_desclayout_actor(desclayouts, device))
                          .add_vertex_flag()
                          .pc<mirinae::U_EnvmapPushConst>()
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return VK_NULL_HANDLE;
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 2> formats_;
        std::array<VkClearValue, 2> clear_values_;
    };

}}  // namespace ::env_base


// env_diffuse
namespace { namespace env_diffuse {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "envdiffuse:main" };
        builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // envmap
        return desclayouts.add(builder, device.logi_device());
    }

    VkRenderPass create_renderpass(VkFormat color, VkDevice logi_device) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(color)
            .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .op_pair_clear_store();

        builder.color_attach_ref().add_color_attach(0);

        builder.subpass_dep().add().preset_single();

        return builder.build(logi_device);
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        mirinae::PipelineBuilder builder{ device };

        builder.shader_stages()
            .add_vert(":asset/spv/env_diffuse_vert.spv")
            .add_frag(":asset/spv/env_diffuse_frag.spv");

        builder.color_blend_state().add(false, 1);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = { VK_FORMAT_B10G11R11_UFLOAT_PACK32 };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .add_vertex_flag()
                          .pc<mirinae::U_EnvdiffusePushConst>()
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return VK_NULL_HANDLE;
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
    };

}}  // namespace ::env_diffuse


// env_specular
namespace { namespace env_specular {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        return desclayouts.get("envdiffuse:main").layout();
    }

    VkRenderPass create_renderpass(VkFormat color, VkDevice logi_device) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(color)
            .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .op_pair_clear_store();

        builder.color_attach_ref().add_color_attach(0);

        builder.subpass_dep().add().preset_single();

        return builder.build(logi_device);
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        mirinae::PipelineBuilder builder{ device };

        builder.shader_stages()
            .add_vert(":asset/spv/env_specular_vert.spv")
            .add_frag(":asset/spv/env_specular_frag.spv");

        builder.color_blend_state().add(false, 1);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = { VK_FORMAT_B10G11R11_UFLOAT_PACK32 };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .add_vertex_flag()
                          .add_frag_flag()
                          .pc<mirinae::U_EnvSpecularPushConst>()
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return VK_NULL_HANDLE;
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
    };

}}  // namespace ::env_specular


// env_lut
namespace { namespace env_lut {

    VkRenderPass create_renderpass(VkFormat color, VkDevice logi_device) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(color)
            .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .op_pair_clear_store();

        builder.color_attach_ref().add_color_attach(0);

        builder.subpass_dep().add().preset_single();

        return builder.build(logi_device);
    }

    VkPipeline create_pipeline(
        VkRenderPass renderpass,
        VkPipelineLayout pipelineLayout,
        mirinae::VulkanDevice& device
    ) {
        mirinae::PipelineBuilder builder{ device };

        builder.shader_stages()
            .add_vert(":asset/spv/env_lut_vert.spv")
            .add_frag(":asset/spv/env_lut_frag.spv");

        builder.color_blend_state().add(false, 1);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = { VK_FORMAT_R16G16_SFLOAT };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}.build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            renderpass_.destroy(device_);
            pipeline_.destroy(device_);
            layout_.destroy(device_);
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return VK_NULL_HANDLE;
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override {
            return static_cast<uint32_t>(clear_values_.size());
        }

    private:
        std::array<VkFormat, 1> formats_;
        std::array<VkClearValue, 1> clear_values_;
    };

}}  // namespace ::env_lut


namespace {

    class EnvmapBundle : public mirinae::IEnvmapBundle {

    public:
        struct Item {
            ::CubeMap cube_map_;
            sung::MonotonicRealtimeTimer timer_;
            entt::entity entity_ = entt::null;
        };

        EnvmapBundle(::LocalRpReg& rp_pkg, mirinae::VulkanDevice& device)
            : device_(device) {
            brdf_lut_.init(512, 512, rp_pkg, device_);
        }

        ~EnvmapBundle() override { this->destroy(); }

        uint32_t count() const override {
            return static_cast<uint32_t>(items_.size());
        }

        VkImageView diffuse_at(uint32_t index) const override {
            return items_.at(index).cube_map_.diffuse().cube_view();
        }

        VkImageView specular_at(uint32_t index) const override {
            return items_.at(index).cube_map_.specular().cube_view();
        }

        VkImageView brdf_lut() const override { return brdf_lut_.view(); }

        void add(
            ::LocalRpReg& rp_pkg,
            mirinae::DescPool& desc_pool,
            mirinae::DesclayoutManager& desclayouts
        ) {
            items_.emplace_back();
            items_.back().cube_map_.init(
                rp_pkg, desc_pool, desclayouts, device_
            );
        }

        void destroy() {
            for (auto& item : items_) item.cube_map_.destroy(device_);
            items_.clear();
            brdf_lut_.destroy(device_);
        }

        auto begin() { return items_.begin(); }
        auto end() { return items_.end(); }

        bool has_entt(entt::entity e) const {
            for (const auto& x : items_) {
                if (x.entity_ == e)
                    return true;
            }

            return false;
        }

        Item* choose_to_update() {
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

    private:
        mirinae::VulkanDevice& device_;
        std::vector<Item> items_;
        ::BrdfLut brdf_lut_;
    };

}  // namespace


namespace {

    const glm::dvec3 DVEC_ZERO{ 0, 0, 0 };
    const glm::dvec3 DVEC_DOWN{ 0, -1, 0 };

    const std::array<glm::dmat4, 6> CUBE_VIEW_MATS{
        glm::lookAt(DVEC_ZERO, glm::dvec3(1, 0, 0), DVEC_DOWN),
        glm::lookAt(DVEC_ZERO, glm::dvec3(-1, 0, 0), DVEC_DOWN),
        glm::lookAt(DVEC_ZERO, glm::dvec3(0, 1, 0), glm::dvec3(0, 0, 1)),
        glm::lookAt(DVEC_ZERO, DVEC_DOWN, glm::dvec3(0, 0, -1)),
        glm::lookAt(DVEC_ZERO, glm::dvec3(0, 0, 1), DVEC_DOWN),
        glm::lookAt(DVEC_ZERO, glm::dvec3(0, 0, -1), DVEC_DOWN)
    };


    class EnvmapSelector {

    public:
        void notify(entt::entity e, double distance) {
            entities_.insert({ distance, e });
        }

        bool empty() const { return entities_.empty(); }

        bool has_entt(entt::entity e) const {
            for (const auto& x : entities_) {
                if (x.second == e)
                    return true;
            }

            return false;
        }

        auto begin() const { return entities_.begin(); }
        auto end() const { return entities_.end(); }

    private:
        std::set<std::pair<double, entt::entity>> entities_;
    };


    class RpMaster : public mirinae::IRpStates {

    public:
        RpMaster(mirinae::VulkanDevice& device) : device_(device) {}

        ~RpMaster() override { this->destroy(); }

        const std::string& name() const override {
            static const std::string out = "envmap";
            return out;
        }

        void init(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts
        ) {
            auto& tex_man = *rp_res.tex_man_;

            // Render passes
            {
                rp_pkg_.base_ = std::make_unique<::env_base::RPBundle>(
                    desclayouts, device_
                );
                rp_pkg_.diffuse_ = std::make_unique<::env_diffuse::RPBundle>(
                    desclayouts, device_
                );
                rp_pkg_.specular_ = std::make_unique<::env_specular::RPBundle>(
                    desclayouts, device_
                );
                rp_pkg_.sky_ = std::make_unique<::env_sky::RPBundle>(
                    desclayouts, device_
                );
                rp_pkg_.brdf_lut_ = std::make_unique<::env_lut::RPBundle>(
                    desclayouts, device_
                );
            }

            desc_pool_.init(
                5,
                desclayouts.get("envdiffuse:main").size_info() +
                    desclayouts.get("env_sky:main").size_info(),
                device_.logi_device()
            );

            // Sky texture
            {
                auto e = this->select_atmos_simple(cosmos.reg());
                auto& atmos = cosmos.reg().get<mirinae::cpnt::AtmosphereSimple>(
                    e
                );
                if (tex_man.request_blck(atmos.sky_tex_path_, false)) {
                    sky_tex_ = tex_man.get(atmos.sky_tex_path_);
                } else {
                    sky_tex_ = tex_man.missing_tex();
                }
            }

            sky_tex_ = tex_man.block_for_tex("Sung/satara_night_4k.hdr", false);
            if (!sky_tex_) {
                const auto alt_path =
                    ":asset/textures/"
                    "kloofendal_48d_partly_cloudy_puresky_1k.hdr";
                sky_tex_ = tex_man.block_for_tex(alt_path, false);
            }
            if (!sky_tex_) {
                MIRINAE_ABORT("Failed to load sky texture");
            }

            desc_set_ = desc_pool_.alloc(
                desclayouts.get("env_sky:main").layout(), device_.logi_device()
            );

            mirinae::DescWriteInfoBuilder desc_info;
            desc_info.set_descset(desc_set_)
                .add_img_sampler(
                    sky_tex_->image_view(), device_.samplers().get_linear()
                )
                .apply_all(device_.logi_device());

            envmaps_ = std::make_unique<::EnvmapBundle>(rp_pkg_, device_);
            envmaps_->add(rp_pkg_, desc_pool_, desclayouts);
            rp_res.envmaps_ = envmaps_;

            timer_.set_min();
        }

        void destroy() {
            envmaps_->destroy();
            desc_pool_.destroy(device_.logi_device());
        }

        void record(const mirinae::RpContext& ctxt) override {
            if (!timer_.check_if_elapsed(3))
                return;

            auto& reg = ctxt.cosmos_->reg();
            ::EnvmapSelector entities;
            for (auto e : reg.view<mirinae::cpnt::Envmap>()) {
                if (auto tform = reg.try_get<mirinae::cpnt::Transform>(e)) {
                    const auto d = glm::length(tform->pos_ - ctxt.view_pos_);
                    entities.notify(e, d);
                } else {
                    entities.notify(e, glm::length(ctxt.view_pos_));
                }
            }
            if (entities.empty())
                return;

            for (auto& x : *envmaps_) {
                if (x.entity_ != entt::null && entities.has_entt(x.entity_))
                    continue;

                x.entity_ = entt::null;
                for (auto& [_, candidate] : entities) {
                    if (!envmaps_->has_entt(candidate)) {
                        x.entity_ = candidate;
                        x.timer_.set_min();
                        break;
                    }
                }
            }

            auto chosen = envmaps_->choose_to_update();
            if (!chosen)
                return;
            const auto e_selected = chosen->entity_;
            auto& env_cpnt = reg.get<mirinae::cpnt::Envmap>(e_selected);
            env_cpnt.last_updated_.check();
            chosen->timer_.check();

            SPDLOG_DEBUG("Updating envmap: entt={}", (int)chosen->entity_);
            this->record_sky(e_selected, *chosen, ctxt, desc_set_);
            this->record_base(e_selected, *chosen, ctxt);
            this->record_diffuse(e_selected, *chosen, ctxt);
            this->record_specular(e_selected, *chosen, ctxt);
        }

    private:
        static entt::entity select_atmos_simple(entt::registry& reg) {
            for (auto entity : reg.view<mirinae::cpnt::AtmosphereSimple>())
                return entity;

            return entt::null;
        }

        void record_base(
            entt::entity e_env,
            ::EnvmapBundle::Item& env_item,
            const mirinae::RpContext& ctxt
        ) {
            namespace cpnt = mirinae::cpnt;
            auto& rp = *rp_pkg_.base_;
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.renderpass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = glm::perspectiveRH_ZO(
                mirinae::Angle::from_deg(90.0).rad(), 1.0, 0.1, 1000.0
            );

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

            auto& cube_map = env_item.cube_map_;
            auto& base_cube = cube_map.base();

            glm::dmat4 world_mat(1);
            if (auto tform = reg.try_get<cpnt::Transform>(e_env)) {
                world_mat = glm::translate<double>(world_mat, -tform->pos_);
            }

            mirinae::Viewport{}
                .set_wh(base_cube.extent2d())
                .record_single(cmdbuf);
            mirinae::Rect2D{}
                .set_wh(base_cube.extent2d())
                .record_scissor(cmdbuf);
            rp_info.wh(base_cube.width(), base_cube.height());

            for (int i = 0; i < 6; ++i) {
                rp_info.fbuf(cube_map.base().face_fbuf(i)).record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                mirinae::U_EnvmapPushConst push_const;
                for (auto e : reg.view<cpnt::DLight, cpnt::Transform>()) {
                    const auto& light = reg.get<cpnt::DLight>(e);
                    const auto& tform = reg.get<cpnt::Transform>(e);
                    push_const.dlight_dir_ = glm::vec4{
                        light.calc_to_light_dir(glm::dmat4(1), tform), 0
                    };
                    push_const.dlight_color_ = glm::vec4{
                        light.color_.scaled_color(), 0
                    };
                    break;
                }

                for (auto& pair : ctxt.draw_sheet_->static_) {
                    auto& unit = *pair.unit_;
                    descset_info.first_set(0)
                        .set(unit.get_desc_set(ctxt.f_index_.get()))
                        .record(cmdbuf);

                    unit.record_bind_vert_buf(cmdbuf);

                    for (auto& actor : pair.actors_) {
                        descset_info.first_set(1)
                            .set(actor.actor_->get_desc_set(ctxt.f_index_.get())
                            )
                            .record(cmdbuf);

                        push_const.proj_view_ =
                            (proj_mat * CUBE_VIEW_MATS[i] * world_mat);

                        mirinae::PushConstInfo{}
                            .layout(rp.pipeline_layout())
                            .add_stage_vert()
                            .record(cmdbuf, push_const);

                        vkCmdDrawIndexed(
                            cmdbuf, unit.vertex_count(), 1, 0, 0, 0
                        );
                    }
                }

                vkCmdEndRenderPass(cmdbuf);
            }

            auto& img = cube_map.base().img_;
            for (uint32_t i = 1; i < img.mip_levels(); ++i) {
                mirinae::ImageMemoryBarrier barrier;
                barrier.image(img.image())
                    .set_src_access(VK_ACCESS_NONE)
                    .set_dst_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .new_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .mip_base(i)
                    .mip_count(1)
                    .layer_base(0)
                    .layer_count(6);
                barrier.record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                );

                mirinae::ImageBlit blit;
                blit.set_src_offsets_full(
                    img.width() >> (i - 1), img.height() >> (i - 1)
                );
                blit.set_dst_offsets_full(img.width() >> i, img.height() >> i);
                blit.src_subres()
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .mip_level(i - 1)
                    .layer_base(0)
                    .layer_count(6);
                blit.dst_subres()
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .mip_level(i)
                    .layer_base(0)
                    .layer_count(6);

                vkCmdBlitImage(
                    cmdbuf,
                    img.image(),
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    img.image(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &blit.get(),
                    VK_FILTER_LINEAR
                );

                barrier.image(img.image())
                    .set_src_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                    .set_dst_access(VK_ACCESS_TRANSFER_READ_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                    .new_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                barrier.record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                );
            }

            mirinae::ImageMemoryBarrier barrier;
            barrier.image(img.image())
                .set_src_access(VK_ACCESS_TRANSFER_READ_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .mip_base(0)
                .mip_count(img.mip_levels())
                .layer_base(0)
                .layer_count(6);
            barrier.record_single(
                cmdbuf,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            );
        }

        void record_sky(
            entt::entity e_env,
            ::EnvmapBundle::Item& env_item,
            const mirinae::RpContext& ctxt,
            const VkDescriptorSet desc_set
        ) {
            namespace cpnt = mirinae::cpnt;
            auto& rp = *rp_pkg_.sky_;
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.renderpass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = glm::perspectiveRH_ZO(
                mirinae::Angle::from_deg(90.0).rad(), 1.0, 0.1, 1000.0
            );

            auto& cube_map = env_item.cube_map_;
            auto& base_cube = cube_map.base();

            glm::dmat4 world_mat(1);
            if (auto tform = reg.try_get<cpnt::Transform>(e_env)) {
                world_mat = glm::translate<double>(world_mat, -tform->pos_);
            }

            mirinae::Viewport{}
                .set_wh(base_cube.width(), base_cube.height())
                .record_single(cmdbuf);
            mirinae::Rect2D{}
                .set_wh(base_cube.width(), base_cube.height())
                .record_scissor(cmdbuf);
            rp_info.wh(base_cube.width(), base_cube.height());

            for (int i = 0; i < 6; ++i) {
                rp_info.fbuf(base_cube.face_fbuf(i)).record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                mirinae::DescSetBindInfo{}
                    .layout(rp.pipeline_layout())
                    .set(desc_set)
                    .record(cmdbuf);

                mirinae::U_EnvSkyPushConst pc;
                pc.proj_view_ = proj_mat * CUBE_VIEW_MATS[i];

                mirinae::PushConstInfo{}
                    .layout(rp.pipeline_layout())
                    .add_stage_vert()
                    .record(cmdbuf, pc);

                vkCmdDraw(cmdbuf, 36, 1, 0, 0);

                vkCmdEndRenderPass(cmdbuf);
            }
        }

        void record_diffuse(
            entt::entity e_env,
            ::EnvmapBundle::Item& env_item,
            const mirinae::RpContext& ctxt
        ) {
            namespace cpnt = mirinae::cpnt;
            auto& rp = *rp_pkg_.diffuse_;
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.renderpass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = glm::perspectiveRH_ZO(
                mirinae::Angle::from_deg(90.0).rad(), 1.0, 0.01, 10.0
            );

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

            auto& cube_map = env_item.cube_map_;
            auto& diffuse = cube_map.diffuse();

            glm::dmat4 world_mat(1);
            if (auto tform = reg.try_get<cpnt::Transform>(e_env)) {
                world_mat = glm::translate<double>(world_mat, -tform->pos_);
            }

            const mirinae::Viewport viewport{ diffuse.extent2d() };
            const mirinae::Rect2D scissor{ diffuse.extent2d() };
            rp_info.wh(diffuse.extent2d());

            for (int i = 0; i < 6; ++i) {
                rp_info.fbuf(diffuse.face_fbuf(i)).record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                viewport.record_single(cmdbuf);
                scissor.record_scissor(cmdbuf);

                descset_info.set(cube_map.desc_set()).record(cmdbuf);

                mirinae::U_EnvdiffusePushConst push_const;
                push_const.proj_view_ = proj_mat * CUBE_VIEW_MATS[i];

                mirinae::PushConstInfo{}
                    .layout(rp.pipeline_layout())
                    .add_stage_vert()
                    .record(cmdbuf, push_const);

                vkCmdDraw(cmdbuf, 36, 1, 0, 0);
                vkCmdEndRenderPass(cmdbuf);
            }
        }

        void record_specular(
            entt::entity e_env,
            ::EnvmapBundle::Item& env_item,
            const mirinae::RpContext& ctxt
        ) {
            namespace cpnt = mirinae::cpnt;
            auto& rp = *rp_pkg_.specular_;
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.renderpass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = glm::perspectiveRH_ZO(
                mirinae::Angle::from_deg(90.0).rad(), 1.0, 0.01, 10.0
            );

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

            auto& cube_map = env_item.cube_map_;
            auto& specular = cube_map.specular();

            glm::dmat4 world_mat(1);
            if (auto tform = reg.try_get<cpnt::Transform>(e_env)) {
                world_mat = glm::translate<double>(world_mat, -tform->pos_);
            }

            for (auto& mip : specular.mips()) {
                const mirinae::Rect2D scissor{ mip.extent2d() };
                const mirinae::Viewport viewport{ scissor.extent2d() };
                rp_info.wh(scissor.extent2d());

                for (int i = 0; i < 6; ++i) {
                    auto& face = mip.faces_[i];

                    rp_info.fbuf(face.fbuf_.get()).record_begin(cmdbuf);

                    vkCmdBindPipeline(
                        cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                    );

                    viewport.record_single(cmdbuf);
                    scissor.record_scissor(cmdbuf);

                    descset_info.set(cube_map.desc_set()).record(cmdbuf);

                    mirinae::U_EnvSpecularPushConst push_const;
                    push_const.proj_view_ = proj_mat * CUBE_VIEW_MATS[i];
                    push_const.roughness_ = mip.roughness_;

                    mirinae::PushConstInfo{}
                        .layout(rp.pipeline_layout())
                        .add_stage_vert()
                        .add_stage_frag()
                        .record(cmdbuf, push_const);

                    vkCmdDraw(cmdbuf, 36, 1, 0, 0);
                    vkCmdEndRenderPass(cmdbuf);
                }
            }
        }

        mirinae::VulkanDevice& device_;
        ::LocalRpReg rp_pkg_;
        mirinae::DescPool desc_pool_;
        sung::MonotonicRealtimeTimer timer_;
        std::shared_ptr<::EnvmapBundle> envmaps_;
        std::shared_ptr<mirinae::ITexture> sky_tex_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;  // For env sky
    };

}  // namespace


namespace mirinae::rp::envmap {

    URpStates create_rp_states_envmap(
        CosmosSimulator& cosmos,
        RpResources& rp_res,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    ) {
        auto out = std::make_unique<RpMaster>(device);
        out->init(cosmos, rp_res, desclayouts);
        return out;
    }

}  // namespace mirinae::rp::envmap
