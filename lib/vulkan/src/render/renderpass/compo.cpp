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


// Compo Sky
namespace {

    class RpStatesCompoSky : public mirinae::IRpStates {

    public:
        RpStatesCompoSky(
            VkImageView sky_tex,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res), render_pass_(device) {
            // Descriptor layout
            {
                mirinae::DescLayoutBuilder builder{ this->name() + ":main" };
                builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
                desclayouts.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                auto& desc_layout = desclayouts.get(name() + ":main");

                desc_pool_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.size_info(),
                    device.logi_device()
                );

                auto desc_sets = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_ = desc_sets[i];

                    writer.add_img_info()
                        .set_img_view(sky_tex)
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 0);
                }
                writer.apply_all(device.logi_device());
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(fbuf_bundle.depth().format())
                    .ini_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
                    .stor_op(VK_ATTACHMENT_STORE_OP_STORE);
                builder.attach_desc()
                    .add(fbuf_bundle.compo().format())
                    .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();

                builder.color_attach_ref().add_color_attach(1);

                builder.depth_attach_ref().set(0);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                auto& desc_layout = desclayouts.get(name() + ":main");

                mirinae::PipelineLayoutBuilder{}
                    .desc(desc_layout.layout())
                    .add_frag_flag()
                    .pc<mirinae::U_CompoSkyMain>()
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/compo_sky_vert.spv")
                    .add_frag(":asset/spv/compo_sky_frag.spv");

                builder.depth_stencil_state()
                    .depth_test_enable(true)
                    .depth_compare_op(VK_COMPARE_OP_LESS_OR_EQUAL);

                builder.color_blend_state().add(false, 1);

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_ = builder.build(render_pass_, pipe_layout_);
            }

            // Framebuffers
            {
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(render_pass_)
                    .set_dim(fbuf_bundle.width(), fbuf_bundle.height())
                    .add_attach(fbuf_bundle.depth().image_view())
                    .add_attach(fbuf_bundle.compo().image_view());
                for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i)
                    frame_data_[i].fbuf_ = fbuf_cinfo.build(device);

                fbuf_width_ = fbuf_bundle.width();
                fbuf_height_ = fbuf_bundle.height();
            }

            // Misc
            {
                clear_values_.at(0).depthStencil = { 1.0f, 0 };
                clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            }

            return;
        }

        ~RpStatesCompoSky() override {
            for (auto& fd : frame_data_) {
                if (VK_NULL_HANDLE != fd.fbuf_) {
                    vkDestroyFramebuffer(
                        device_.logi_device(), fd.fbuf_, nullptr
                    );
                    fd.fbuf_ = VK_NULL_HANDLE;
                }
            }

            desc_pool_.destroy(device_.logi_device());

            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device_.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != pipe_layout_) {
                vkDestroyPipelineLayout(
                    device_.logi_device(), pipe_layout_, nullptr
                );
                pipe_layout_ = VK_NULL_HANDLE;
            }
        }

        const std::string& name() const override {
            static const std::string name = "compo_sky";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;
            auto& fd = frame_data_[ctxt.f_index_.get()];

            const VkExtent2D fbuf_ext{ fbuf_width_, fbuf_height_ };

            mirinae::RenderPassBeginInfo{}
                .rp(render_pass_)
                .fbuf(fd.fbuf_)
                .wh(fbuf_ext)
                .clear_value_count(clear_values_.size())
                .clear_values(clear_values_.data())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(pipe_layout_)
                .set(fd.desc_set_)
                .record(cmdbuf);

            mirinae::U_CompoSkyMain pc;
            pc.proj_inv_ = glm::inverse(ctxt.proj_mat_);
            pc.view_inv_ = glm::inverse(ctxt.view_mat_);
            if (auto& atmos = ctxt.draw_sheet_->atmosphere_)
                pc.fog_color_density_ = glm::vec4{ atmos->fog_color_,
                                                   atmos->fog_density_ };

            mirinae::PushConstInfo{}
                .layout(pipe_layout_)
                .add_stage_frag()
                .record(cmdbuf, pc);

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);

            vkCmdEndRenderPass(cmdbuf);
        }

    private:
        struct FrameData {
            VkDescriptorSet desc_set_;
            VkFramebuffer fbuf_;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;
        mirinae::DescPool desc_pool_;
        mirinae::RenderPassRaii render_pass_;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout pipe_layout_ = VK_NULL_HANDLE;

        std::array<VkClearValue, 2> clear_values_;
        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
    };

}  // namespace


namespace mirinae::rp::compo {

    URpStates create_rps_sky(
        VkImageView sky_tex,
        mirinae::FbufImageBundle& fbuf_bundle,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesCompoSky>(
            sky_tex, fbuf_bundle, rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp::compo
