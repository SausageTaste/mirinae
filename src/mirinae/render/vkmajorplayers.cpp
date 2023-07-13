#include "mirinae/render/vkmajorplayers.hpp"

#include <set>
#include <sstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <mirinae/util/konsts.hpp>


namespace {

    const std::vector<std::string> VALIDATION_LAYERS{
        "VK_LAYER_KHRONOS_validation",
    };


    spdlog::level::level_enum convert_enum(VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
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
        spdlog::log(convert_enum(messageSeverity), "validation layer: {}", pCallbackData->pMessage);
        return VK_FALSE;
    }

    void populate_create_info(VkDebugUtilsMessengerCreateInfoEXT& create_info) {
        create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = callback_vk_debug;
    }


    VkDebugUtilsMessengerEXT create_debug_msger(VkInstance instance) {
        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (nullptr == func) {
            spdlog::error("Failed to get Vulkan function: vkCreateDebugUtilsMessengerEXT");
            return nullptr;
        }

        VkDebugUtilsMessengerCreateInfoEXT create_info;
        populate_create_info(create_info);

        VkDebugUtilsMessengerEXT debug_messenger;
        if (VK_SUCCESS != func(instance, &create_info, nullptr, &debug_messenger)) {
            spdlog::error("Failed to create debug utils messenger");
            return nullptr;
        }

        return debug_messenger;
    }

    void destroy_debug_msger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }


    std::vector<VkExtensionProperties> get_vk_extensions() {
        uint32_t count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
        return extensions;
    }

    std::vector<VkLayerProperties> get_vk_layers() {
        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);

        std::vector<VkLayerProperties> layers(count);
        vkEnumerateInstanceLayerProperties(&count, layers.data());
        return layers;
    }

    bool is_validation_layer_available() {
        const auto available_layers = ::get_vk_layers();
        for (auto& layer_name : VALIDATION_LAYERS) {
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

}



// InstanceFactory
namespace {

    class InstanceFactory {

    public:
        InstanceFactory() {
            {
                app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                app_info.pApplicationName = "mirinapp";
                app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
                app_info.pEngineName = mirinae::ENGINE_NAME;
                app_info.engineVersion = VK_MAKE_VERSION(mirinae::ENGINE_VERSION_MAJOR, mirinae::ENGINE_VERSION_MINOR, mirinae::ENGINE_VERSION_PATCH);
                app_info.apiVersion = VK_API_VERSION_1_0;
            }

            {
                create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                create_info.enabledLayerCount = 0;
            }

            {
                uint32_t glfwExtensionCount = 0;
                const char** glfwExtensions;
                glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
                extension_requests_.insert(extension_requests_.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);
            }
        }

        VkInstance create() {
            create_info.pApplicationInfo = &app_info;

            std::vector<const char*> ext_name_ptrs;
            for (auto& x : extension_requests_)
                ext_name_ptrs.push_back(x.c_str());
            create_info.enabledExtensionCount = ext_name_ptrs.size();
            create_info.ppEnabledExtensionNames = ext_name_ptrs.data();

            std::vector<const char*> layer_name_ptrs;
            for (auto& x : layer_requests_)
                layer_name_ptrs.push_back(x.c_str());
            create_info.enabledLayerCount = layer_name_ptrs.size();
            create_info.ppEnabledLayerNames = layer_name_ptrs.data();

            if (validation_layer_enabled_) {
                populate_create_info(debug_create_info);
                create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
            }
            else
                create_info.pNext = nullptr;

            VkInstance instance;
            if (VK_SUCCESS != vkCreateInstance(&create_info, nullptr, &instance))
                throw std::runtime_error("failed to create instance!");

            return instance;
        }

        bool enable_validation_layer() {
            if (!::is_validation_layer_available()) {
                spdlog::error("Validation layers not available");
                return false;
            }

            layer_requests_.insert(layer_requests_.end(), VALIDATION_LAYERS.begin(), VALIDATION_LAYERS.end());
            extension_requests_.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            validation_layer_enabled_ = true;
            return true;
        }

    private:
        VkApplicationInfo app_info{};
        VkInstanceCreateInfo create_info{};
        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};

        std::vector<std::string> extension_requests_;
        std::vector<std::string> layer_requests_;
        bool validation_layer_enabled_ = false;

    };

}


// PhysDevice auxiliaries
namespace {

    auto get_queue_family_props(VkPhysicalDevice phys_device) {
        std::vector<VkQueueFamilyProperties> output;
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &count, nullptr);
        if (0 == count)
            return output;

        output.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &count, output.data());
        return output;
    }

}


// PhysDevice
namespace mirinae {

    void PhysDevice::set(VkPhysicalDevice handle, const VkSurfaceKHR surface) {
        if (nullptr == handle) {
            spdlog::error("PhysDevice::set has recieved a nullptr");
            this->clear();
            return;
        }

        handle_ = handle;
        vkGetPhysicalDeviceProperties(handle_, &properties_);
        vkGetPhysicalDeviceFeatures(handle_, &features_);

        const auto queue_family = ::get_queue_family_props(handle_);
        for (int i = 0; i < queue_family.size(); ++i) {
            if (queue_family[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                graphics_family_index_ = i;

            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(handle_, i, surface, &present_support);
            if (present_support)
                present_family_index_ = i;
        }
    }

    void PhysDevice::clear() {
        handle_ = nullptr;
        properties_ = {};
        features_ = {};
        graphics_family_index_ = std::nullopt;
        present_family_index_ = std::nullopt;
    }

    std::string PhysDevice::make_report_str() const {
        std::stringstream ss;

        ss << "==================================\n";
        ss << properties_.deviceName << '\n';
        ss << "----------------------------------\n";

        ss << "Device type                ";
        switch (properties_.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                ss << "Integrated\n";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                ss << "Descrete\n";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                ss << "Virtual\n";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                ss << "CPU\n";
                break;
            default:
                ss << "Unknown\n";
                break;
        }

        ss << "Max image 2D dimension     " << properties_.limits.maxImageDimension2D << '\n';
        ss << "    push constant          " << properties_.limits.maxPushConstantsSize << '\n';
        ss << "    memory alloc count     " << properties_.limits.maxMemoryAllocationCount << '\n';
        ss << "    sampler alloc count    " << properties_.limits.maxSamplerAllocationCount << '\n';
        ss << "    bound descriptor sets  " << properties_.limits.maxBoundDescriptorSets << '\n';

        ss << "==================================\n";
        return ss.str();
    }

    const char* PhysDevice::name() const {
        return properties_.deviceName;
    }

    std::optional<uint32_t> PhysDevice::graphics_family_index() const {
        return graphics_family_index_;
    }

    std::optional<uint32_t> PhysDevice::present_family_index() const {
        return present_family_index_;
    }

    bool PhysDevice::is_descrete_gpu() const {
        return this->properties_.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    }

}


// LogiDevice
namespace mirinae {

    void LogiDevice::init(const PhysDevice& phys_device) {
        std::set<uint32_t> unique_queue_families{ 
            phys_device.graphics_family_index().value(), 
            phys_device.present_family_index().value(),
        };

        float queue_priority = 1;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        for (auto queue_fam : unique_queue_families) {
            auto& queueCreateInfo = queueCreateInfos.emplace_back();
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queue_fam;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queue_priority;
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = queueCreateInfos.size();
        createInfo.pEnabledFeatures = &deviceFeatures;

        if (vkCreateDevice(phys_device.get(), &createInfo, nullptr, &device_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device_, phys_device.graphics_family_index().value(), 0, &graphics_queue_);
        vkGetDeviceQueue(device_, phys_device.present_family_index().value(), 0, &present_queue_);
    }

    void LogiDevice::destroy() {
        if (nullptr != device_) {
            vkDestroyDevice(device_, nullptr);
            device_ = nullptr;
        }

        graphics_queue_ = nullptr;
    }

}


// VulkanInstance
namespace mirinae {

    VulkanInstance::VulkanInstance() {
        ::InstanceFactory factory;
        factory.enable_validation_layer();
        instance_ = factory.create();
        debug_messenger_ = ::create_debug_msger(instance_);
    }

    VulkanInstance::~VulkanInstance() {
        this->destroy();
    }

    void VulkanInstance::destroy() {
        if (nullptr != instance_) {
            vkDestroyInstance(instance_, nullptr);
            instance_ = nullptr;
        }
        if (nullptr != debug_messenger_) {
            ::destroy_debug_msger(instance_, debug_messenger_, nullptr);
            debug_messenger_ = nullptr;
        }

    }

    VkPhysicalDevice VulkanInstance::select_phys_device(const VkSurfaceKHR surface) {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance_, &count, nullptr);
        if (0 == count) {
            spdlog::error("There is no GPU with Vulkan support");
            return nullptr;
        }

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance_, &count, devices.data());

        VkPhysicalDevice output = nullptr;
        for (auto handle : devices) {
            PhysDevice phys_device;
            phys_device.set(handle, surface);

            if (!phys_device.is_descrete_gpu())
                continue;
            if (!phys_device.graphics_family_index().has_value())
                continue;

            output = handle;
        }

        return output;
    }

}
