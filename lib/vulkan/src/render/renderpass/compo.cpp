#include "mirinae/render/renderpass/compo.hpp"

#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/renderpass/builder.hpp"


// compo
namespace { namespace compo {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "compo:main" };
        builder
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // depth
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // albedo
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // normal
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // material
            .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_CompoMain
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // dlight shadowmap
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // slight shadowmap
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env diffuse
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env specular
            .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // env lut
        return desclayouts.add(builder, device.logi_device());
    }

    VkRenderPass create_renderpass(
        VkFormat compo_format, VkDevice logi_device
    ) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(compo_format)
            .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
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
            .add_vert(":asset/spv/compo_basic_vert.spv")
            .add_frag(":asset/spv/compo_basic_frag.spv");

        builder.rasterization_state().cull_mode_back();

        builder.color_blend_state().add(false, 1);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            uint32_t width,
            uint32_t height,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                fbuf_bundle.compo().format(),
            };

            clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);

            mirinae::FbufCinfo fbuf_cinfo;
            fbuf_cinfo.set_rp(renderpass_)
                .set_dim(width, height)
                .add_attach(fbuf_bundle.compo().image_view());
            for (int i = 0; i < swapchain.views_count(); ++i)
                fbufs_.push_back(fbuf_cinfo.build(device));
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }

            for (auto& handle : fbufs_) {
                vkDestroyFramebuffer(device_.logi_device(), handle, nullptr);
            }
            fbufs_.clear();
        }

        VkFramebuffer fbuf_at(uint32_t index) const override {
            return fbufs_.at(index);
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
        std::vector<VkFramebuffer> fbufs_;  // As many as swapchain images
    };

}}  // namespace ::compo


// compo_sky
namespace { namespace compo_sky {

    VkDescriptorSetLayout create_desclayout_main(
        mirinae::DesclayoutManager& desclayouts, mirinae::VulkanDevice& device
    ) {
        mirinae::DescLayoutBuilder builder{ "compo_sky:main" };
        builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // sky texture
        return desclayouts.add(builder, device.logi_device());
    }

    VkRenderPass create_renderpass(
        VkFormat depth, VkFormat compo, VkDevice logi_device
    ) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(depth)
            .ini_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
            .stor_op(VK_ATTACHMENT_STORE_OP_STORE);
        builder.attach_desc()
            .add(compo)
            .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
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
            .add_vert(":asset/spv/compo_sky_vert.spv")
            .add_frag(":asset/spv/compo_sky_frag.spv");

        builder.depth_stencil_state()
            .depth_test_enable(true)  //
            .depth_compare_op(VK_COMPARE_OP_LESS_OR_EQUAL);

        builder.color_blend_state().add(false, 1);

        builder.dynamic_state().add_viewport().add_scissor();

        return builder.build(renderpass, pipelineLayout);
    }


    class RPBundle : public mirinae::IRenderPassBundle {

    public:
        RPBundle(
            VkFormat depth_format,
            VkFormat compo_format,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : IRenderPassBundle(device) {
            formats_ = {
                depth_format,
                compo_format,
            };

            clear_values_.at(0).depthStencil = { 1.0f, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            renderpass_ = create_renderpass(
                formats_.at(0), formats_.at(1), device.logi_device()
            );
            layout_ = mirinae::PipelineLayoutBuilder{}
                          .desc(create_desclayout_main(desclayouts, device))
                          .add_frag_flag()
                          .pc<mirinae::U_CompoSkyMain>()
                          .build(device);
            pipeline_ = create_pipeline(renderpass_, layout_, device);
        }

        ~RPBundle() override { this->destroy(); }

        void destroy() override {
            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), layout_, nullptr
                );
                layout_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != renderpass_) {
                vkDestroyRenderPass(
                    device_.logi_device(), renderpass_, nullptr
                );
                renderpass_ = VK_NULL_HANDLE;
            }
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
        constexpr static int ATTACH_COUNT = 2;
        std::array<VkFormat, ATTACH_COUNT> formats_;
        std::array<VkClearValue, ATTACH_COUNT> clear_values_;
    };

}}  // namespace ::compo_sky


namespace mirinae::rp::compo {

    void create_rp(
        IRenderPassRegistry& reg,
        uint32_t width,
        uint32_t height,
        FbufImageBundle& fbuf_bundle,
        DesclayoutManager& desclayouts,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        reg.add<::compo::RPBundle>(
            "compo", width, height, fbuf_bundle, desclayouts, swapchain, device
        );
        reg.add<::compo_sky::RPBundle>(
            "compo_sky",
            fbuf_bundle.depth().format(),
            fbuf_bundle.compo().format(),
            desclayouts,
            device
        );
    }

}  // namespace mirinae::rp::compo


// RpMasterBasic
namespace mirinae::rp::compo {

    void RpMasterBasic::init(
        DesclayoutManager& desclayouts,
        FbufImageBundle& fbufs,
        VkImageView dlight_shadowmap,
        VkImageView slight_shadowmap,
        VkImageView env_diffuse,
        VkImageView env_specular,
        VkImageView env_lut,
        VulkanDevice& device
    ) {
        auto& desclayout = desclayouts.get("compo:main");
        desc_pool_.init(
            MAX_FRAMES_IN_FLIGHT, desclayout.size_info(), device.logi_device()
        );
        desc_sets_ = desc_pool_.alloc(
            MAX_FRAMES_IN_FLIGHT, desclayout.layout(), device.logi_device()
        );

        const auto sam_lin = device.samplers().get_linear();
        const auto sam_nea = device.samplers().get_nearest();
        const auto sam_cube = device.samplers().get_cubemap();
        DescWriteInfoBuilder builder;
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            auto& ubuf = ubufs_.emplace_back();
            ubuf.init_ubuf(sizeof(U_CompoMain), device.mem_alloc());

            builder.set_descset(desc_sets_.at(i))
                .add_img_sampler(fbufs.depth().image_view(), sam_lin)
                .add_img_sampler(fbufs.albedo().image_view(), sam_lin)
                .add_img_sampler(fbufs.normal().image_view(), sam_lin)
                .add_img_sampler(fbufs.material().image_view(), sam_lin)
                .add_ubuf(ubuf)
                .add_img_sampler(dlight_shadowmap, sam_nea)
                .add_img_sampler(slight_shadowmap, sam_nea)
                .add_img_sampler(env_diffuse, sam_cube)
                .add_img_sampler(env_specular, sam_cube)
                .add_img_sampler(env_lut, sam_lin);
        }
        builder.apply_all(device.logi_device());
    }

    void RpMasterBasic::destroy(VulkanDevice& device) {
        desc_pool_.destroy(device.logi_device());

        for (auto& ubuf : ubufs_) ubuf.destroy(device.mem_alloc());
        ubufs_.clear();
    }

    void RpMasterBasic::record(
        const VkCommandBuffer cur_cmd_buf,
        const VkExtent2D& fbuf_ext,
        const FrameIndex frame_index,
        const ShainImageIndex image_index,
        const IRenderPassRegistry& rp_pkg
    ) {
        auto& rp = rp_pkg.get("compo");

        RenderPassBeginInfo{}
            .rp(rp.renderpass())
            .fbuf(rp.fbuf_at(image_index.get()))
            .wh(fbuf_ext)
            .clear_value_count(rp.clear_value_count())
            .clear_values(rp.clear_values())
            .record_begin(cur_cmd_buf);

        vkCmdBindPipeline(
            cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
        );

        Viewport{ fbuf_ext }.record_single(cur_cmd_buf);
        Rect2D{ fbuf_ext }.record_scissor(cur_cmd_buf);

        DescSetBindInfo{}
            .layout(rp.pipeline_layout())
            .set(desc_sets_.at(frame_index.get()))
            .record(cur_cmd_buf);

        vkCmdDraw(cur_cmd_buf, 3, 1, 0, 0);

        vkCmdEndRenderPass(cur_cmd_buf);
    }

}  // namespace mirinae::rp::compo

// RpMasterSky
namespace mirinae::rp::compo {

    void RpMasterSky::init(
        VkImageView sky_texture,
        IRenderPassRegistry& rp_pkg,
        DesclayoutManager& desclayouts,
        FbufImageBundle& fbufs,
        Swapchain& shain,
        VulkanDevice& device
    ) {
        auto& desclayout = desclayouts.get("compo_sky:main");
        desc_pool_.init(
            MAX_FRAMES_IN_FLIGHT, desclayout.size_info(), device.logi_device()
        );
        desc_sets_ = desc_pool_.alloc(
            MAX_FRAMES_IN_FLIGHT, desclayout.layout(), device.logi_device()
        );

        const auto sam_lin = device.samplers().get_linear();
        DescWriteInfoBuilder builder;
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            builder.set_descset(desc_sets_.at(i))
                .add_img_sampler(sky_texture, sam_lin);
        }
        builder.apply_all(device.logi_device());

        auto& rp = rp_pkg.get("compo_sky");

        FbufCinfo fbuf_cinfo;
        fbuf_cinfo.set_rp(rp.renderpass())
            .set_dim(fbufs.width(), fbufs.height())
            .add_attach(fbufs.depth().image_view())
            .add_attach(fbufs.compo().image_view());
        for (int i = 0; i < shain.views_count(); ++i)
            fbufs_.push_back(fbuf_cinfo.build(device));
    }

    void RpMasterSky::destroy(VulkanDevice& device) {
        desc_pool_.destroy(device.logi_device());

        for (auto& x : fbufs_)
            vkDestroyFramebuffer(device.logi_device(), x, nullptr);
        fbufs_.clear();
    }

    void RpMasterSky::record(
        const VkCommandBuffer cur_cmd_buf,
        const glm::mat4 proj_inv,
        const glm::mat4 view_inv,
        const VkExtent2D& fbuf_ext,
        const FrameIndex frame_index,
        const ShainImageIndex image_index,
        const IRenderPassRegistry& rp_pkg
    ) {
        auto& rp = rp_pkg.get("compo_sky");

        RenderPassBeginInfo{}
            .rp(rp.renderpass())
            .fbuf(fbufs_.at(image_index.get()))
            .wh(fbuf_ext)
            .clear_value_count(rp.clear_value_count())
            .clear_values(rp.clear_values())
            .record_begin(cur_cmd_buf);

        vkCmdBindPipeline(
            cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
        );

        Viewport{ fbuf_ext }.record_single(cur_cmd_buf);
        Rect2D{ fbuf_ext }.record_scissor(cur_cmd_buf);

        DescSetBindInfo{}
            .layout(rp.pipeline_layout())
            .set(desc_sets_.at(frame_index.get()))
            .record(cur_cmd_buf);

        U_CompoSkyMain pc;
        pc.proj_inv_ = proj_inv;
        pc.view_inv_ = view_inv;

        mirinae::PushConstInfo{}
            .layout(rp.pipeline_layout())
            .add_stage_frag()
            .record(cur_cmd_buf, pc);

        vkCmdDraw(cur_cmd_buf, 3, 1, 0, 0);

        vkCmdEndRenderPass(cur_cmd_buf);
    }

}  // namespace mirinae::rp::compo
