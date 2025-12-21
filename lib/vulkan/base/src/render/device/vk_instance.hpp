#pragma once

#include <string>
#include <vector>

#include <vulkan/vulkan.h>


namespace mirinae {

    std::vector<const char*> make_char_vec(
        const std::vector<std::string>& strings
    );


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
        void enable_validation_layer() { validation_layer_enabled_ = true; }

        VulkanExtensionsLayers ext_layers_;

    private:
        VkApplicationInfo app_info{};
        VkInstanceCreateInfo cinfo_{};
        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
        VkValidationFeaturesEXT validation_features_{};
        VkValidationFeatureEnableEXT enable_validation_features_[1];
        bool validation_layer_enabled_ = false;
    };


    class VulkanInstance {

    public:
        ~VulkanInstance();

        void init(InstanceFactory& factory);
        void destroy();
        VkInstance get() const;

        VkPhysicalDevice select_phys_device(const VkSurfaceKHR surface);

    private:
        VkInstance instance_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    };


}  // namespace mirinae
