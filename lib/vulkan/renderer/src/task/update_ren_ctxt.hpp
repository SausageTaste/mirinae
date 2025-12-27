#pragma once

#include "mirinae/lightweight/task.hpp"
#include "mirinae/scene/scene.hpp"
#include "mirinae/vulkan/base/renderpass/common.hpp"
#include "util/flags.hpp"
#include "util/frame_sync.hpp"


namespace mirinae {

    class UpdateRenContext : public DependingTask {

    public:
        void init(
            const Scene& scene,
            FlagShip& flag_ship,
            FrameSync& framesync,
            RpContext& rp_ctxt,
            Swapchain& swapchain,
            VulkanDevice& device
        );

        void prepare();

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override;

        const Scene* scene_ = nullptr;
        FlagShip* flag_ship_ = nullptr;
        FrameSync* framesync_ = nullptr;
        RpContext* ren_ctxt_ = nullptr;
        Swapchain* swapchain_ = nullptr;
        VulkanDevice* device_ = nullptr;
    };

}  // namespace mirinae
