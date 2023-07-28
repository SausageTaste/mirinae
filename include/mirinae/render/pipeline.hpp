#pragma once

#include <vulkan/vulkan.h>


namespace mirinae {

    class Pipeline {

    public:
        Pipeline(VkPipelineLayout layout);

        void destroy(VkDevice logi_device);

    private:
        VkPipelineLayout layout_;

    };


    Pipeline create_unorthodox_pipeline(const VkExtent2D& swapchain_extent, VkDevice logi_device);

}
