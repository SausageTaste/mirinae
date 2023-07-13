#pragma once

#include <vector>
#include <string>

#include <vulkan/vulkan.h>


namespace mirinae {

    class PhysDevice {

    public:
        PhysDevice() = default;
        PhysDevice(VkPhysicalDevice handle);
        PhysDevice& operator=(VkPhysicalDevice handle);

        std::string make_report_str() const;
        const char* name() const;
        bool is_descrete_gpu() const;

    private:
        VkPhysicalDevice handle_ = nullptr;
        VkPhysicalDeviceProperties properties_{};
        VkPhysicalDeviceFeatures features_{};

    };


    class VulkanInstance {

    public:
        VulkanInstance();
        ~VulkanInstance();

        void destroy();

        VkPhysicalDevice select_phys_device();

    private:
        VkInstance instance_ = nullptr;
        VkDebugUtilsMessengerEXT debug_messenger_ = nullptr;

    };

}
