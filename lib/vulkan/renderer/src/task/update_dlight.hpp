#pragma once

#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/vulkan/base/render/vkdevice.hpp"


namespace mirinae {

    class UpdateDlight : public DependingTask {

    public:
        void init(CosmosSimulator& cosmos, Swapchain& swhain);
        void prepare();

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override;

        CosmosSimulator* cosmos_ = nullptr;
        Swapchain* swhain_;
    };

}  // namespace mirinae
