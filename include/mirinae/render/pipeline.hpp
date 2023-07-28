#pragma once

#include "mirinae/render/vkmajorplayers.hpp"


namespace mirinae {

    Pipeline create_unorthodox_pipeline(const VkExtent2D& swapchain_extent, RenderPass& renderpass, LogiDevice& logi_device);

}
