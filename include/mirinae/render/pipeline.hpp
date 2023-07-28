#pragma once

#include <vulkan/vulkan.h>


namespace mirinae {

    class Pipeline {

    public:
        Pipeline() = default;
        Pipeline(VkPipelineLayout layout);

        void destroy(VkDevice logi_device);

        VkPipelineLayout layout() { return layout_; }

    private:
        VkPipelineLayout layout_ = nullptr;

    };


    Pipeline create_unorthodox_pipeline(const VkExtent2D& swapchain_extent, VkDevice logi_device);

}
