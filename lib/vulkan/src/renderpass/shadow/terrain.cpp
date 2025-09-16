#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/shadow/shadow.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
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

        template <typename T>
        U_ShadowTerrainPushConst& fbuf_size(T x, T y) {
            fbuf_size_.x = static_cast<float>(x);
            fbuf_size_.y = static_cast<float>(y);
            return *this;
        }

        U_ShadowTerrainPushConst& fbuf_size(const VkExtent2D& x) {
            fbuf_size_.x = static_cast<float>(x.width);
            fbuf_size_.y = static_cast<float>(x.height);
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
        glm::vec2 fbuf_size_;
        float height_scale_;
        float tess_factor_;
    };

}  // namespace


// Tasks
namespace { namespace task {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            const mirinae::ShadowMapBundle& shadow_maps,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            reg_ = &reg;
            rp_ = &rp;
            shadow_maps_ = &shadow_maps;
            cmd_pool_ = &cmd_pool;
            device_ = &device;
        }

        void prepare(const mirinae::RpCtxt& ctxt) { ctxt_ = &ctxt; }

        enki::ITaskSet& fence() { return fence_; }

        void collect_cmdbuf(std::vector<VkCommandBuffer>& out) {
            if (VK_NULL_HANDLE != cmdbuf_) {
                out.push_back(cmdbuf_);
            }
        }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            cmdbuf_ = cmd_pool_->get(ctxt_->f_index_, tid, *device_);
            if (cmdbuf_ == VK_NULL_HANDLE)
                return;

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            this->record_dlight(
                cmdbuf_, *rp_, *ctxt_, *reg_, shadow_maps_->dlights()
            );
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void record_dlight(
            const VkCommandBuffer cmdbuf,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt,
            const entt::registry& reg,
            const mirinae::DlightShadowMapBundle& dlights
        ) {
            for (uint32_t i = 0; i < dlights.count(); ++i) {
                auto& shadow = dlights.at(i);
                auto dlight = reg.try_get<mirinae::cpnt::DLight>(shadow.entt());
                if (!dlight)
                    continue;

                mirinae::ImageMemoryBarrier{}
                    .image(shadow.img(ctxt.f_index_))
                    .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .old_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .new_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .set_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_signle_mip_layer()
                    .record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                    );

                const auto fbuf_size = shadow.extent2d();
                const mirinae::Viewport viewport{ fbuf_size };
                const mirinae::Rect2D rect2d{ fbuf_size };

                mirinae::DescSetBindInfo descset_info{ rp.pipe_layout() };

                const auto layer_count = dlight->cascades_.cascades_.size();
                for (size_t layer = 0; layer < layer_count; ++layer) {
                    auto& cascade = dlight->cascades_.cascades_.at(layer);

                    mirinae::RenderPassBeginInfo{}
                        .rp(rp.render_pass())
                        .fbuf(shadow.fbuf(ctxt.f_index_, layer))
                        .wh(fbuf_size)
                        .clear_value_count(rp.clear_value_count())
                        .clear_values(rp.clear_values())
                        .record_begin(cmdbuf);

                    vkCmdBindPipeline(
                        cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                    );
                    vkCmdSetDepthBias(cmdbuf, -20, 0, -10);

                    viewport.record_single(cmdbuf);
                    rect2d.record_scissor(cmdbuf);

                    mirinae::PushConstInfo pc_info;
                    pc_info.layout(rp.pipe_layout())
                        .add_stage_vert()
                        .add_stage_tesc()
                        .add_stage_tese();

                    for (auto e : reg.view<mirinae::cpnt::Terrain>()) {
                        auto& terr = reg.get<mirinae::cpnt::Terrain>(e);

                        auto unit = terr.ren_unit<mirinae::RenUnitTerrain>();
                        if (!unit)
                            continue;
                        if (!unit->is_ready())
                            continue;

                        mirinae::DescSetBindInfo{}
                            .layout(rp.pipe_layout())
                            .add(unit->desc_set())
                            .record(cmdbuf);

                        glm::dmat4 model_mat(1);
                        if (auto tf = reg.try_get<mirinae::cpnt::Transform>(e))
                            model_mat = tf->make_model_mat();

                        ::U_ShadowTerrainPushConst pc;
                        pc.pvm(cascade.light_mat_ * model_mat)
                            .fbuf_size(fbuf_size)
                            .height_scale(terr.height_scale_)
                            .tess_factor(terr.tess_factor_);
                        pc_info.record(cmdbuf, pc);

                        unit->draw_indexed(cmdbuf);
                    }
                }

                vkCmdEndRenderPass(cmdbuf);
            }
        }

        const mirinae::DebugLabel DEBUG_LABEL{
            "Shadow Terrain", 0.38, 0.38, 0.38
        };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const mirinae::ShadowMapBundle* shadow_maps_ = nullptr;
        const entt::registry* reg_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        RpTask() {}

        void init(
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            const mirinae::ShadowMapBundle& shadow_maps,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(reg, rp, shadow_maps, cmd_pool, device);
        }

        std::string_view name() const override { return "shadow terrain"; }

        void prepare(const mirinae::RpCtxt& ctxt) override {
            record_tasks_.prepare(ctxt);
        }

        void collect_cmdbuf(std::vector<VkCommandBuffer>& out) override {
            record_tasks_.collect_cmdbuf(out);
        }

        enki::ITaskSet* record_task() override { return &record_tasks_; }

        enki::ITaskSet* record_fence() override {
            return &record_tasks_.fence();
        }

    private:
        DrawTasks record_tasks_;
    };

}}  // namespace ::task


namespace {

    class RpStatesShadowTerrain
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<1> {

    public:
        RpStatesShadowTerrain(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
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
                    .desc(rp_res_.desclays_.get("gbuf_terrain:main").layout())
                    .add_vertex_flag()
                    .add_tesc_flag()
                    .add_tese_flag()
                    .pc<U_ShadowTerrainPushConst>(0)
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                using Vertex = mirinae::RenUnitTerrain::Vertex;
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/shadow_terrain_vert.spv")
                    .add_tesc(":asset/spv/shadow_terrain_tesc.spv")
                    .add_tese(":asset/spv/shadow_terrain_tese.spv")
                    .add_frag(":asset/spv/shadow_terrain_frag.spv");

                builder.vertex_input_state()
                    .add_binding<Vertex>()
                    .add_attrib_vec3(offsetof(Vertex, pos_))
                    .add_attrib_vec2(offsetof(Vertex, texco_));

                builder.input_assembly_state().topology_patch_list();

                builder.tes_state().patch_ctrl_points(4);

                builder.rasterization_state()
                    .depth_clamp_enable(device.features().depthClamp)
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

        std::string_view name() const override { return "shadow terrain"; }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            const auto shadow_maps = static_cast<mirinae::ShadowMapBundle*>(
                rp_res_.shadow_maps_.get()
            );

            auto out = std::make_unique<task::RpTask>();
            out->init(
                cosmos_.reg(), *this, *shadow_maps, rp_res_.cmd_pool_, device_
            );
            return out;
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_shadow_terrain(RpCreateBundle& cbundle) {
        return std::make_unique<::RpStatesShadowTerrain>(
            cbundle.cosmos_, cbundle.rp_res_, cbundle.device_
        );
    }

}  // namespace mirinae::rp
