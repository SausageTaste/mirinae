#pragma once

#include <vulkan/vulkan.h>


namespace mirinae {

    const char* to_str(VkFormat format);
    const char* to_str(VkResult result);
    const char* to_str(VkPresentModeKHR present_mode);
    const char* to_str(VkSurfaceTransformFlagBitsKHR transform_flag);

}  // namespace mirinae
