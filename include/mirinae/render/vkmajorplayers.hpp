#pragma once

#include <vector>
#include <string>

#include <vulkan/vulkan.h>


namespace mirinae {

    class VulkanInstance {

    public:
        VulkanInstance();
        ~VulkanInstance();

        void destroy();

    private:
        VkInstance instance_ = nullptr;
        VkDebugUtilsMessengerEXT debug_messenger_ = nullptr;

    };

}
