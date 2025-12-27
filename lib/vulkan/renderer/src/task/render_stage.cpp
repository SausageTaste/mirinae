#include "task/render_stage.hpp"

#include "mirinae/vulkan/base/renderee/atmos.hpp"
#include "task/init_model.hpp"
#include "task/ren_passes.hpp"
#include "task/update_dlight.hpp"
#include "task/update_ren_ctxt.hpp"


namespace {


    class RenderStage : public mirinae::StageTask {

    public:
        RenderStage() : StageTask("vulan renderer") {
            init_static_ = mirinae::create_init_static_model_task();
            init_skinned_ = mirinae::create_init_skinned_model_task();

            update_ren_ctxt_.succeed(this);
            init_static_->succeed(&update_ren_ctxt_);
            init_skinned_->succeed(&update_ren_ctxt_);
            update_dlight_.succeed(&update_ren_ctxt_);
            update_atmos_epic_.succeed(&update_ren_ctxt_);
            render_passes_.succeed(
                init_static_.get(),
                init_skinned_.get(),
                &update_dlight_,
                &update_atmos_epic_
            );
            fence_.succeed(&render_passes_);
        }

        void init(
            mirinae::CmdBufList& cmdbufs,
            mirinae::CosmosSimulator& cosmos,
            mirinae::FlagShip& flag_ship,
            mirinae::FrameSync& framesync,
            mirinae::IModelManager& model_mgr,
            mirinae::RpContext& rp_ctxt,
            mirinae::RpResources& rp_res,
            std::function<bool()> resize_func,
            std::vector<std::unique_ptr<mirinae::IRpBase>>& passes,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        ) {
            update_ren_ctxt_.init(
                cosmos.scene(), flag_ship, framesync, rp_ctxt, swapchain, device
            );

            init_static_->init(
                cosmos.scene(), device, model_mgr, rp_ctxt, rp_res
            );

            init_skinned_->init(
                cosmos.scene(), device, model_mgr, rp_ctxt, rp_res
            );

            update_dlight_.init(cosmos, swapchain);

            update_atmos_epic_.init(
                mirinae::MAX_FRAMES_IN_FLIGHT, cosmos.reg(), rp_ctxt, device
            );

            render_passes_.init(
                cmdbufs, flag_ship, rp_ctxt, rp_res, resize_func, passes, device
            );
        }

    private:
        enki::ITaskSet* get_fence() { return &fence_; }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) {
            update_ren_ctxt_.prepare();
            init_static_->prepare();
            init_skinned_->prepare();
            update_dlight_.prepare();
            update_atmos_epic_.prepare();
            render_passes_.prepare();
        }

        mirinae::UpdateRenContext update_ren_ctxt_;
        std::unique_ptr<mirinae::IInitModelTask> init_static_;
        std::unique_ptr<mirinae::IInitModelTask> init_skinned_;
        mirinae::UpdateDlight update_dlight_;
        mirinae::TaskAtmosEpic update_atmos_epic_;
        mirinae::RenderPassesTask render_passes_;

        mirinae::FenceTask fence_;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<StageTask> create_render_stage(
        CmdBufList& cmdbufs,
        CosmosSimulator& cosmos,
        FlagShip& flag_ship,
        FrameSync& framesync,
        IModelManager& model_mgr,
        RpContext& rp_ctxt,
        RpResources& rp_res,
        std::function<bool()> resize_func,
        std::vector<std::unique_ptr<IRpBase>>& passes,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        auto stage = std::make_unique<RenderStage>();
        stage->init(
            cmdbufs,
            cosmos,
            flag_ship,
            framesync,
            model_mgr,
            rp_ctxt,
            rp_res,
            resize_func,
            passes,
            swapchain,
            device
        );
        return stage;
    }

}  // namespace mirinae
