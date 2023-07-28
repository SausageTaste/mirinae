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


    class SwapChainSupportDetails {

    public:
        void init(VkSurfaceKHR surface, VkPhysicalDevice phys_device);

        bool is_complete() const;
        VkSurfaceFormatKHR choose_format() const;
        VkPresentModeKHR choose_present_mode() const;
        VkExtent2D choose_extent(uint32_t fbuf_width, uint32_t fbuf_height) const;
        uint32_t choose_image_count() const;
        auto get_transform() const { return capabilities_.currentTransform; }

    private:
        VkSurfaceCapabilitiesKHR capabilities_;
        std::vector<VkSurfaceFormatKHR> formats_;
        std::vector<VkPresentModeKHR> present_modes_;

    };


    class PhysDevice {

    public:
        void set(VkPhysicalDevice handle, const VkSurfaceKHR surface);
        void clear();
        VkPhysicalDevice get() { return handle_; }

        std::string make_report_str() const;
        const char* name() const;
        std::optional<uint32_t> graphics_family_index() const;
        std::optional<uint32_t> present_family_index() const;
        bool is_descrete_gpu() const;

        std::vector<VkExtensionProperties> get_extensions() const;
        size_t count_unsupported_extensions(const std::vector<std::string>& extensions) const;

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

        void init(PhysDevice& phys_device, const std::vector<std::string>& extensions);
        void destroy();

        VkDevice get() {
            return device_;
        }

    private:
        VkDevice device_ = nullptr;
        VkQueue graphics_queue_ = nullptr;
        VkQueue present_queue_ = nullptr;

    };


    class Swapchain {

    public:
        void init(uint32_t fbuf_width, uint32_t fbuf_height, VkSurfaceKHR surface, PhysDevice& phys_device, LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

    private:
        VkSwapchainKHR swapchain_ = nullptr;
        std::vector<VkImage> images_;
        VkFormat format_;
        VkExtent2D extent_;

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
