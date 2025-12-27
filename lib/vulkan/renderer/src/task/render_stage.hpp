#pragma once

#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/vulkan/base/render/renderee.hpp"
#include "mirinae/vulkan/base/renderpass/common.hpp"
#include "util/cmdbuf_list.hpp"
#include "util/flags.hpp"
#include "util/frame_sync.hpp"


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
    );

}  // namespace mirinae
