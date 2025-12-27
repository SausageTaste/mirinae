#include "task/render_stage.hpp"


namespace mirinae {

    RenderStage::RenderStage() : StageTask("vulan renderer") {
        init_static_ = create_init_static_model_task();
        init_skinned_ = create_init_skinned_model_task();

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

    void RenderStage::init(
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
        update_ren_ctxt_.init(
            cosmos.scene(), flag_ship, framesync, rp_ctxt, swapchain, device
        );

        init_static_->init(cosmos.scene(), device, model_mgr, rp_ctxt, rp_res);

        init_skinned_->init(cosmos.scene(), device, model_mgr, rp_ctxt, rp_res);

        update_dlight_.init(cosmos, swapchain);

        update_atmos_epic_.init(
            mirinae::MAX_FRAMES_IN_FLIGHT, cosmos.reg(), rp_ctxt, device
        );

        render_passes_.init(
            cmdbufs, flag_ship, rp_ctxt, rp_res, resize_func, passes, device
        );
    }

    enki::ITaskSet* RenderStage::get_fence() { return &fence_; }

    void RenderStage::ExecuteRange(enki::TaskSetPartition range, uint32_t tid) {
        update_ren_ctxt_.prepare();
        init_static_->prepare();
        init_skinned_->prepare();
        update_dlight_.prepare();
        update_atmos_epic_.prepare();
        render_passes_.prepare();
    }

}  // namespace mirinae
