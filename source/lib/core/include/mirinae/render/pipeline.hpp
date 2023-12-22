#pragma once

#include "mirinae/platform/filesys.hpp"

#include "uniform.hpp"


namespace mirinae {

    Pipeline create_unorthodox_pipeline(
        const VkExtent2D& swapchain_extent,
        RenderPass& renderpass,
        DescriptorSetLayout& desclayout,
        IFilesys& filesys,
        VkDevice logi_device
    );

}
