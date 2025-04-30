#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/compo.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"


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
                auto atmos = this->select_atmos_simple(cosmos.reg());
                MIRINAE_ASSERT(nullptr != atmos);
                auto& tex = *rp_res.tex_man_;
                if (tex.request_blck(atmos->sky_tex_path_, false)) {
                    sky_tex_ = tex.get(atmos->sky_tex_path_);
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
                    .add(rp_res.gbuf_.depth_format())
                    .ini_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .fin_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
                    .stor_op(VK_ATTACHMENT_STORE_OP_STORE);
                builder.attach_desc()
                    .add(rp_res.gbuf_.compo_format())
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
                    .depth_write_enable(false)
                    .depth_compare_op(VK_COMPARE_OP_GREATER_OR_EQUAL);

                builder.color_blend_state().add(false, 1);

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_ = builder.build(render_pass_, pipe_layout_);
            }

            // Framebuffers
            {
                for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    mirinae::FbufCinfo fbuf_cinfo;
                    fbuf_cinfo.set_rp(render_pass_)
                        .set_dim(rp_res.gbuf_.width(), rp_res.gbuf_.height())
                        .add_attach(rp_res.gbuf_.depth(i).image_view())
                        .add_attach(rp_res.gbuf_.compo(i).image_view());
                    frame_data_[i].fbuf_ = fbuf_cinfo.build(device);
                }

                fbuf_width_ = rp_res.gbuf_.width();
                fbuf_height_ = rp_res.gbuf_.height();
            }

            // Misc
            {
                clear_values_.at(0).depthStencil = { 0, 0 };
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

            mirinae::ImageMemoryBarrier{}
                .image(ctxt.rp_res_->gbuf_.depth(ctxt.f_index_.get()).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .old_lay(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .new_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .set_src_acc(VK_ACCESS_SHADER_READ_BIT)
                .set_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
                .set_signle_mip_layer()
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                );

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
            if (auto atmos = this->select_atmos_simple(ctxt.cosmos_->reg()))
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

        static const mirinae::cpnt::AtmosphereSimple* select_atmos_simple(
            const entt::registry& reg
        ) {
            using Atmos = mirinae::cpnt::AtmosphereSimple;

            for (auto entity : reg.view<Atmos>()) {
                return &reg.get<Atmos>(entity);
            }

            return nullptr;
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
