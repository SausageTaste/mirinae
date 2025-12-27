#pragma once

#include "mirinae/lightweight/task.hpp"
#include "mirinae/vulkan/base/renderee/atmos.hpp"
#include "task/init_model.hpp"
#include "task/ren_passes.hpp"
#include "task/update_dlight.hpp"
#include "task/update_ren_ctxt.hpp"


namespace mirinae {

    class RenderStage : public StageTask {

    public:
        RenderStage();

        void init(
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
        );

        UpdateRenContext update_ren_ctxt_;
        std::unique_ptr<IInitModelTask> init_static_;
        std::unique_ptr<IInitModelTask> init_skinned_;
        UpdateDlight update_dlight_;
        TaskAtmosEpic update_atmos_epic_;
        RenderPassesTask render_passes_;

    private:
        enki::ITaskSet* get_fence() override;
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override;

        FenceTask fence_;
    };

}  // namespace mirinae
