#include "renderpass/envmap/envmap.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/atmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lightweight/task.hpp"

#include "render/cmdbuf.hpp"
#include "renderpass/builder.hpp"
#include "renderpass/envmap/cubemap.hpp"


namespace {

    struct FrameData {
        mirinae::HRpImage sky_view_lut_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;

}  // namespace


// Tasks
namespace {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() {
            fence_.succeed(this);
            timer_.set_min();
        }

        void init(
            const ::FrameDataArr& frame_data,
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            mirinae::EnvmapBundle& envmaps,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            rp_ = &rp;
            reg_ = &reg;
            frame_data_ = &frame_data;
            envmaps_ = &envmaps;
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
            cmdbuf_ = VK_NULL_HANDLE;
            if (!timer_.check_if_elapsed(mirinae::ENVMAP_UPDATE_INTERVAL))
                return;

            cmdbuf_ = cmd_pool_->get(ctxt_->f_index_, tid, *device_);
            if (cmdbuf_ == VK_NULL_HANDLE)
                return;

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            this->record(
                cmdbuf_,
                frame_data_->at(ctxt_->f_index_.get()),
                *reg_,
                *envmaps_->begin(),
                *rp_
            );
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const ::FrameData& fd,
            const entt::registry& reg,
            const mirinae::EnvmapBundle::Item& env_item,
            const mirinae::IRenPass& rp
        ) {
            mirinae::ImageMemoryBarrier barrier_pre{};
            barrier_pre.image(env_item.cube_map_.base().depth_img(0))
                .set_src_access(VK_ACCESS_SHADER_READ_BIT)
                .set_dst_access(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                );

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.render_pass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = glm::perspectiveRH_ZO(
                sung::to_radians(90.0), 1.0, 1000.0, 0.1
            );

            auto& cube_map = env_item.cube_map_;
            auto& base_cube = cube_map.base();

            mirinae::Viewport{}
                .set_wh(base_cube.width(), base_cube.height())
                .record_single(cmdbuf);
            mirinae::Rect2D{}
                .set_wh(base_cube.width(), base_cube.height())
                .record_scissor(cmdbuf);
            rp_info.wh(base_cube.width(), base_cube.height());

            mirinae::U_EnvSkyPushConst pc;
            for (auto e : reg.view<mirinae::cpnt::AtmosphereEpic>()) {
                auto& atmos = reg.get<mirinae::cpnt::AtmosphereEpic>(e);
                pc.atmos_radius_bottom_ = atmos.radius_bottom();
                break;
            }
            for (auto e : reg.view<mirinae::cpnt::DLight>()) {
                auto& light = reg.get<mirinae::cpnt::DLight>(e);
                auto& tform = reg.get<mirinae::cpnt::Transform>(e);
                const auto dir = light.calc_to_light_dir(glm::dmat4(1), tform);
                pc.sun_dir_w_ = glm::vec4{ dir, 0 };
                break;
            }

            for (int i = 0; i < 6; ++i) {
                rp_info.fbuf(base_cube.face_fbuf(i)).record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                mirinae::DescSetBindInfo{}
                    .layout(rp.pipe_layout())
                    .set(fd.desc_set_)
                    .record(cmdbuf);

                pc.proj_view_ = proj_mat * mirinae::CUBE_VIEW_MATS[i];

                mirinae::PushConstInfo{}
                    .layout(rp.pipe_layout())
                    .add_stage_vert()
                    .add_stage_frag()
                    .record(cmdbuf, pc);

                vkCmdDraw(cmdbuf, 36, 1, 0, 0);

                vkCmdEndRenderPass(cmdbuf);
            }
        }

        const mirinae::DebugLabel DEBUG_LABEL{
            "Envmap Sky Atmos", 0.5, 0.78, 0.52
        };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;
        sung::MonotonicRealtimeTimer timer_;

        const ::FrameDataArr* frame_data_ = nullptr;
        const entt::registry* reg_ = nullptr;
        const mirinae::EnvmapBundle* envmaps_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        RpTask() {}

        void init(
            const ::FrameDataArr& frame_data,
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            mirinae::EnvmapBundle& envmaps,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(frame_data, reg, rp, envmaps, cmd_pool, device);
        }

        std::string_view name() const override { return "env atmos sky"; }

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

}  // namespace


namespace {

    class RpBase
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<2> {

    public:
        RpBase(mirinae::RpCreateBundle& cbundle)
            : device_(cbundle.device_)
            , rp_res_(cbundle.rp_res_)
            , cosmos_(cbundle.cosmos_) {
            // Clear values
            {
                clear_values_.at(0).depthStencil = { 0, 0 };
                clear_values_.at(1).color = { 0, 0, 0, 1 };
            }

            // Image references
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_[i];
                fd.sky_view_lut_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("sky view LUT:sky_view_lut_f#{}", i), name_s()
                );
            }

            // Desc layout
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
                builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
                rp_res_.desclays_.add(builder, device_.logi_device());
            }

            auto& desclay = rp_res_.desclays_.get(name_s() + ":main");

            // Desc sets
            {
                desc_pool_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desclay.size_info(),
                    device_.logi_device()
                );

                auto desc_sets = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desclay.layout(),
                    device_.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_ = desc_sets[i];

                    writer.add_img_info()
                        .set_img_view(fd.sky_view_lut_->view_.get())
                        .set_sampler(device_.samplers().get_cubemap())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 0);
                }
                writer.apply_all(device_.logi_device());
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(device_.img_formats().depth_map())
                    .ini_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .fin_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                builder.attach_desc()
                    .add(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
                    .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_clear_store();

                builder.depth_attach_ref().set(0);
                builder.color_attach_ref().add_color_attach(1);

                builder.subpass_dep().add().preset_single();

                render_pass_.reset(
                    builder.build(device_.logi_device()), device_
                );
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclay.layout())
                    .add_vertex_flag()
                    .add_frag_flag()
                    .pc<mirinae::U_EnvSkyPushConst>()
                    .build(pipe_layout_, device_);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device_ };

                builder.shader_stages()
                    .add_vert(":asset/spv/env_atmos_sky_vert.spv")
                    .add_frag(":asset/spv/env_atmos_sky_frag.spv");

                builder.color_blend_state().add(false, 1);

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_.reset(
                    builder.build(render_pass_, pipe_layout_), device_
                );
            }
        }

        ~RpBase() override {
            for (auto& fd : frame_data_) {
                rp_res_.ren_img_.free_img(fd.sky_view_lut_->id(), name_s());
                fd.desc_set_ = VK_NULL_HANDLE;
            }

            desc_pool_.destroy(device_.logi_device());
            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "env atmos sky"; }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<::RpTask>();
            out->init(
                frame_data_,
                cosmos_.reg(),
                *this,
                *static_cast<mirinae::EnvmapBundle*>(rp_res_.envmaps_.get()),
                rp_res_.cmd_pool_,
                device_
            );
            return out;
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;
        mirinae::CosmosSimulator& cosmos_;

        FrameDataArr frame_data_;
        mirinae::DescPool desc_pool_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_env_sky_atmos(RpCreateBundle& cbundle) {
        return std::make_unique<RpBase>(cbundle);
    }

}  // namespace mirinae::rp
