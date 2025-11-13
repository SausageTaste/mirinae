#pragma once

#include <vulkan/vulkan.h>


namespace mirinae {

    struct VulkanPlatformFunctions {
        virtual ~VulkanPlatformFunctions() = default;
        virtual VkSurfaceKHR create_surface(VkInstance) = 0;
        virtual void imgui_new_frame() = 0;
    };

}
