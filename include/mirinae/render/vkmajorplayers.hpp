#pragma once

#include <vector>
#include <string>
#include <optional>

#include <vulkan/vulkan.h>


namespace mirinae {

    class VulkanExtensionsLayers {

    public:
        void add_validation();

        bool are_layers_available() const;

        std::vector<std::string> extensions_;
        std::vector<std::string> layers_;

    };


    class InstanceFactory {

    public:
        InstanceFactory();

        VkInstance create();

        void enable_validation_layer();

        VulkanExtensionsLayers ext_layers_;

    private:
        VkApplicationInfo app_info{};
        VkInstanceCreateInfo create_info{};
        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
        bool validation_layer_enabled_ = false;

    };


    class PhysDevice {

    public:
        void set(VkPhysicalDevice handle, const VkSurfaceKHR surface);
        void clear();
        VkPhysicalDevice get() const { return handle_; }

        std::string make_report_str() const;
        const char* name() const;
        std::optional<uint32_t> graphics_family_index() const;
        std::optional<uint32_t> present_family_index() const;
        bool is_descrete_gpu() const;

    private:
        VkPhysicalDevice handle_ = nullptr;
        VkPhysicalDeviceProperties properties_{};
        VkPhysicalDeviceFeatures features_{};
        std::optional<uint32_t> graphics_family_index_;
        std::optional<uint32_t> present_family_index_;

    };


    class LogiDevice {

    public:
        ~LogiDevice() {
            this->destroy();
        }

        void init(const PhysDevice& phys_device);
        void destroy();

    private:
        VkDevice device_ = nullptr;
        VkQueue graphics_queue_ = nullptr;
        VkQueue present_queue_ = nullptr;

    };


    class VulkanInstance {

    public:
        ~VulkanInstance() {
            this->destroy();
        }

        void init(InstanceFactory& factory);
        void destroy();

        VkInstance get() const { return instance_; }

        VkPhysicalDevice select_phys_device(const VkSurfaceKHR surface);

    private:
        VkInstance instance_ = nullptr;
        VkDebugUtilsMessengerEXT debug_messenger_ = nullptr;

    };

}
