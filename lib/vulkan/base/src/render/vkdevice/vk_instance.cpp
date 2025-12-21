#include "render/vkdevice/vk_instance.hpp"

#include "mirinae/lightweight/konsts.hpp"
#include "mirinae/vulkan/base/render/vkcheck.hpp"
#include "render/vkdevice/phys_device.hpp"


namespace {

    constexpr bool ENABLE_SYNC_VALIDATION = true;


    std::vector<VkLayerProperties> get_vk_layers() {
        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);

        std::vector<VkLayerProperties> layers(count);
        vkEnumerateInstanceLayerProperties(&count, layers.data());
        return layers;
    }

    spdlog::level::level_enum convert_enum(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity
    ) {
        switch (severity) {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                return spdlog::level::level_enum::trace;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                return spdlog::level::level_enum::info;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                return spdlog::level::level_enum::warn;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                return spdlog::level::level_enum::err;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
                return spdlog::level::level_enum::trace;
            default:
                return spdlog::level::level_enum::trace;
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL callback_vk_debug(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    ) {
        SPDLOG_LOGGER_CALL(
            spdlog::default_logger_raw(),
            convert_enum(messageSeverity),
            "validation layer: {}",
            pCallbackData->pMessage
        );

        return VK_FALSE;
    }

    void populate_create_info(VkDebugUtilsMessengerCreateInfoEXT& create_info) {
        create_info = {};
        create_info.sType =
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = callback_vk_debug;
    }

    VkDebugUtilsMessengerEXT create_debug_msger(VkInstance instance) {
        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
        );
        if (nullptr == func) {
            SPDLOG_ERROR(
                "Failed to get Vulkan function: vkCreateDebugUtilsMessengerEXT"
            );
            return VK_NULL_HANDLE;
        }

        VkDebugUtilsMessengerCreateInfoEXT cinfo;
        populate_create_info(cinfo);

        VkDebugUtilsMessengerEXT debug_messenger;
        if (VK_SUCCESS != func(instance, &cinfo, nullptr, &debug_messenger)) {
            SPDLOG_ERROR("Failed to create debug utils messenger");
            return VK_NULL_HANDLE;
        }

        return debug_messenger;
    }

    void destroy_debug_msger(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator
    ) {
        constexpr auto FUNC_NAME = "vkDestroyDebugUtilsMessengerEXT";
        auto ptr = vkGetInstanceProcAddr(instance, FUNC_NAME);
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(ptr);
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }

}  // namespace


// VulkanExtensionsLayers
namespace mirinae {

    void VulkanExtensionsLayers::add_validation() {
        extensions_.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        layers_.push_back("VK_LAYER_KHRONOS_validation");
    }

    bool VulkanExtensionsLayers::are_layers_available() const {
        const auto available_layers = ::get_vk_layers();
        for (auto& layer_name : layers_) {
            bool layer_found = false;
            for (auto& layer_properties : available_layers) {
                if (layer_name == layer_properties.layerName) {
                    layer_found = true;
                    break;
                }
            }

            if (!layer_found)
                return false;
        }
        return true;
    }

}  // namespace mirinae


// InstanceFactory
namespace mirinae {

    InstanceFactory::InstanceFactory() {
        {
            app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            app_info.pApplicationName = "mirinapp";
            app_info.applicationVersion = VK_MAKE_VERSION(
                mirinae::ENGINE_VERSION_MAJOR,
                mirinae::ENGINE_VERSION_MINOR,
                mirinae::ENGINE_VERSION_PATCH
            );
            app_info.pEngineName = mirinae::ENGINE_NAME;
            app_info.engineVersion = VK_MAKE_VERSION(
                mirinae::ENGINE_VERSION_MAJOR,
                mirinae::ENGINE_VERSION_MINOR,
                mirinae::ENGINE_VERSION_PATCH
            );
            app_info.apiVersion = VK_API_VERSION_1_1;
        }

        {
            cinfo_.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            cinfo_.enabledLayerCount = 0;
        }
    }

    VkInstance InstanceFactory::create() {
        cinfo_.pApplicationInfo = &app_info;

        const auto ext_name_ptrs = make_char_vec(ext_layers_.extensions_);
        cinfo_.enabledExtensionCount = ext_name_ptrs.size();
        cinfo_.ppEnabledExtensionNames = ext_name_ptrs.data();

        const auto layer_name_ptrs = make_char_vec(ext_layers_.layers_);
        cinfo_.enabledLayerCount = layer_name_ptrs.size();
        cinfo_.ppEnabledLayerNames = layer_name_ptrs.data();

        if (validation_layer_enabled_) {
            ::populate_create_info(debug_create_info);
            cinfo_.pNext = &debug_create_info;

            if (ENABLE_SYNC_VALIDATION) {
                validation_features_.sType =
                    VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
                enable_validation_features_[0] =
                    VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
                validation_features_.enabledValidationFeatureCount = 1;
                validation_features_.pEnabledValidationFeatures =
                    enable_validation_features_;
                debug_create_info.pNext = &validation_features_;
            } else {
                debug_create_info.pNext = nullptr;
            }
        } else
            cinfo_.pNext = nullptr;

        VkInstance instance;
        VK_CHECK(vkCreateInstance(&cinfo_, nullptr, &instance));
        return instance;
    }

}  // namespace mirinae


// VulkanInstance
namespace mirinae {

    VulkanInstance::~VulkanInstance() { this->destroy(); }

    void VulkanInstance::init(InstanceFactory& factory) {
        this->destroy();
        instance_ = factory.create();
        debug_messenger_ = ::create_debug_msger(instance_);
    }

    void VulkanInstance::destroy() {
        if (VK_NULL_HANDLE != debug_messenger_) {
            ::destroy_debug_msger(instance_, debug_messenger_, nullptr);
            debug_messenger_ = VK_NULL_HANDLE;
        }
        if (VK_NULL_HANDLE != instance_) {
            vkDestroyInstance(instance_, nullptr);
            instance_ = VK_NULL_HANDLE;
        }
    }

    VkInstance VulkanInstance::get() const { return instance_; }

    VkPhysicalDevice VulkanInstance::select_phys_device(
        const VkSurfaceKHR surface
    ) {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance_, &count, nullptr);
        if (0 == count) {
            SPDLOG_ERROR("There is no GPU with Vulkan support");
            return VK_NULL_HANDLE;
        }

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance_, &count, devices.data());

        VkPhysicalDevice selected = VK_NULL_HANDLE;
        double best_score = -1;

        for (auto handle : devices) {
            mirinae::PhysDevice phys_device;
            phys_device.set(handle, surface);

            if (!phys_device.graphics_family_index())
                continue;

            double this_score = 0;
            if (phys_device.is_descrete_gpu())
                this_score += 1000;

            if (this_score > best_score) {
                best_score = this_score;
                selected = handle;
            }
        }

        return selected;
    }

}  // namespace mirinae


// Free functions
namespace mirinae {

    std::vector<const char*> make_char_vec(
        const std::vector<std::string>& strings
    ) {
        std::vector<const char*> output;
        for (auto& x : strings) output.push_back(x.c_str());
        return output;
    }

}  // namespace mirinae
