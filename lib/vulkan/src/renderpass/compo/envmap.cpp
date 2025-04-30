#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/compo.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"


// Compo Envmap
namespace {

    struct U_CompoEnvmapPushConst {
        glm::mat4 view_inv_;
        glm::mat4 proj_inv_;
        glm::vec4 fog_color_density_;
    };


    class RpStatesCompoEnvmap : public mirinae::IRpStates {

    public:
        RpStatesCompoEnvmap(
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
                    .add_img_frag(1)   // depth
                    .add_img_frag(1)   // albedo
                    .add_img_frag(1)   // normal
                    .add_img_frag(1);  // material
                desclayouts.add(builder, device.logi_device());
            }

            // Desc layout: envmaps
            {
                mirinae::DescLayoutBuilder builder{ name() + ":envmaps" };
                builder
                    .add_img_frag(1)   // u_env_diffuse
                    .add_img_frag(1)   // u_env_specular
                    .add_img_frag(1);  // u_env_lut
                desclayouts.add(builder, device.logi_device());
            }

            // Desc sets: main
            {
                auto& desc_layout = desclayouts.get(name() + ":main");

                desc_pool_main_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.size_info(),
                    device.logi_device()
                );

                auto desc_sets = desc_pool_main_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_main_ = desc_sets[i];

                    // Depth
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.depth(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_main_, 0);
                    // Albedo
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.albedo(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_main_, 1);
                    // Normal
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.normal(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_main_, 2);
                    // Material
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.material(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_main_, 3);
                }
                writer.apply_all(device.logi_device());
            }

            // Desc sets: envmaps
            {
                auto& desc_layout = desclayouts.get(name() + ":envmaps");

                desc_pool_env_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.size_info(),
                    device.logi_device()
                );

                auto desc_sets = desc_pool_env_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_env_ = desc_sets[i];

                    // Diffuse envmap
                    writer.add_img_info()
                        .set_img_view(rp_res.envmaps_->diffuse_at(0))
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_env_, 0);

                    // Specular envmap
                    writer.add_img_info()
                        .set_img_view(rp_res.envmaps_->specular_at(0))
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_env_, 1);

                    // LUT
                    writer.add_img_info()
                        .set_img_view(rp_res.envmaps_->brdf_lut())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_env_, 2);
                }
                writer.apply_all(device.logi_device());
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(rp_res.gbuf_.compo_format())
                    .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();

                builder.color_attach_ref().add_color_attach(0);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclayouts.get(name() + ":main").layout())
                    .desc(desclayouts.get(name() + ":envmaps").layout())
                    .add_frag_flag()
                    .pc<U_CompoEnvmapPushConst>()
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/compo_envmap_vert.spv")
                    .add_frag(":asset/spv/compo_envmap_frag.spv");

                builder.rasterization_state().cull_mode_back();

                builder.color_blend_state().add().set_additive_blend();

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_ = builder.build(render_pass_, pipe_layout_);
            }

            // Framebuffers
            {
                fbuf_width_ = rp_res.gbuf_.width();
                fbuf_height_ = rp_res.gbuf_.height();

                for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    mirinae::FbufCinfo fbuf_cinfo;
                    fbuf_cinfo.set_rp(render_pass_)
                        .set_dim(fbuf_width_, fbuf_height_)
                        .add_attach(rp_res.gbuf_.compo(i).image_view());
                    frame_data_[i].fbuf_ = fbuf_cinfo.build(device);
                }
            }

            // Misc
            {
                clear_values_.at(0).color = { 0, 0, 0, 1 };
            }

            return;
        }

        ~RpStatesCompoEnvmap() override {
            for (auto& fd : frame_data_) {
                if (VK_NULL_HANDLE != fd.fbuf_) {
                    vkDestroyFramebuffer(
                        device_.logi_device(), fd.fbuf_, nullptr
                    );
                    fd.fbuf_ = VK_NULL_HANDLE;
                }
            }

            desc_pool_main_.destroy(device_.logi_device());
            desc_pool_env_.destroy(device_.logi_device());
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "compo_envmap";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;
            auto& reg = ctxt.cosmos_->reg();
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
                .first_set(0)
                .set(fd.desc_set_main_)
                .record(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(pipe_layout_)
                .first_set(1)
                .set(fd.desc_set_env_)
                .record(cmdbuf);

            U_CompoEnvmapPushConst pc;
            pc.proj_inv_ = glm::inverse(ctxt.proj_mat_);
            pc.view_inv_ = glm::inverse(ctxt.view_mat_);
            for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                pc.fog_color_density_ = glm::vec4(
                    atmos.fog_color_, atmos.fog_density_
                );
                break;
            }

            mirinae::PushConstInfo{}
                .layout(pipe_layout_)
                .add_stage_frag()
                .record(cmdbuf, pc);

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmdbuf);
        }

    private:
        struct FrameData {
            VkDescriptorSet desc_set_main_ = VK_NULL_HANDLE;
            VkDescriptorSet desc_set_env_ = VK_NULL_HANDLE;
            VkFramebuffer fbuf_ = VK_NULL_HANDLE;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;

        mirinae::DescPool desc_pool_main_, desc_pool_env_;
        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        std::array<VkClearValue, 1> clear_values_;
        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
    };

}  // namespace


namespace mirinae::rp::compo {

    URpStates create_rps_envmap(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesCompoEnvmap>(
            cosmos, rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp::compo
