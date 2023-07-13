#pragma once

#include <vector>
#include <string>
#include <optional>

#include <vulkan/vulkan.h>


namespace mirinae {

    class PhysDevice {

    public:
        PhysDevice() = default;
        PhysDevice(VkPhysicalDevice handle);
        PhysDevice& operator=(VkPhysicalDevice handle);

        void set(VkPhysicalDevice handle);
        void clear();

        std::string make_report_str() const;
        const char* name() const;
        std::optional<uint32_t> graphics_family_index() const;
        bool is_descrete_gpu() const;

    private:
        VkPhysicalDevice handle_ = nullptr;
        VkPhysicalDeviceProperties properties_{};
        VkPhysicalDeviceFeatures features_{};
        std::optional<uint32_t> graphics_family_index_;

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
