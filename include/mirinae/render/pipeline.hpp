#pragma once

#include "uniform.hpp"


namespace mirinae {

    Pipeline create_unorthodox_pipeline(
        const VkExtent2D& swapchain_extent,
        RenderPass& renderpass,
        DescriptorSetLayout& desclayout,
        LogiDevice& logi_device
    );

}
