#pragma once

#include "mirinae/renderpass/transp.hpp"
#include "mirinae/renderpass/builder.hpp"


namespace {

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
                mirinae::DescLayoutBuilder builder{ name() + ":frame" };
                builder
                    .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_CompoMain
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // dlight
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // slight
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env diffuse
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env specular
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // env lut
                desclayouts.add(builder, device.logi_device());
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
                    .desc(desclayouts.get("transp:frame").layout())
                    .desc(desclayouts.get("gbuf:model").layout())
                    .desc(desclayouts.get("gbuf:actor_skinned").layout())
                    .build(pipe_layout_, device_);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device_ };

                builder.shader_stages()
                    .add_vert(":asset/spv/gbuf_skin_vert.spv")
                    .add_frag(":asset/spv/gbuf_basic_frag.spv");

                builder.vertex_input_state().set_skinned();

                builder.rasterization_state().cull_mode_back();

                builder.depth_stencil_state()
                    .depth_test_enable(true)
                    .depth_write_enable(true);

                builder.color_blend_state().add(true, 1);

                builder.dynamic_state().add_viewport().add_scissor();

                builder.build(render_pass_, pipe_layout_);
            }

            // Frame buffers
            {
                mirinae::FbufCinfo cinfo;
                cinfo.set_rp(render_pass_)
                    .set_dim(rp_res.gbuf_.width(), rp_res.gbuf_.height())
                    .set_layers(1);

                for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    auto& fd = frame_data_.emplace_back();

                    cinfo.add_attach(rp_res.gbuf_.depth(i).image_view())
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
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "transp skinned";
            return name;
        }

        void record(mirinae::RpContext& ctxt) override {
            namespace cpnt = mirinae::cpnt;
        }

    private:
        struct FrameData {
            mirinae::Fbuf fbuf_;
        };

        mirinae::VulkanDevice& device_;

        std::vector<FrameData> frame_data_;
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
