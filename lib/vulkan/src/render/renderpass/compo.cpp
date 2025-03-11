#include "mirinae/render/renderpass/compo.hpp"

#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/renderpass/builder.hpp"


// Compo Dlight
namespace {

    struct U_CompoDlightMain {
        glm::mat4 proj_;
        glm::mat4 proj_inv_;
        glm::mat4 view_;
        glm::mat4 view_inv_;
        glm::vec4 fog_color_density_;
    };


    struct U_CompoDlightShadowMap {

    public:
        U_CompoDlightShadowMap& set_dlight_dir(const glm::dvec3& v) {
            dlight_dir_.x = static_cast<float>(v.x);
            dlight_dir_.y = static_cast<float>(v.y);
            dlight_dir_.z = static_cast<float>(v.z);
            return *this;
        }

        U_CompoDlightShadowMap& set_dlight_color(const glm::vec3& v) {
            dlight_color_.x = v.r;
            dlight_color_.y = v.g;
            dlight_color_.z = v.b;
            return *this;
        }

        glm::mat4 light_mats_[4];
        glm::vec4 cascade_depths_;
        glm::vec4 dlight_color_;
        glm::vec4 dlight_dir_;
    };


    class RpStatesCompoDlight : public mirinae::IRpStates {

    public:
        RpStatesCompoDlight(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            namespace cpnt = mirinae::cpnt;
            auto& reg = cosmos.reg();

            // Desc layout: main
            {
                mirinae::DescLayoutBuilder builder{ name() + ":main" };
                builder
                    .add_img_frag(1)    // depth
                    .add_img_frag(1)    // albedo
                    .add_img_frag(1)    // normal
                    .add_img_frag(1)    // material
                    .add_ubuf_frag(1);  // U_CompoDlight
                desclayouts.add(builder, device.logi_device());
            }

            // Desc layout: shadow map
            {
                mirinae::DescLayoutBuilder builder{ name() + ":shadow_map" };
                builder
                    .add_img_frag(1)    // shadow map
                    .add_ubuf_frag(1);  // U_CompoDlightShadowMap
                desclayouts.add(builder, device.logi_device());
            }

            // Desc sets: main
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
                    fd.ubuf_.init_ubuf<U_CompoDlightMain>(device.mem_alloc());

                    // Depth
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.depth().image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 0);
                    // Albedo
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.albedo().image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 1);
                    // Normal
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.normal().image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 2);
                    // Material
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.material().image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 3);
                    // U_CompoDlight
                    writer.add_buf_info(fd.ubuf_);
                    writer.add_buf_write(fd.desc_set_, 4);
                }
                writer.apply_all(device.logi_device());
            }

            // Desc sets: shadow map
            {
                const auto sh_count = rp_res.shadow_maps_->dlight_count();
                auto& desc_layout = desclayouts.get(name() + ":shadow_map");

                desc_pool_sh_.init(
                    sh_count, desc_layout.size_info(), device.logi_device()
                );

                auto desc_sets = desc_pool_sh_.alloc(
                    sh_count, desc_layout.layout(), device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < sh_count; i++) {
                    auto& fd = shmap_data_.emplace_back();
                    fd.desc_set_ = desc_sets[i];
                    fd.ubuf_.init_ubuf<U_CompoDlightShadowMap>(device.mem_alloc(
                    ));

                    // Shadow map
                    writer.add_img_info()
                        .set_img_view(rp_res.shadow_maps_->dlight_view_at(i))
                        .set_sampler(device.samplers().get_nearest())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 0);
                    // U_CompoDlightShadowMap
                    writer.add_buf_info(fd.ubuf_);
                    writer.add_buf_write(fd.desc_set_, 1);
                }
                writer.apply_all(device.logi_device());
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(rp_res.gbuf_.compo().format())
                    .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_clear_store();

                builder.color_attach_ref().add_color_attach(0);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclayouts.get(name() + ":main").layout())
                    .desc(desclayouts.get(name() + ":shadow_map").layout())
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/compo_dlight_vert.spv")
                    .add_frag(":asset/spv/compo_dlight_frag.spv");

                builder.rasterization_state().cull_mode_back();

                builder.color_blend_state().add().set_additive_blend();

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_ = builder.build(render_pass_, pipe_layout_);
            }

            // Framebuffers
            {
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(render_pass_)
                    .set_dim(rp_res.gbuf_.width(), rp_res.gbuf_.height())
                    .add_attach(rp_res.gbuf_.compo().image_view());
                for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i)
                    frame_data_[i].fbuf_ = fbuf_cinfo.build(device);

                fbuf_width_ = rp_res.gbuf_.width();
                fbuf_height_ = rp_res.gbuf_.height();
            }

            // Misc
            {
                clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            }

            return;
        }

        ~RpStatesCompoDlight() override {
            for (auto& fd : frame_data_) {
                fd.ubuf_.destroy(device_.mem_alloc());

                if (VK_NULL_HANDLE != fd.fbuf_) {
                    vkDestroyFramebuffer(
                        device_.logi_device(), fd.fbuf_, nullptr
                    );
                    fd.fbuf_ = VK_NULL_HANDLE;
                }
            }

            for (auto& sh_data : shmap_data_) {
                sh_data.ubuf_.destroy(device_.mem_alloc());
            }

            desc_pool_.destroy(device_.logi_device());
            desc_pool_sh_.destroy(device_.logi_device());
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "compo_dlight";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;
            auto& reg = ctxt.cosmos_->reg();
            auto& fd = frame_data_[ctxt.f_index_.get()];
            const VkExtent2D fbuf_ext{ fbuf_width_, fbuf_height_ };

            U_CompoDlightMain ubuf;
            ubuf.view_ = ctxt.view_mat_;
            ubuf.view_inv_ = glm::inverse(ubuf.view_);
            ubuf.proj_ = ctxt.proj_mat_;
            ubuf.proj_inv_ = glm::inverse(ubuf.proj_);
            for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                ubuf.fog_color_density_ = glm::vec4(
                    atmos.fog_color_, atmos.fog_density_
                );
                break;
            }
            fd.ubuf_.set_data(ubuf, device_.mem_alloc());

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

            for (size_t i = 0; i < rp_res_.shadow_maps_->dlight_count(); ++i) {
                const auto e = rp_res_.shadow_maps_->dlight_entt_at(i);
                if (entt::null == e)
                    continue;

                auto& sh_data = shmap_data_.at(i);
                auto shadow_view = rp_res_.shadow_maps_->dlight_view_at(i);
                auto& dlight = reg.get<mirinae::cpnt::DLight>(e);
                auto& tform = reg.get<mirinae::cpnt::Transform>(e);

                U_CompoDlightShadowMap ubuf_sh;
                ubuf_sh
                    .set_dlight_dir(dlight.calc_to_light_dir(ubuf.view_, tform))
                    .set_dlight_color(dlight.color_.scaled_color());
                ubuf_sh.light_mats_[0] =
                    dlight.cascades_.cascades_[0].light_mat_;
                ubuf_sh.light_mats_[1] =
                    dlight.cascades_.cascades_[1].light_mat_;
                ubuf_sh.light_mats_[2] =
                    dlight.cascades_.cascades_[2].light_mat_;
                ubuf_sh.light_mats_[3] =
                    dlight.cascades_.cascades_[3].light_mat_;
                ubuf_sh.cascade_depths_[0] = dlight.cascades_.far_depths_[0];
                ubuf_sh.cascade_depths_[1] = dlight.cascades_.far_depths_[1];
                ubuf_sh.cascade_depths_[2] = dlight.cascades_.far_depths_[2];
                ubuf_sh.cascade_depths_[3] = dlight.cascades_.far_depths_[3];
                sh_data.ubuf_.set_data(ubuf_sh, device_.mem_alloc());

                mirinae::DescSetBindInfo{}
                    .layout(pipe_layout_)
                    .first_set(1)
                    .set(sh_data.desc_set_)
                    .record(cmdbuf);

                vkCmdDraw(cmdbuf, 3, 1, 0, 0);
            }

            vkCmdEndRenderPass(cmdbuf);
        }

    private:
        struct FrameData {
            mirinae::Buffer ubuf_;
            VkDescriptorSet desc_set_;
            VkFramebuffer fbuf_;
        };

        struct ShadowMapData {
            mirinae::Buffer ubuf_;
            VkDescriptorSet desc_set_;
        };

        static entt::entity select_atmos_simple(entt::registry& reg) {
            for (auto entity : reg.view<mirinae::cpnt::AtmosphereSimple>())
                return entity;

            return entt::null;
        }

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;
        std::vector<ShadowMapData> shmap_data_;

        std::shared_ptr<mirinae::ITexture> sky_tex_;
        mirinae::DescPool desc_pool_, desc_pool_sh_;
        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        std::array<VkClearValue, 1> clear_values_;
        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
    };

}  // namespace


// Compo Sky
namespace {

    class RpStatesCompoSky : public mirinae::IRpStates {

    public:
        RpStatesCompoSky(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            namespace cpnt = mirinae::cpnt;
            auto& reg = cosmos.reg();

            // Sky texture
            {
                auto e = this->select_atmos_simple(cosmos.reg());
                auto& atmos = cosmos.reg().get<cpnt::AtmosphereSimple>(e);
                auto& tex = *rp_res.tex_man_;
                if (tex.request_blck(atmos.sky_tex_path_, false)) {
                    sky_tex_ = tex.get(atmos.sky_tex_path_);
                } else {
                    sky_tex_ = tex.missing_tex();
                }
            }

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
                        .set_img_view(sky_tex_->image_view())
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
                    .add(rp_res.gbuf_.depth().format())
                    .ini_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
                    .stor_op(VK_ATTACHMENT_STORE_OP_STORE);
                builder.attach_desc()
                    .add(rp_res.gbuf_.compo().format())
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
                    .set_dim(rp_res.gbuf_.width(), rp_res.gbuf_.height())
                    .add_attach(rp_res.gbuf_.depth().image_view())
                    .add_attach(rp_res.gbuf_.compo().image_view());
                for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i)
                    frame_data_[i].fbuf_ = fbuf_cinfo.build(device);

                fbuf_width_ = rp_res.gbuf_.width();
                fbuf_height_ = rp_res.gbuf_.height();
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

            sky_tex_.reset();
            desc_pool_.destroy(device_.logi_device());
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
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

        static entt::entity select_atmos_simple(entt::registry& reg) {
            for (auto entity : reg.view<mirinae::cpnt::AtmosphereSimple>())
                return entity;

            return entt::null;
        }

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;
        std::shared_ptr<mirinae::ITexture> sky_tex_;
        mirinae::DescPool desc_pool_;
        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        std::array<VkClearValue, 2> clear_values_;
        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
    };

}  // namespace


namespace mirinae::rp::compo {

    URpStates create_rps_dlight(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesCompoDlight>(
            cosmos, rp_res, desclayouts, device
        );
    }

    URpStates create_rps_sky(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesCompoSky>(
            cosmos, rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp::compo
