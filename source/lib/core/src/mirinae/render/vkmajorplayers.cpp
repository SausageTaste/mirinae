#include "mirinae/render/vkmajorplayers.hpp"

#include <array>
#include <algorithm>
#include <set>
#include <sstream>

#include <spdlog/spdlog.h>

#include "mirinae/util/konsts.hpp"


namespace {

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
            return VK_NULL_HANDLE;
        }

        VkDebugUtilsMessengerCreateInfoEXT create_info;
        populate_create_info(create_info);

        VkDebugUtilsMessengerEXT debug_messenger;
        if (VK_SUCCESS != func(instance, &create_info, nullptr, &debug_messenger)) {
            spdlog::error("Failed to create debug utils messenger");
            return VK_NULL_HANDLE;
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

    std::vector<const char*> make_char_vec(const std::vector<std::string>& strings) {
        std::vector<const char*> output;
        for (auto& x : strings)
            output.push_back(x.c_str());
        return output;
    }


    VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, VkDevice device) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange.aspectMask = aspect_flags;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VkImageView image_view;
        if (VK_SUCCESS != vkCreateImageView(device, &view_info, nullptr, &image_view))
            throw std::runtime_error("Failed to create image view");
        return image_view;
    }

}



// InstanceFactory
namespace mirinae {

    InstanceFactory::InstanceFactory() {
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
    }

    VkInstance InstanceFactory::create() {
        create_info.pApplicationInfo = &app_info;

        const auto ext_name_ptrs = ::make_char_vec(ext_layers_.extensions_);
        create_info.enabledExtensionCount = ext_name_ptrs.size();
        create_info.ppEnabledExtensionNames = ext_name_ptrs.data();

        const auto layer_name_ptrs = ::make_char_vec(ext_layers_.layers_);
        create_info.enabledLayerCount = layer_name_ptrs.size();
        create_info.ppEnabledLayerNames = layer_name_ptrs.data();

        if (validation_layer_enabled_) {
            ::populate_create_info(debug_create_info);
            create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
        }
        else
            create_info.pNext = nullptr;

        VkInstance instance;
        if (VK_SUCCESS != vkCreateInstance(&create_info, nullptr, &instance))
            throw std::runtime_error("failed to create instance!");

        return instance;
    }

    void InstanceFactory::enable_validation_layer() {
        validation_layer_enabled_ = true;
    }

}


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

}


// SwapChainSupportDetails
namespace mirinae {

    void SwapChainSupportDetails::init(VkSurfaceKHR surface, VkPhysicalDevice phys_device) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &capabilities_);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            formats_.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &formatCount, formats_.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            present_modes_.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &presentModeCount, present_modes_.data());
        }
    }

    bool SwapChainSupportDetails::is_complete() const {
        if (formats_.empty())
            return false;
        if (present_modes_.empty())
            return false;
        return true;
    }

    VkSurfaceFormatKHR SwapChainSupportDetails::choose_format() const {
        for (const auto& available_format : formats_) {
            if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return available_format;
            }
        }
        return formats_[0];
    }

    VkPresentModeKHR SwapChainSupportDetails::choose_present_mode() const {
        for (const auto& availablePresentMode : present_modes_) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D SwapChainSupportDetails::choose_extent(uint32_t fbuf_width, uint32_t fbuf_height) const {
        if (capabilities_.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
            return capabilities_.currentExtent;
        }
        else {
            VkExtent2D actualExtent{ fbuf_width, fbuf_height };

            actualExtent.width = std::clamp(actualExtent.width, capabilities_.minImageExtent.width, capabilities_.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities_.minImageExtent.height, capabilities_.maxImageExtent.height);

            return actualExtent;
        }
    }

    uint32_t SwapChainSupportDetails::choose_image_count() const {
        auto image_count = capabilities_.minImageCount + 1;
        if (capabilities_.maxImageCount > 0 && image_count > capabilities_.maxImageCount) {
            image_count = capabilities_.maxImageCount;
        }
        return image_count;
    }

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

        ss << "Device type                               ";
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

        ss << "Max image 2D dimension                    " << properties_.limits.maxImageDimension2D << '\n';
        ss << "    push constant                         " << properties_.limits.maxPushConstantsSize << '\n';
        ss << "    memory alloc count                    " << properties_.limits.maxMemoryAllocationCount << '\n';
        ss << "    sampler alloc count                   " << properties_.limits.maxSamplerAllocationCount << '\n';
        ss << "    bound descriptor sets                 " << properties_.limits.maxBoundDescriptorSets << '\n';
        ss << "    per stage descriptor samplers         " << properties_.limits.maxPerStageDescriptorSamplers << '\n';
        ss << "    per stage descriptor uniform buffers  " << properties_.limits.maxPerStageDescriptorUniformBuffers << '\n';

        ss << "==================================";
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

    bool PhysDevice::is_anisotropic_filtering_supported() const {
        return this->features_.samplerAnisotropy;
    }

    std::vector<VkExtensionProperties> PhysDevice::get_extensions() const {
        std::vector<VkExtensionProperties> output;

        uint32_t count;
        vkEnumerateDeviceExtensionProperties(handle_, nullptr, &count, nullptr);
        if (0 == count)
            return output;

        output.resize(count);
        vkEnumerateDeviceExtensionProperties(handle_, nullptr, &count, output.data());
        return output;
    }

    size_t PhysDevice::count_unsupported_extensions(const std::vector<std::string>& extensions) const {
        const auto available_extensions = this->get_extensions();
        std::set<std::string> required_extensions(extensions.begin(), extensions.end());

        for (const auto& extension : available_extensions) {
            auto found = required_extensions.find(extension.extensionName);
            if (found != required_extensions.end())
                required_extensions.erase(found);
        }

        for (auto& ext_name : required_extensions)
            spdlog::warn("Required extension not available in physical device: {}", ext_name);

        return required_extensions.size();
    }

    VkFormat PhysDevice::find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(handle_, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("Failed to find supported format!");
    }

}


// LogiDevice
namespace mirinae {

    void LogiDevice::init(PhysDevice& phys_device, const std::vector<std::string>& extensions) {
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
        {
            if (phys_device.is_anisotropic_filtering_supported())
                deviceFeatures.samplerAnisotropy = VK_TRUE;
        }

        const auto char_extension = ::make_char_vec(extensions);

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = queueCreateInfos.size();
        createInfo.ppEnabledExtensionNames = char_extension.data();
        createInfo.enabledExtensionCount = char_extension.size();
        createInfo.pEnabledFeatures = &deviceFeatures;

        if (vkCreateDevice(phys_device.get(), &createInfo, nullptr, &device_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device_, phys_device.graphics_family_index().value(), 0, &graphics_queue_);
        vkGetDeviceQueue(device_, phys_device.present_family_index().value(), 0, &present_queue_);
    }

    void LogiDevice::destroy() {
        if (nullptr != device_) {
            vkDeviceWaitIdle(device_);
            vkDestroyDevice(device_, nullptr);
            device_ = nullptr;
        }

        graphics_queue_ = nullptr;
    }

    void LogiDevice::wait_idle() {
        if (nullptr != device_) {
            vkDeviceWaitIdle(device_);
        }
    }

}


// Semaphore
namespace mirinae {

    void Semaphore::init(LogiDevice& logi_device) {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (vkCreateSemaphore(logi_device.get(), &semaphoreInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }

    void Semaphore::destroy(LogiDevice& logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroySemaphore(logi_device.get(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}


// Fence
namespace mirinae {

    void Fence::init(bool init_signaled, LogiDevice& logi_device) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        if (init_signaled)
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateFence(logi_device.get(), &fenceInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }

    void Fence::destroy(LogiDevice& logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyFence(logi_device.get(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    void Fence::wait(LogiDevice& logi_device) {
        if (VK_NULL_HANDLE == handle_) {
            spdlog::warn("Tried to wait on a fence that is not created");
            return;
        }

        vkWaitForFences(logi_device.get(), 1, &handle_, VK_TRUE, UINT64_MAX);
    }

    void Fence::reset(LogiDevice& logi_device) {
        if (VK_NULL_HANDLE == handle_) {
            spdlog::warn("Tried to reset a fence that is not created");
            return;
        }

        vkResetFences(logi_device.get(), 1, &handle_);
    }

}


// Swapchain
namespace mirinae {

    void Swapchain::init(uint32_t fbuf_width, uint32_t fbuf_height, VkSurfaceKHR surface, PhysDevice& phys_device, LogiDevice& logi_device) {
        mirinae::SwapChainSupportDetails swapchain_details;
        swapchain_details.init(surface, phys_device.get());
        if (!swapchain_details.is_complete()) {
            throw std::runtime_error{ "The swapchain is not complete" };
        }

        std::array<uint32_t, 2> queue_family_indices{ *phys_device.graphics_family_index(), *phys_device.present_family_index() };
        VkSwapchainCreateInfoKHR swapchain_create_info{};

        // Fill in swapchain create info
        {
            swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            swapchain_create_info.surface = surface;
            swapchain_create_info.minImageCount = swapchain_details.choose_image_count();
            swapchain_create_info.imageFormat = swapchain_details.choose_format().format;
            swapchain_create_info.imageColorSpace = swapchain_details.choose_format().colorSpace;
            swapchain_create_info.imageExtent = swapchain_details.choose_extent(fbuf_width, fbuf_height);
            swapchain_create_info.imageArrayLayers = 1;
            swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            swapchain_create_info.preTransform = swapchain_details.get_transform();
            swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            swapchain_create_info.presentMode = swapchain_details.choose_present_mode();
            swapchain_create_info.clipped = VK_TRUE;
            swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

            if (queue_family_indices[0] != queue_family_indices[1]) {
                swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                swapchain_create_info.queueFamilyIndexCount = queue_family_indices.size();
                swapchain_create_info.pQueueFamilyIndices = queue_family_indices.data();
            }
            else {
                swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                swapchain_create_info.queueFamilyIndexCount = 0; // Optional
                swapchain_create_info.pQueueFamilyIndices = nullptr; // Optional
            }
        }

        if (VK_SUCCESS != vkCreateSwapchainKHR(logi_device.get(), &swapchain_create_info, nullptr, &swapchain_)) {
            throw std::runtime_error("Failed to create swapchain");
        }

        // Store some data
        {
            format_ = swapchain_create_info.imageFormat;
            extent_ = swapchain_create_info.imageExtent;

            uint32_t image_count = swapchain_details.choose_image_count();
            vkGetSwapchainImagesKHR(logi_device.get(), swapchain_, &image_count, nullptr);
            images_.resize(image_count);
            vkGetSwapchainImagesKHR(logi_device.get(), swapchain_, &image_count, images_.data());
        }

        spdlog::info(
            "Swapchain created: format={}, extent=({}, {}), present_mode={}, image_count={}",
            static_cast<int>(format_),
            extent_.width, extent_.height,
            static_cast<int>(swapchain_create_info.presentMode),
            images_.size()
        );

        // Create views
        for (size_t i = 0; i < images_.size(); i++) {
            views_.push_back(::create_image_view(images_.at(i), format_, VK_IMAGE_ASPECT_COLOR_BIT, logi_device.get()));
        }
    }

    void Swapchain::destroy(LogiDevice& logi_device) {
        for (auto view : views_)
            vkDestroyImageView(logi_device.get(), view, nullptr);
        views_.clear();

        if (VK_NULL_HANDLE != swapchain_) {
            vkDestroySwapchainKHR(logi_device.get(), swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }
    }

    std::optional<ShainImageIndex> Swapchain::acquire_next_image(Semaphore& img_avaiable_semaphore, LogiDevice& logi_device) {
        uint32_t imageIndex;
        const auto result = vkAcquireNextImageKHR(logi_device.get(), swapchain_, UINT64_MAX, img_avaiable_semaphore.get(), VK_NULL_HANDLE, &imageIndex);

        switch (result) {
            case VK_SUCCESS:
            case VK_SUBOPTIMAL_KHR:
                return ShainImageIndex{ imageIndex };
            case VK_ERROR_OUT_OF_DATE_KHR:
            default:
                return std::nullopt;
        }
    }

}


// VulkanInstance
namespace mirinae {

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

    VkPhysicalDevice VulkanInstance::select_phys_device(const VkSurfaceKHR surface) {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance_, &count, nullptr);
        if (0 == count) {
            spdlog::error("There is no GPU with Vulkan support");
            return VK_NULL_HANDLE;
        }

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance_, &count, devices.data());

        VkPhysicalDevice output = VK_NULL_HANDLE;
        for (auto handle : devices) {
            PhysDevice phys_device;
            phys_device.set(handle, surface);

            if (!phys_device.graphics_family_index().has_value())
                continue;

            output = handle;
        }

        return output;
    }

}


// RenderPass
namespace mirinae {

    void RenderPass::init(VkFormat swapchain_format, VkFormat depth_format, LogiDevice& logi_device) {
        this->destroy(logi_device);

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchain_format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depth_format;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = (uint32_t)attachments.size();
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(logi_device.get(), &renderPassInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void RenderPass::destroy(LogiDevice& logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyRenderPass(logi_device.get(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}


// Pipeline
namespace mirinae {

    Pipeline::Pipeline(VkPipeline pipeline, VkPipelineLayout layout) {
        pipeline_ = pipeline;
        layout_ = layout;
    }

    void Pipeline::destroy(LogiDevice& logi_device) {
        if (VK_NULL_HANDLE != pipeline_) {
            vkDestroyPipeline(logi_device.get(), pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }

        if (VK_NULL_HANDLE != layout_) {
            vkDestroyPipelineLayout(logi_device.get(), layout_, nullptr);
            layout_ = VK_NULL_HANDLE;
        }
    }

}


// Framebuffer
namespace mirinae {

    void Framebuffer::init(const VkExtent2D& swapchain_extent, VkImageView view, VkImageView depth_view, RenderPass& renderpass, LogiDevice& logi_device) {
        std::array<VkImageView, 2> attachments{
            view,
            depth_view,
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderpass.get();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchain_extent.width;
        framebufferInfo.height = swapchain_extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(logi_device.get(), &framebufferInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }

    void Framebuffer::destroy(LogiDevice& logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyFramebuffer(logi_device.get(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}


// CommandPool
namespace mirinae {

    void CommandPool::init(uint32_t graphics_queue, LogiDevice& logi_device) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphics_queue;

        if (vkCreateCommandPool(logi_device.get(), &poolInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void CommandPool::destroy(LogiDevice& logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyCommandPool(logi_device.get(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    VkCommandBuffer CommandPool::alloc(LogiDevice& logi_device) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = handle_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(logi_device.get(), &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        return commandBuffer;
    }

    void CommandPool::free(VkCommandBuffer cmdbuf, LogiDevice& logi_device) {
        vkFreeCommandBuffers(logi_device.get(), handle_, 1, &cmdbuf);
    }

    VkCommandBuffer CommandPool::begin_single_time(LogiDevice& logi_device) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = handle_;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(logi_device.get(), &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void CommandPool::end_single_time(VkCommandBuffer cmdbuf, LogiDevice& logi_device) {
        vkEndCommandBuffer(cmdbuf);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;

        vkQueueSubmit(logi_device.graphics_queue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(logi_device.graphics_queue());

        vkFreeCommandBuffers(logi_device.get(), handle_, 1, &cmdbuf);
    }

}


namespace {

    uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties, mirinae::PhysDevice& phys_device) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(phys_device.get(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    bool create_vk_image(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory,
        mirinae::PhysDevice& phys_device,
        mirinae::LogiDevice& logi_device
    ) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(logi_device.get(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(logi_device.get(), image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = ::find_memory_type(memRequirements.memoryTypeBits, properties, phys_device);

        if (vkAllocateMemory(logi_device.get(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            return false;
        }

        vkBindImageMemory(logi_device.get(), image, imageMemory, 0);
        return true;
    }

    void transition_image_layout(
        const VkImage image,
        const uint32_t mip_levels,
        const VkFormat format,
        const VkImageLayout old_layout,
        const VkImageLayout new_layout,
        mirinae::CommandPool& cmd_pool,
        mirinae::LogiDevice& logi_device
    ) {
        auto cmd_buf = cmd_pool.begin_single_time(logi_device);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.srcAccessMask = 0; // TODO
        barrier.dstAccessMask = 0; // TODO
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mip_levels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags src_stage;
        VkPipelineStageFlags dst_stage;
        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else {
            spdlog::error("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            cmd_buf,
            src_stage,
            dst_stage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        cmd_pool.end_single_time(cmd_buf, logi_device);
    }

    void copy_buffer_to_image(
        const VkImage dst_image,
        const VkBuffer src_buffer,
        const uint32_t width,
        const uint32_t height,
        const uint32_t mip_level,
        mirinae::CommandPool& cmd_pool,
        mirinae::LogiDevice& logi_device
    ) {
        auto cmd_buf = cmd_pool.begin_single_time(logi_device);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = mip_level;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(
            cmd_buf,
            src_buffer,
            dst_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        cmd_pool.end_single_time(cmd_buf, logi_device);
    }

}


// TextureImage
namespace mirinae {

    void TextureImage::init(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        mirinae::PhysDevice& phys_device,
        mirinae::LogiDevice& logi_device
    ) {
        const auto result = ::create_vk_image(
            width, height, format, tiling, usage, properties, image_, memory_, phys_device, logi_device
        );

        if (!result) {
            throw std::runtime_error("failed to create image!");
        }

        format_ = format;
        width_ = width;
        height_ = height;
    }

    void TextureImage::destroy(LogiDevice& logi_device) {
        if (VK_NULL_HANDLE != image_) {
            vkDestroyImage(logi_device.get(), image_, nullptr);
            image_ = VK_NULL_HANDLE;
        }

        if (VK_NULL_HANDLE != memory_) {
            vkFreeMemory(logi_device.get(), memory_, nullptr);
            memory_ = VK_NULL_HANDLE;
        }
    }

    void TextureImage::copy_and_transition(VkBuffer staging_buffer, mirinae::CommandPool& cmd_pool, mirinae::LogiDevice& logi_device) {
        ::transition_image_layout(
            image_,
            1,
            format_,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            cmd_pool,
            logi_device
        );

        ::copy_buffer_to_image(
            image_,
            staging_buffer,
            width_,
            height_,
            0,
            cmd_pool,
            logi_device
        );

        ::transition_image_layout(
            image_,
            1,
            format_,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            cmd_pool,
            logi_device
        );
    }

}


// ImageView
namespace mirinae {

    void ImageView::init(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, LogiDevice& logi_device) {
        this->destroy(logi_device);
        this->handle_ = ::create_image_view(image, format, aspect_flags, logi_device.get());
    }

    void ImageView::destroy(LogiDevice& logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyImageView(logi_device.get(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}


// Sampler
namespace mirinae {

    void Sampler::init(PhysDevice& phys_device, LogiDevice& logi_device) {
        this->destroy(logi_device);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = phys_device.is_anisotropic_filtering_supported() ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = phys_device.max_sampler_anisotropy();
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(logi_device.get(), &samplerInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void Sampler::destroy(LogiDevice& logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroySampler(logi_device.get(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }


}
