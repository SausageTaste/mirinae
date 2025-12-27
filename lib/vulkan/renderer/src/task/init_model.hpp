#pragma once

#include "mirinae/lightweight/task.hpp"
#include "mirinae/scene/scene.hpp"
#include "mirinae/vulkan/base/renderpass/common.hpp"


namespace mirinae {

    struct IInitModelTask : public DependingTask {
        virtual void init(
            Scene& scene,
            VulkanDevice& device,
            IModelManager& model_mgr,
            RpCtxt& rp_ctxt,
            RpResources& rp_res
        ) = 0;

        virtual void prepare() = 0;
    };


    std::unique_ptr<IInitModelTask> create_init_static_model_task();
    std::unique_ptr<IInitModelTask> create_init_skinned_model_task();

}  // namespace mirinae
