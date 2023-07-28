#include "mirinae/render/vkmajorplayers.hpp"

#include <array>
#include <algorithm>
#include <set>
#include <sstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <mirinae/util/konsts.hpp>


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

    std::vector<const char*> make_char_vec(const std::vector<std::string>& strings) {
        std::vector<const char*> output;
        for (auto& x : strings)
            output.push_back(x.c_str());
        return output;
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
            vkDestroyDevice(device_, nullptr);
            device_ = nullptr;
        }

        graphics_queue_ = nullptr;
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
        if (nullptr != handle_) {
            vkDestroySemaphore(logi_device.get(), handle_, nullptr);
            handle_ = nullptr;
        }
    }

}


// Fence
namespace mirinae {

    void Fence::init(LogiDevice& logi_device) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        if (vkCreateFence(logi_device.get(), &fenceInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }

    void Fence::destroy(LogiDevice& logi_device) {
        if (nullptr != handle_) {
            vkDestroyFence(logi_device.get(), handle_, nullptr);
            handle_ = nullptr;
        }
    }

    void Fence::wait(LogiDevice& logi_device) {
        if (nullptr != handle_)
            vkWaitForFences(logi_device.get(), 1, &handle_, VK_TRUE, UINT64_MAX);
        else
            spdlog::warn("Tried to wait on a fence that is not created");
    }

    void Fence::reset(LogiDevice& logi_device) {
        if (nullptr != handle_)
            vkResetFences(logi_device.get(), 1, &handle_);
        else
            spdlog::warn("Tried to reset a fence that is not created");
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
        views_.resize(images_.size());
        for (size_t i = 0; i < images_.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = images_.at(i);
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = format_;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (VK_SUCCESS != vkCreateImageView(logi_device.get(), &createInfo, nullptr, &views_[i])) {
                throw std::runtime_error("Failed to create image view");
            }
        }
    }

    void Swapchain::destroy(LogiDevice& logi_device) {
        for (auto view : views_)
            vkDestroyImageView(logi_device.get(), view, nullptr);
        views_.clear();

        if (nullptr != swapchain_) {
            vkDestroySwapchainKHR(logi_device.get(), swapchain_, nullptr);
            swapchain_ = nullptr;
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


// RenderPass
namespace mirinae {

    void RenderPass::init(VkFormat swapchain_format, LogiDevice& logi_device) {
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

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(logi_device.get(), &renderPassInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void RenderPass::destroy(LogiDevice& logi_device) {
        if (nullptr != handle_) {
            vkDestroyRenderPass(logi_device.get(), handle_, nullptr);
            handle_ = nullptr;
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
        if (nullptr != pipeline_) {
            vkDestroyPipeline(logi_device.get(), pipeline_, nullptr);
            pipeline_ = nullptr;
        }

        if (nullptr != layout_) {
            vkDestroyPipelineLayout(logi_device.get(), layout_, nullptr);
            layout_ = nullptr;
        }
    }

}


// Framebuffer
namespace mirinae {

    void Framebuffer::init(const VkExtent2D& swapchain_extent, VkImageView view, RenderPass& renderpass, LogiDevice& logi_device) {
        VkImageView attachments[] = {
            view
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderpass.get();
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchain_extent.width;
        framebufferInfo.height = swapchain_extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(logi_device.get(), &framebufferInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }

    void Framebuffer::destroy(LogiDevice& logi_device) {
        if (nullptr != handle_) {
            vkDestroyFramebuffer(logi_device.get(), handle_, nullptr);
            handle_ = nullptr;
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
        if (nullptr != handle_) {
            vkDestroyCommandPool(logi_device.get(), handle_, nullptr);
            handle_ = nullptr;
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

}
