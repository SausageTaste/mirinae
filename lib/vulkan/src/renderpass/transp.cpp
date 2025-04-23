#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/transp.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/transform.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"


namespace {

    class U_TranspSkinnedFrame {

    public:
        U_TranspSkinnedFrame& set_proj(const glm::dmat4& v) {
            proj_ = v;
            proj_inv_ = glm::inverse(v);
            return *this;
        }

        U_TranspSkinnedFrame& set_view(const glm::dmat4& v) {
            view_ = v;
            view_inv_ = glm::inverse(v);
            return *this;
        }

        // Dlights

        U_TranspSkinnedFrame& set_dlight_mat(size_t idx, const glm::mat4& m) {
            dlight_mats_[idx] = m;
            return *this;
        }

        template <typename T>
        U_TranspSkinnedFrame& set_dlight_cascade_depths(const T* arr) {
            dlight_cascade_depths_.x = static_cast<float>(arr[0]);
            dlight_cascade_depths_.y = static_cast<float>(arr[1]);
            dlight_cascade_depths_.z = static_cast<float>(arr[2]);
            dlight_cascade_depths_.w = static_cast<float>(arr[3]);
            return *this;
        }

        U_TranspSkinnedFrame& set_dlight_dir(const glm::dvec3& v) {
            dlight_dir_.x = static_cast<float>(v.x);
            dlight_dir_.y = static_cast<float>(v.y);
            dlight_dir_.z = static_cast<float>(v.z);
            return *this;
        }

        U_TranspSkinnedFrame& set_dlight_color(const glm::vec3& v) {
            dlight_color_.x = v.r;
            dlight_color_.y = v.g;
            dlight_color_.z = v.b;
            return *this;
        }

        // Misc

        template <typename T>
        U_TranspSkinnedFrame& set_mie_anisotropy(T v) {
            mie_anisotropy_ = static_cast<float>(v);
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

        float mie_anisotropy_;
    };


    class RpMasterTranspSkinned : public mirinae::IRpStates {

    public:
        RpMasterTranspSkinned(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device) {
            // Desc layout
            {
                auto& shadow = *rp_res.shadow_maps_;
                const auto dlights = shadow.dlights().count();
                const auto slights = shadow.slight_count();

                mirinae::DescLayoutBuilder builder{ this->name() + ":frame" };
                builder
                    .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_CompoMain
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env diffuse
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env specular
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // env lut
                desclayouts.add(builder, device.logi_device());
            }

            // Ubuf
            {
                for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    auto& fd = frame_data_.emplace_back();
                    fd.ubuf_.init_ubuf<::U_TranspSkinnedFrame>(device.mem_alloc(
                    ));
                }
            }

            // Desc sets
            {
                auto& layout = desclayouts.get(this->name() + ":frame");
                auto& shadow = *rp_res.shadow_maps_;
                auto& dlights = shadow.dlights();
                const auto slight_count = shadow.slight_count();

                desc_pool_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    layout.size_info(),
                    device.logi_device()
                );

                const auto desc_sets = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter w;
                for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    const mirinae::FrameIndex f_idx(i);
                    auto& fd = frame_data_[i];
                    fd.desc_ = desc_sets[i];

                    // Ubuf
                    w.add_buf_info(fd.ubuf_).add_buf_write(fd.desc_, 0);
                    // Dlight shadow maps
                    for (uint32_t i_dl = 0; i_dl < dlights.count(); ++i_dl) {
                        constexpr auto LAYOUT =
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        w.add_img_info()
                            .set_img_view(dlights.at(i_dl).view(f_idx))
                            .set_layout(LAYOUT)
                            .set_sampler(device.samplers().get_shadow());
                        break;
                    }
                    w.add_sampled_img_write(fd.desc_, 1);
                    // Slight shadow maps
                    for (uint32_t i_sl = 0; i_sl < slight_count; ++i_sl) {
                        constexpr auto LAYOUT =
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        const auto view = shadow.slight_view_at(i_sl);
                        w.add_img_info()
                            .set_img_view(view)
                            .set_layout(LAYOUT)
                            .set_sampler(device.samplers().get_shadow());
                        break;
                    }
                    w.add_sampled_img_write(fd.desc_, 2);
                    // Env diffuse
                    w.add_img_info()
                        .set_img_view(rp_res.envmaps_->diffuse_at(0))
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        .set_sampler(device.samplers().get_cubemap());
                    w.add_sampled_img_write(fd.desc_, 3);
                    // Env specular
                    w.add_img_info()
                        .set_img_view(rp_res.envmaps_->specular_at(0))
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        .set_sampler(device.samplers().get_cubemap());
                    w.add_sampled_img_write(fd.desc_, 4);
                    // Env lut
                    w.add_img_info()
                        .set_img_view(rp_res.envmaps_->brdf_lut())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        .set_sampler(device.samplers().get_linear());
                    w.add_sampled_img_write(fd.desc_, 5);
                }
                w.apply_all(device.logi_device());
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(rp_res.gbuf_.depth_format())
                    .ini_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .fin_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();
                builder.attach_desc()
                    .add(rp_res.gbuf_.compo_format())
                    .ini_lay(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_lay(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();

                builder.depth_attach_ref().set(0);
                builder.color_attach_ref().add_color_attach(1);  // compo

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclayouts.get(this->name() + ":frame").layout())
                    .desc(desclayouts.get("gbuf:model").layout())
                    .desc(desclayouts.get("gbuf:actor_skinned").layout())
                    .build(pipe_layout_, device_);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device_ };

                builder.shader_stages()
                    .add_vert(":asset/spv/transp_skin_vert.spv")
                    .add_frag(":asset/spv/transp_basic_frag.spv");

                builder.vertex_input_state().set_skinned();

                builder.depth_stencil_state()
                    .depth_test_enable(true)
                    .depth_write_enable(false);

                builder.color_blend_state().add(true, 1);

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_ = builder.build(render_pass_, pipe_layout_);
            }

            // Frame buffers
            {
                mirinae::FbufCinfo cinfo;
                cinfo.set_rp(render_pass_)
                    .set_dim(rp_res.gbuf_.width(), rp_res.gbuf_.height())
                    .set_layers(1);

                for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    auto& fd = frame_data_[i];

                    cinfo.clear_attach()
                        .add_attach(rp_res.gbuf_.depth(i).image_view())
                        .add_attach(rp_res.gbuf_.compo(i).image_view());
                    fd.fbuf_.init(cinfo.get(), device_.logi_device());
                }
            }

            // Misc
            {
                clear_values_[0].depthStencil = { 0, 0 };
                clear_values_[1].color = { 0, 0, 0, 1 };
            }
        }

        ~RpMasterTranspSkinned() {
            for (auto& fd : frame_data_) {
                fd.fbuf_.destroy(device_.logi_device());
                fd.ubuf_.destroy(device_.mem_alloc());
            }

            desc_pool_.destroy(device_.logi_device());
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "transp skinned";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            namespace cpnt = mirinae::cpnt;
            auto& shadows = *ctxt.rp_res_->shadow_maps_;
            const auto& reg = ctxt.cosmos_->reg();
            auto& fd = frame_data_[ctxt.f_index_.get()];
            const auto cmdbuf = ctxt.cmdbuf_;
            const auto fbuf_ext = ctxt.rp_res_->gbuf_.extent();
            const auto view_inv = glm::inverse(ctxt.view_mat_);

            {
                U_TranspSkinnedFrame ubuf;
                ubuf.set_proj(ctxt.proj_mat_).set_view(ctxt.view_mat_);

                for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                    auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                    ubuf.set_mie_anisotropy(atmos.mie_anisotropy_);
                    break;
                }

                for (uint32_t i = 0; i < shadows.dlights().count(); ++i) {
                    auto& dlight = shadows.dlights().at(i);
                    if (entt::null == dlight.entt())
                        continue;

                    auto dlit = reg.try_get<cpnt::DLight>(dlight.entt());
                    if (!dlit)
                        continue;

                    const auto& light = reg.get<cpnt::DLight>(dlight.entt());
                    const auto& tform = reg.get<cpnt::Transform>(dlight.entt());
                    const auto& cascades = light.cascades_;
                    const auto& casc_arr = cascades.cascades_;

                    ubuf.set_dlight_mat(0, casc_arr[0].light_mat_ * view_inv)
                        .set_dlight_mat(1, casc_arr[1].light_mat_ * view_inv)
                        .set_dlight_mat(2, casc_arr[2].light_mat_ * view_inv)
                        .set_dlight_mat(3, casc_arr[3].light_mat_ * view_inv)
                        .set_dlight_cascade_depths(cascades.far_depths_.data())
                        .set_dlight_dir(
                            light.calc_to_light_dir(ctxt.view_mat_, tform)
                        )
                        .set_dlight_color(light.color_.scaled_color());
                    break;
                }

                fd.ubuf_.set_data(ubuf, device_.mem_alloc());
            }

            mirinae::ImageMemoryBarrier{}
                .image(ctxt.rp_res_->gbuf_.depth(ctxt.f_index_.get()).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .old_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .add_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
                .add_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                );
            mirinae::ImageMemoryBarrier{}
                .image(ctxt.rp_res_->gbuf_.compo(ctxt.f_index_.get()).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .add_src_acc(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .add_dst_acc(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );

            mirinae::RenderPassBeginInfo{}
                .rp(render_pass_.get())
                .fbuf(fd.fbuf_.get())
                .wh(fbuf_ext)
                .clear_value_count(clear_values_.size())
                .clear_values(clear_values_.data())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.get()
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo descset_info{ pipe_layout_.get() };
            descset_info.first_set(0).set(fd.desc_).record(cmdbuf);

            for (auto& pair : ctxt.draw_sheet_->skinned_trs_) {
                auto& unit = *pair.unit_;
                descset_info.first_set(1)
                    .set(unit.get_desc_set(ctxt.f_index_.get()))
                    .record(cmdbuf);

                unit.record_bind_vert_buf(cmdbuf);

                for (auto& actor : pair.actors_) {
                    descset_info.first_set(2)
                        .set(actor.actor_->get_desc_set(ctxt.f_index_.get()))
                        .record(cmdbuf);

                    vkCmdDrawIndexed(cmdbuf, unit.vertex_count(), 1, 0, 0, 0);
                }
            }

            vkCmdEndRenderPass(cmdbuf);
        }

    private:
        struct FrameData {
            mirinae::Fbuf fbuf_;
            mirinae::Buffer ubuf_;
            VkDescriptorSet desc_ = VK_NULL_HANDLE;
        };

        mirinae::VulkanDevice& device_;
        std::vector<FrameData> frame_data_;
        mirinae::DescPool desc_pool_;

        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
        std::array<VkClearValue, 2> clear_values_;
    };

}  // namespace


namespace mirinae::rp {

    URpStates create_rp_states_transp_skinned(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<::RpMasterTranspSkinned>(
            rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp
