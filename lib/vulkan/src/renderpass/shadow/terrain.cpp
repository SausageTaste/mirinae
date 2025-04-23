#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/shadow.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderee/terrain.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/shadow/bundles.hpp"


namespace {

    class U_ShadowTerrainPushConst {

    public:
        U_ShadowTerrainPushConst& pvm(const glm::mat4& pvm) {
            pvm_ = pvm;
            return *this;
        }

        U_ShadowTerrainPushConst& tile_index(int x, int y) {
            tile_index_count_.x = static_cast<float>(x);
            tile_index_count_.y = static_cast<float>(y);
            return *this;
        }

        U_ShadowTerrainPushConst& tile_count(int x, int y) {
            tile_index_count_.z = static_cast<float>(x);
            tile_index_count_.w = static_cast<float>(y);
            return *this;
        }

        U_ShadowTerrainPushConst& height_map_size(uint32_t x, uint32_t y) {
            height_map_size_fbuf_size_.x = static_cast<float>(x);
            height_map_size_fbuf_size_.y = static_cast<float>(y);
            return *this;
        }

        U_ShadowTerrainPushConst& height_map_size(const VkExtent2D& e) {
            height_map_size_fbuf_size_.x = static_cast<float>(e.width);
            height_map_size_fbuf_size_.y = static_cast<float>(e.height);
            return *this;
        }

        template <typename T>
        U_ShadowTerrainPushConst& fbuf_size(T x, T y) {
            height_map_size_fbuf_size_.z = static_cast<float>(x);
            height_map_size_fbuf_size_.w = static_cast<float>(y);
            return *this;
        }

        U_ShadowTerrainPushConst& fbuf_size(const VkExtent2D& x) {
            height_map_size_fbuf_size_.z = static_cast<float>(x.width);
            height_map_size_fbuf_size_.w = static_cast<float>(x.height);
            return *this;
        }

        U_ShadowTerrainPushConst& height_scale(float x) {
            height_scale_ = x;
            return *this;
        }

        U_ShadowTerrainPushConst& tess_factor(float x) {
            tess_factor_ = x;
            return *this;
        }

    private:
        glm::mat4 pvm_;
        glm::vec4 tile_index_count_;
        glm::vec4 height_map_size_fbuf_size_;
        float height_scale_;
        float tess_factor_;
    };


    class RpStatesShadowTerrain
        : public mirinae::IRpStates
        , public mirinae::RenPassBundle<1> {

    public:
        RpStatesShadowTerrain(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            auto shadow_maps = dynamic_cast<mirinae::ShadowMapBundle*>(
                rp_res_.shadow_maps_.get()
            );
            MIRINAE_ASSERT(shadow_maps);

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(device.img_formats().depth_map())
                    .ini_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .fin_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();

                builder.depth_attach_ref().set(0);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclayouts.get("gbuf_terrain:main").layout())
                    .add_vertex_flag()
                    .add_tesc_flag()
                    .add_tese_flag()
                    .pc<U_ShadowTerrainPushConst>(0)
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/shadow_terrain_vert.spv")
                    .add_tesc(":asset/spv/shadow_terrain_tesc.spv")
                    .add_tese(":asset/spv/shadow_terrain_tese.spv")
                    .add_frag(":asset/spv/shadow_basic_frag.spv");

                builder.input_assembly_state().topology_patch_list();

                builder.tes_state().patch_ctrl_points(4);

                builder.rasterization_state()
                    .depth_clamp_enable(device.has_supp_depth_clamp())
                    .depth_bias(0, 1);

                builder.depth_stencil_state()
                    .depth_test_enable(true)
                    .depth_write_enable(true);

                builder.dynamic_state()
                    .add(VK_DYNAMIC_STATE_DEPTH_BIAS)
                    .add_viewport()
                    .add_scissor();

                pipeline_ = builder.build(render_pass_.get(), pipe_layout_);
            }

            // Misc
            {
                clear_values_.at(0).depthStencil = { 0, 0 };
            }

            return;
        }

        ~RpStatesShadowTerrain() override {
            this->destroy_render_pass_elements(device_);
        }

        const std::string& name() const override {
            static const std::string name = "shadow_terrain";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            namespace cpnt = mirinae::cpnt;

            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;
            const auto shadow_maps = dynamic_cast<mirinae::ShadowMapBundle*>(
                rp_res_.shadow_maps_.get()
            );
            MIRINAE_ASSERT(shadow_maps);

            for (uint32_t i = 0; i < shadow_maps->dlights().count(); ++i) {
                auto& shadow = shadow_maps->dlights().at(i);
                if (shadow.entt() == entt::null)
                    continue;
                auto& dlight = reg.get<cpnt::DLight>(shadow.entt());

                mirinae::ImageMemoryBarrier{}
                    .image(shadow.img(ctxt.f_index_))
                    .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .old_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .new_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .set_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_signle_mip_layer()
                    .record_single(
                        ctxt.cmdbuf_,
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    );

                mirinae::RenderPassBeginInfo{}
                    .rp(render_pass_.get())
                    .fbuf(shadow.fbuf(ctxt.f_index_))
                    .wh(shadow.extent2d())
                    .clear_value_count(clear_values_.size())
                    .clear_values(clear_values_.data())
                    .record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_
                );
                vkCmdSetDepthBias(cmdbuf, -20, 0, -10);

                const auto half_width = shadow.width() / 2.0;
                const auto half_height = shadow.height() / 2.0;
                const std::array<glm::dvec2, 4> offsets{
                    glm::dvec2{ 0, 0 },
                    glm::dvec2{ half_width, 0 },
                    glm::dvec2{ 0, half_height },
                    glm::dvec2{ half_width, half_height },
                };

                mirinae::DescSetBindInfo descset_info{ pipe_layout_ };

                for (size_t cascade_i = 0; cascade_i < 4; ++cascade_i) {
                    const auto& cascade = dlight.cascades_.cascades_.at(
                        cascade_i
                    );
                    auto& offset = offsets.at(cascade_i);

                    mirinae::Viewport{}
                        .set_xy(offset)
                        .set_wh(half_width, half_height)
                        .record_single(cmdbuf);
                    mirinae::Rect2D{}
                        .set_xy(offset)
                        .set_wh(half_width, half_height)
                        .record_scissor(cmdbuf);

                    mirinae::PushConstInfo pc_info;
                    pc_info.layout(pipe_layout_)
                        .add_stage_vert()
                        .add_stage_tesc()
                        .add_stage_tese();

                    for (auto e : reg.view<cpnt::Terrain>()) {
                        auto& terr = reg.get<cpnt::Terrain>(e);

                        auto unit = terr.ren_unit<mirinae::RenUnitTerrain>();
                        if (!unit)
                            continue;
                        if (!unit->is_ready())
                            continue;

                        mirinae::DescSetBindInfo{}
                            .layout(pipe_layout_)
                            .add(unit->desc_set())
                            .record(cmdbuf);

                        glm::dmat4 model_mat(1);
                        if (auto tform = reg.try_get<cpnt::Transform>(e))
                            model_mat = tform->make_model_mat();

                        ::U_ShadowTerrainPushConst pc;
                        pc.pvm(cascade.light_mat_ * model_mat)
                            .tile_count(24, 24)
                            .height_map_size(unit->height_map_size())
                            .fbuf_size(half_width, half_height)
                            .height_scale(64)
                            .tess_factor(terr.tess_factor_);

                        for (int x = 0; x < 24; ++x) {
                            for (int y = 0; y < 24; ++y) {
                                pc.tile_index(x, y);
                                pc_info.record(cmdbuf, pc);
                                vkCmdDraw(cmdbuf, 4, 1, 0, 0);
                            }
                        }
                    }
                }

                vkCmdEndRenderPass(cmdbuf);
            }

            return;
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;
    };

}  // namespace


namespace mirinae::rp {

    URpStates create_rp_states_shadow_terrain(
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<::RpStatesShadowTerrain>(
            rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp
