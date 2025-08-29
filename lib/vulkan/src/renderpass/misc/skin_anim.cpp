#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/misc/misc.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/mem_cinfo.hpp"
#include "mirinae/render/renderee.hpp"
#include "mirinae/renderee/ren_actor_skinned.hpp"
#include "mirinae/renderpass/builder.hpp"


namespace {

    namespace cpnt = mirinae::cpnt;


    struct U_SkinAnim {
        float time_ = 0;
    };


    struct FrameData {};

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;


    class DrawSheet {

    public:
        void clear() {
            opa_.clear();
            trs_.clear();
        }

        void fetch(const entt::registry& reg) {
            for (const auto e : reg.view<cpnt::MdlActorSkinned>()) {
                auto& mactor = reg.get<cpnt::MdlActorSkinned>(e);
                if (!mactor.model_)
                    continue;
                auto renmdl = mactor.get_model<mirinae::RenderModelSkinned>();
                if (!renmdl)
                    continue;
                auto actor = mactor.get_actor<mirinae::RenderActorSkinned>();
                if (!actor)
                    continue;

                glm::dmat4 model_mat(1);
                if (auto tfrom = reg.try_get<cpnt::Transform>(e))
                    model_mat = tfrom->make_model_mat();

                const auto unit_count = renmdl->runits_.size();
                for (size_t i = 0; i < unit_count; ++i) {
                    if (!mactor.visibility_.get(i))
                        continue;

                    auto& dst = opa_.emplace_back();
                    dst.unit_ = &renmdl->runits_[i];
                    dst.actor_ = actor;
                    dst.model_mat_ = model_mat;
                    dst.unit_idx_ = i;
                }

                const auto unit_trs_count = renmdl->runits_alpha_.size();
                for (size_t i = 0; i < unit_trs_count; ++i) {
                    if (!mactor.visibility_.get(i + unit_count))
                        continue;

                    auto& dst = trs_.emplace_back();
                    dst.unit_ = &renmdl->runits_alpha_[i];
                    dst.actor_ = actor;
                    dst.model_mat_ = model_mat;
                    dst.unit_idx_ = i;
                }
            }
        }

        struct RenderPair {
            const mirinae::RenderUnitSkinned* unit_ = nullptr;
            const mirinae::RenderActorSkinned* actor_ = nullptr;
            glm::dmat4 model_mat_{ 1 };
            size_t unit_idx_ = 0;
        };

        auto& opa() const { return opa_; }

    private:
        std::vector<RenderPair> opa_;
        std::vector<RenderPair> trs_;
    };

}  // namespace


namespace {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const entt::registry& reg,
            const mirinae::IPipelinePair& rp,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            cmd_pool_ = &cmd_pool;
            device_ = &device;
            reg_ = &reg;
            rp_ = &rp;
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

            draw_set_.clear();
            draw_set_.fetch(*reg_);

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            const auto res = this->record(
                cmdbuf_, draw_set_, *rp_, *ctxt_, *device_
            );
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);

            if (!res) {
                cmdbuf_ = VK_NULL_HANDLE;
                return;
            }
        }

        static bool record(
            const VkCommandBuffer cmdbuf,
            const ::DrawSheet& draw_set,
            const mirinae::IPipelinePair& rp,
            const mirinae::RpCtxt& ctxt,
            mirinae::VulkanDevice& device
        ) {
            mirinae::DescSetBindInfo descset_info{ rp.pipe_layout() };
            descset_info.bind_point(VK_PIPELINE_BIND_POINT_COMPUTE);

            ::U_SkinAnim push_const;
            push_const.time_ = static_cast<float>(ctxt.dt_);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, rp.pipeline()
            );

            for (auto& pair : draw_set.opa()) {
                auto& unit = *pair.unit_;
                auto& actor = *pair.actor_;
                auto& ac_unit = actor.get_runit(pair.unit_idx_);

                unit.record_bind_vert_buf(cmdbuf);

                descset_info.set(ac_unit.descset(ctxt.f_index_)).record(cmdbuf);

                mirinae::PushConstInfo{}
                    .layout(rp.pipe_layout())
                    .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .record(cmdbuf, push_const);

                vkCmdDispatch(cmdbuf, unit.vertex_count(), 1, 1);
            }

            return true;
        }

        const mirinae::DebugLabel DEBUG_LABEL{ "Skin anim" };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;
        ::DrawSheet draw_set_;

        const entt::registry* reg_ = nullptr;
        const mirinae::IPipelinePair* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        void init(
            const entt::registry& reg,
            const mirinae::IPipelinePair& rp,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(reg, rp, cmd_pool, device);
        }

        std::string_view name() const override { return "skin anim"; }

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

    class RenderPassDebug
        : public mirinae::IRpBase
        , public mirinae::IPipelinePair {

    public:
        RenderPassDebug(mirinae::RpCreateBundle& bundle)
            : cosmos_(bundle.cosmos_)
            , rp_res_(bundle.rp_res_)
            , device_(bundle.device_) {
            // Desc layout
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
                builder.add_sbuf(VK_SHADER_STAGE_COMPUTE_BIT, 1)
                    .add_sbuf(VK_SHADER_STAGE_COMPUTE_BIT, 1)
                    .add_ubuf(VK_SHADER_STAGE_COMPUTE_BIT, 1);
                rp_res_.desclays_.add(builder, device_.logi_device());
            }

            const auto& desclay = rp_res_.desclays_.get("skin_anim:main");

            // Pipeline
            {
                mirinae::PipelineLayoutBuilder{}
                    .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                    .pc<::U_SkinAnim>()
                    .desc(desclay.layout())
                    .build(pipe_layout_, device_);

                pipeline_ = mirinae::create_compute_pipeline(
                    ":asset/spv/misc_skin_anim_comp.spv", pipe_layout_, device_
                );
            }
        }

        ~RenderPassDebug() {
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        std::string_view name() const override { return "skin_anim"; }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto task = std::make_unique<RpTask>();
            task->init(cosmos_.reg(), *this, rp_res_.cmd_pool_, device_);
            return task;
        }

        VkPipeline pipeline() const override { return pipeline_; }
        VkPipelineLayout pipe_layout() const override { return pipe_layout_; }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        FrameDataArr frame_data_;
        mirinae::DescPool desc_pool_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<mirinae::IRpBase> create_rp_skin_anim(
        RpCreateBundle& bundle
    ) {
        return std::make_unique<RenderPassDebug>(bundle);
    }

}  // namespace mirinae::rp
