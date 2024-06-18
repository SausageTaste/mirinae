#include "mirinae/render/vkdevice.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <sstream>

#include <spdlog/spdlog.h>

#include "mirinae/util/konsts.hpp"


namespace {

    VkSurfaceKHR surface_cast(uint64_t value) {
        static_assert(sizeof(VkSurfaceKHR) == sizeof(uint64_t));
        return *reinterpret_cast<VkSurfaceKHR*>(&value);
    }

    std::vector<const char*> make_char_vec(
        const std::vector<std::string>& strings
    ) {
        std::vector<const char*> output;
        for (auto& x : strings) output.push_back(x.c_str());
        return output;
    }

    std::vector<VkExtensionProperties> get_vk_extensions() {
        uint32_t count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateInstanceExtensionProperties(
            nullptr, &count, extensions.data()
        );
        return extensions;
    }

    std::vector<VkLayerProperties> get_vk_layers() {
        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);

        std::vector<VkLayerProperties> layers(count);
        vkEnumerateInstanceLayerProperties(&count, layers.data());
        return layers;
    }

    auto get_queue_family_props(VkPhysicalDevice phys_device) {
        std::vector<VkQueueFamilyProperties> output;
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &count, nullptr);
        if (0 == count)
            return output;

        output.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(
            phys_device, &count, output.data()
        );
        return output;
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
        spdlog::log(
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
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
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
            spdlog::error(
                "Failed to get Vulkan function: vkCreateDebugUtilsMessengerEXT"
            );
            return VK_NULL_HANDLE;
        }

        VkDebugUtilsMessengerCreateInfoEXT cinfo;
        populate_create_info(cinfo);

        VkDebugUtilsMessengerEXT debug_messenger;
        if (VK_SUCCESS != func(instance, &cinfo, nullptr, &debug_messenger)) {
            spdlog::error("Failed to create debug utils messenger");
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


namespace {

    class VulkanExtensionsLayers {

    public:
        void add_validation() {
            extensions_.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            layers_.push_back("VK_LAYER_KHRONOS_validation");
        }

        bool are_layers_available() const {
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

        std::vector<std::string> extensions_;
        std::vector<std::string> layers_;
    };


    class FormatProperties {

    public:
        FormatProperties(VkPhysicalDevice phys_device, VkFormat format) {
            vkGetPhysicalDeviceFormatProperties(phys_device, format, &props_);
        }

        bool check_linear_tiling_feature(VkFormatFeatureFlags feature) const {
            return (props_.linearTilingFeatures & feature) == feature;
        }
        bool check_optimal_tiling_feature(VkFormatFeatureFlags feature) const {
            return (props_.optimalTilingFeatures & feature) == feature;
        }

    private:
        VkFormatProperties props_{};
    };


    class PhysDevice {

    public:
        void set(VkPhysicalDevice handle, const VkSurfaceKHR surface) {
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
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    handle_, i, surface, &present_support
                );
                if (present_support)
                    present_family_index_ = i;
            }
        }

        void clear() {
            handle_ = nullptr;
            properties_ = {};
            features_ = {};
            graphics_family_index_ = std::nullopt;
            present_family_index_ = std::nullopt;
        }

        VkPhysicalDevice get() { return handle_; }

        std::string make_report_str() const {
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

            ss << "Max image 2D dimension                    "
               << properties_.limits.maxImageDimension2D << '\n';
            ss << "    push constant                         "
               << properties_.limits.maxPushConstantsSize << '\n';
            ss << "    memory alloc count                    "
               << properties_.limits.maxMemoryAllocationCount << '\n';
            ss << "    sampler alloc count                   "
               << properties_.limits.maxSamplerAllocationCount << '\n';
            ss << "    bound descriptor sets                 "
               << properties_.limits.maxBoundDescriptorSets << '\n';
            ss << "    per stage descriptor samplers         "
               << properties_.limits.maxPerStageDescriptorSamplers << '\n';
            ss << "    per stage descriptor uniform buffers  "
               << properties_.limits.maxPerStageDescriptorUniformBuffers
               << '\n';

            ss << "ASTC LDR supported                        "
               << features_.textureCompressionASTC_LDR << '\n';
            ss << "  * ASTC 4x4 UNorm                        "
               << this->is_texture_format_supported(
                      VK_FORMAT_ASTC_4x4_UNORM_BLOCK
                  )
               << '\n';
            ss << "  * ASTC 4x4 UNorm sRGB                   "
               << this->is_texture_format_supported(
                      VK_FORMAT_ASTC_4x4_SRGB_BLOCK
                  )
               << '\n';

            ss << "ASTC HDR\n";
            ss << "  * ASTC 4x4 SFloat                       "
               << this->is_texture_format_supported(
                      VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK
                  )
               << '\n';

            ss << "BC supported                              "
               << features_.textureCompressionBC << '\n';
            ss << "  * BC1 RGB                               "
               << this->is_texture_format_supported(
                      VK_FORMAT_BC1_RGB_UNORM_BLOCK
                  )
               << '\n';
            ss << "  * BC1 RGB sRGB                          "
               << this->is_texture_format_supported(VK_FORMAT_BC1_RGB_SRGB_BLOCK
                  )
               << '\n';
            ss << "  * BC1 RGBA                              "
               << this->is_texture_format_supported(
                      VK_FORMAT_BC1_RGBA_UNORM_BLOCK
                  )
               << '\n';
            ss << "  * BC1 RGBA sRGB                         "
               << this->is_texture_format_supported(
                      VK_FORMAT_BC1_RGBA_SRGB_BLOCK
                  )
               << '\n';
            ss << "  * BC3 RGBA                              "
               << this->is_texture_format_supported(VK_FORMAT_BC3_UNORM_BLOCK)
               << '\n';
            ss << "  * BC3 RGBA sRGB                         "
               << this->is_texture_format_supported(VK_FORMAT_BC3_SRGB_BLOCK)
               << '\n';

            ss << "ETC2 supported                            "
               << features_.textureCompressionETC2 << '\n';

            ss << "==================================";
            return ss.str();
        }

        const char* name() const { return properties_.deviceName; }

        std::optional<uint32_t> graphics_family_index() const {
            return graphics_family_index_;
        }

        std::optional<uint32_t> present_family_index() const {
            return present_family_index_;
        }

        bool is_descrete_gpu() const {
            return properties_.deviceType ==
                   VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        }

        bool is_anisotropic_filtering_supported() const {
            return features_.samplerAnisotropy;
        }

        bool is_depth_clamp_supported() const { return features_.depthClamp; }

        auto max_sampler_anisotropy() const {
            return properties_.limits.maxSamplerAnisotropy;
        }

        std::vector<VkExtensionProperties> get_extensions() const {
            std::vector<VkExtensionProperties> output;

            uint32_t count;
            vkEnumerateDeviceExtensionProperties(
                handle_, nullptr, &count, nullptr
            );
            if (0 == count)
                return output;

            output.resize(count);
            vkEnumerateDeviceExtensionProperties(
                handle_, nullptr, &count, output.data()
            );
            return output;
        }

        size_t count_unsupported_extensions(
            const std::vector<std::string>& extensions
        ) const {
            const auto available_extensions = this->get_extensions();
            std::set<std::string> required_extensions(
                extensions.begin(), extensions.end()
            );

            for (const auto& extension : available_extensions) {
                auto found = required_extensions.find(extension.extensionName);
                if (found != required_extensions.end())
                    required_extensions.erase(found);
            }

            for (auto& ext_name : required_extensions)
                spdlog::warn(
                    "Required extension not available in physical device: {}",
                    ext_name
                );

            return required_extensions.size();
        }

        VkFormat select_first_supported_format(
            const std::vector<VkFormat>& candidates,
            VkImageTiling tiling,
            VkFormatFeatureFlags features
        ) const {
            for (VkFormat format : candidates) {
                ::FormatProperties props(handle_, format);

                if (tiling == VK_IMAGE_TILING_LINEAR) {
                    if (props.check_linear_tiling_feature(features))
                        return format;
                } else if (tiling == VK_IMAGE_TILING_OPTIMAL) {
                    if (props.check_optimal_tiling_feature(features))
                        return format;
                }
            }

            throw std::runtime_error("Failed to find supported format!");
        }

        bool is_texture_format_supported(VkFormat format) const {
            constexpr auto common_texture_ops =
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

            ::FormatProperties props(handle_, format);
            return props.check_optimal_tiling_feature(common_texture_ops);
        }

    private:
        VkPhysicalDevice handle_ = nullptr;
        VkPhysicalDeviceProperties properties_{};
        VkPhysicalDeviceFeatures features_{};
        std::optional<uint32_t> graphics_family_index_;
        std::optional<uint32_t> present_family_index_;
    };


    class InstanceFactory {

    public:
        InstanceFactory() {
            {
                app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                app_info.pApplicationName = "mirinapp";
                app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
                app_info.pEngineName = mirinae::ENGINE_NAME;
                app_info.engineVersion = VK_MAKE_VERSION(
                    mirinae::ENGINE_VERSION_MAJOR,
                    mirinae::ENGINE_VERSION_MINOR,
                    mirinae::ENGINE_VERSION_PATCH
                );
                app_info.apiVersion = VK_API_VERSION_1_0;
            }

            {
                cinfo_.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                cinfo_.enabledLayerCount = 0;
            }
        }

        VkInstance create() {
            cinfo_.pApplicationInfo = &app_info;

            const auto ext_name_ptrs = ::make_char_vec(ext_layers_.extensions_);
            cinfo_.enabledExtensionCount = ext_name_ptrs.size();
            cinfo_.ppEnabledExtensionNames = ext_name_ptrs.data();

            const auto layer_name_ptrs = ::make_char_vec(ext_layers_.layers_);
            cinfo_.enabledLayerCount = layer_name_ptrs.size();
            cinfo_.ppEnabledLayerNames = layer_name_ptrs.data();

            if (validation_layer_enabled_) {
                ::populate_create_info(debug_create_info);
                cinfo_.pNext =
                    reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(
                        &debug_create_info
                    );
            } else
                cinfo_.pNext = nullptr;

            VkInstance instance;
            VK_CHECK(vkCreateInstance(&cinfo_, nullptr, &instance));
            return instance;
        }

        void enable_validation_layer() { validation_layer_enabled_ = true; }

        VulkanExtensionsLayers ext_layers_;

    private:
        VkApplicationInfo app_info{};
        VkInstanceCreateInfo cinfo_{};
        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
        bool validation_layer_enabled_ = false;
    };


    class VulkanInstance {

    public:
        ~VulkanInstance() { this->destroy(); }

        void init(InstanceFactory& factory) {
            this->destroy();
            instance_ = factory.create();
            debug_messenger_ = ::create_debug_msger(instance_);
        }

        void destroy() {
            if (VK_NULL_HANDLE != debug_messenger_) {
                ::destroy_debug_msger(instance_, debug_messenger_, nullptr);
                debug_messenger_ = VK_NULL_HANDLE;
            }
            if (VK_NULL_HANDLE != instance_) {
                vkDestroyInstance(instance_, nullptr);
                instance_ = VK_NULL_HANDLE;
            }
        }

        VkInstance get() const { return instance_; }

        VkPhysicalDevice select_phys_device(const VkSurfaceKHR surface) {
            uint32_t count = 0;
            vkEnumeratePhysicalDevices(instance_, &count, nullptr);
            if (0 == count) {
                spdlog::error("There is no GPU with Vulkan support");
                return VK_NULL_HANDLE;
            }

            std::vector<VkPhysicalDevice> devices(count);
            vkEnumeratePhysicalDevices(instance_, &count, devices.data());

            VkPhysicalDevice selected = VK_NULL_HANDLE;
            double best_score = -1;

            for (auto handle : devices) {
                PhysDevice phys_device;
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

    private:
        VkInstance instance_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    };


    class SwapChainSupportDetails {

    public:
        void init(VkSurfaceKHR surface, VkPhysicalDevice phys_device) {
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                phys_device, surface, &caps_
            );

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                phys_device, surface, &formatCount, nullptr
            );
            if (formatCount != 0) {
                formats_.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(
                    phys_device, surface, &formatCount, formats_.data()
                );
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                phys_device, surface, &presentModeCount, nullptr
            );
            if (presentModeCount != 0) {
                present_modes_.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    phys_device,
                    surface,
                    &presentModeCount,
                    present_modes_.data()
                );
            }
        }

        bool is_complete() const {
            if (formats_.empty())
                return false;
            if (present_modes_.empty())
                return false;
            return true;
        }

        VkSurfaceFormatKHR choose_format() const {
            for (const auto& f : formats_) {
                if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    return f;
                }
            }
            return formats_[0];
        }

        VkPresentModeKHR choose_present_mode() const {
            for (const auto& mode : present_modes_) {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    return mode;
                }
            }
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        VkExtent2D choose_extent(uint32_t fbuf_w, uint32_t fbuf_h) const {
            constexpr auto UINT32_MAX_V = std::numeric_limits<uint32_t>::max();
            if (caps_.currentExtent.width != UINT32_MAX_V) {
                return caps_.currentExtent;
            } else {
                VkExtent2D actualExtent{ fbuf_w, fbuf_h };

                actualExtent.width = std::clamp(
                    actualExtent.width,
                    caps_.minImageExtent.width,
                    caps_.maxImageExtent.width
                );
                actualExtent.height = std::clamp(
                    actualExtent.height,
                    caps_.minImageExtent.height,
                    caps_.maxImageExtent.height
                );

                return actualExtent;
            }
        }

        uint32_t choose_image_count() const {
            auto image_count = caps_.minImageCount + 1;
            if (caps_.maxImageCount > 0 && image_count > caps_.maxImageCount) {
                image_count = caps_.maxImageCount;
            }
            return image_count;
        }

        auto get_transform() const { return caps_.currentTransform; }

    private:
        VkSurfaceCapabilitiesKHR caps_;
        std::vector<VkSurfaceFormatKHR> formats_;
        std::vector<VkPresentModeKHR> present_modes_;
    };


    class LogiDevice {

    public:
        ~LogiDevice() { this->destroy(); }

        void init(PhysDevice& phys_dev, const std::vector<std::string>& ext) {
            std::set<uint32_t> unique_queue_families{
                phys_dev.graphics_family_index().value(),
                phys_dev.present_family_index().value(),
            };

            float queue_priority = 1;
            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            for (auto queue_fam : unique_queue_families) {
                auto& queueCreateInfo = queueCreateInfos.emplace_back();
                queueCreateInfo.sType =
                    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = queue_fam;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queue_priority;
            }

            VkPhysicalDeviceFeatures deviceFeatures{};
            {
                deviceFeatures.samplerAnisotropy =
                    phys_dev.is_anisotropic_filtering_supported();
                deviceFeatures.depthClamp = phys_dev.is_depth_clamp_supported();
            }

            const auto char_extension = ::make_char_vec(ext);

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.queueCreateInfoCount = queueCreateInfos.size();
            createInfo.ppEnabledExtensionNames = char_extension.data();
            createInfo.enabledExtensionCount = char_extension.size();
            createInfo.pEnabledFeatures = &deviceFeatures;

            VK_CHECK(
                vkCreateDevice(phys_dev.get(), &createInfo, nullptr, &device_)
            );

            vkGetDeviceQueue(
                device_,
                phys_dev.graphics_family_index().value(),
                0,
                &graphics_queue_
            );
            vkGetDeviceQueue(
                device_,
                phys_dev.present_family_index().value(),
                0,
                &present_queue_
            );
        }

        void destroy() {
            if (nullptr != device_) {
                vkDeviceWaitIdle(device_);
                vkDestroyDevice(device_, nullptr);
                device_ = nullptr;
            }

            graphics_queue_ = nullptr;
        }

        void wait_idle() {
            if (nullptr != device_) {
                vkDeviceWaitIdle(device_);
            }
        }

        VkDevice get() { return device_; }
        VkQueue graphics_queue() { return graphics_queue_; }
        VkQueue present_queue() { return present_queue_; }

    private:
        VkDevice device_ = nullptr;
        VkQueue graphics_queue_ = nullptr;
        VkQueue present_queue_ = nullptr;
    };

}  // namespace


// Samplers
namespace {

    class SamplerRaii {

    public:
        SamplerRaii(::LogiDevice& logi_d) : logi_d_(logi_d) {}

        ~SamplerRaii() { this->reset(); }

        void reset(VkSampler sampler = VK_NULL_HANDLE) {
            this->destroy();
            handle_ = sampler;
        }

        void destroy() {
            if (VK_NULL_HANDLE != handle_) {
                vkDestroySampler(logi_d_.get(), handle_, nullptr);
                handle_ = VK_NULL_HANDLE;
            }
        }

        VkSampler get() const { return handle_; }

    private:
        ::LogiDevice& logi_d_;
        VkSampler handle_ = VK_NULL_HANDLE;
    };


    class SamplerBuilder {

    public:
        SamplerBuilder() : cinfo_({}) {
            cinfo_.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            cinfo_.magFilter = VK_FILTER_LINEAR;
            cinfo_.minFilter = VK_FILTER_LINEAR;
            cinfo_.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            cinfo_.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            cinfo_.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            cinfo_.anisotropyEnable = VK_FALSE;
            cinfo_.maxAnisotropy = 0;
            cinfo_.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            cinfo_.unnormalizedCoordinates = VK_FALSE;
            cinfo_.compareEnable = VK_FALSE;
            cinfo_.compareOp = VK_COMPARE_OP_ALWAYS;
            cinfo_.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            cinfo_.mipLodBias = 0;
            cinfo_.minLod = 0;
            cinfo_.maxLod = VK_LOD_CLAMP_NONE;
        }

        VkSampler build(const ::PhysDevice& pd, ::LogiDevice& ld) {
            cinfo_.anisotropyEnable = pd.is_anisotropic_filtering_supported();
            cinfo_.maxAnisotropy = pd.max_sampler_anisotropy();

            VkSampler output = VK_NULL_HANDLE;
            const auto result = vkCreateSampler(
                ld.get(), &cinfo_, nullptr, &output
            );

            if (VK_SUCCESS == result)
                return output;
            else
                return VK_NULL_HANDLE;
        }

        SamplerBuilder& mag_filter(VkFilter filter) {
            cinfo_.magFilter = filter;
            return *this;
        }
        SamplerBuilder& mag_filter_nearest() {
            return this->mag_filter(VK_FILTER_NEAREST);
        }
        SamplerBuilder& mag_filter_linear() {
            return this->mag_filter(VK_FILTER_LINEAR);
        }

        SamplerBuilder& min_filter(VkFilter filter) {
            cinfo_.minFilter = filter;
            return *this;
        }
        SamplerBuilder& min_filter_nearest() {
            return this->min_filter(VK_FILTER_NEAREST);
        }
        SamplerBuilder& min_filter_linear() {
            return this->min_filter(VK_FILTER_LINEAR);
        }

    private:
        VkSamplerCreateInfo cinfo_;
    };


    class SamplerManager : public mirinae::ISamplerManager {

    public:
        void init(const ::PhysDevice& pd, ::LogiDevice& ld) {
            {
                ::SamplerBuilder sampler_builder;
                linear_ = sampler_builder.build(pd, ld);
            }

            {
                ::SamplerBuilder sampler_builder;
                sampler_builder.mag_filter_nearest();
                sampler_builder.min_filter_nearest();
                nearest_ = sampler_builder.build(pd, ld);
            }
        }

        void destroy(::LogiDevice& ld) {
            if (VK_NULL_HANDLE != linear_) {
                vkDestroySampler(ld.get(), linear_, nullptr);
                linear_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != nearest_) {
                vkDestroySampler(ld.get(), nearest_, nullptr);
                nearest_ = VK_NULL_HANDLE;
            }
        }

        VkSampler get_linear() override { return linear_; }
        VkSampler get_nearest() override { return nearest_; }

    private:
        VkSampler linear_;
        VkSampler nearest_;
    };

}  // namespace


// Free functions
namespace mirinae {

    VkImageView create_image_view(
        VkImage image,
        uint32_t mip_levels,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkDevice device
    ) {
        VkImageViewCreateInfo cinfo{};
        cinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        cinfo.image = image;
        cinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        cinfo.format = format;
        cinfo.subresourceRange.aspectMask = aspect_flags;
        cinfo.subresourceRange.baseMipLevel = 0;
        cinfo.subresourceRange.levelCount = mip_levels;
        cinfo.subresourceRange.baseArrayLayer = 0;
        cinfo.subresourceRange.layerCount = 1;

        VkImageView imgview;
        VK_CHECK(vkCreateImageView(device, &cinfo, nullptr, &imgview));
        return imgview;
    }

}  // namespace mirinae


// VulkanDevice::Pimpl
namespace mirinae {

    class VulkanDevice::Pimpl {

    public:
        Pimpl(mirinae::EngineCreateInfo&& create_info)
            : create_info_(std::move(create_info)) {
            // Check engine creation info
            if (!create_info_.filesys_) {
                spdlog::critical("Filesystem is not set");
                throw std::runtime_error{ "Filesystem is not set" };
            }

            ::InstanceFactory instance_factory;
            if (create_info_.enable_validation_layers_) {
                instance_factory.enable_validation_layer();
                instance_factory.ext_layers_.add_validation();
            }
            instance_factory.ext_layers_.extensions_.insert(
                instance_factory.ext_layers_.extensions_.end(),
                create_info_.instance_extensions_.begin(),
                create_info_.instance_extensions_.end()
            );

            instance_.init(instance_factory);
            surface_ = ::surface_cast(
                create_info_.surface_creator_(instance_.get())
            );
            phys_device_.set(instance_.select_phys_device(surface_), surface_);
            spdlog::info(
                "Physical device selected: {}\n{}",
                phys_device_.name(),
                phys_device_.make_report_str()
            );

            std::vector<std::string> device_extensions;
            device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            if (phys_device_.count_unsupported_extensions(device_extensions))
                throw std::runtime_error{ "Some extensions are not supported" };

            logi_device_.init(phys_device_, device_extensions);
            mem_allocator_ = mirinae::create_vma_allocator(
                instance_.get(), phys_device_.get(), logi_device_.get()
            );

            ::SwapChainSupportDetails swapchain_details;
            swapchain_details.init(surface_, phys_device_.get());
            if (!swapchain_details.is_complete()) {
                throw std::runtime_error{ "The swapchain is not complete" };
            }

            samplers_.init(phys_device_, logi_device_);
        }

        ~Pimpl() {
            samplers_.destroy(logi_device_);

            mirinae::destroy_vma_allocator(mem_allocator_);
            mem_allocator_ = nullptr;

            logi_device_.destroy();

            vkDestroySurfaceKHR(instance_.get(), surface_, nullptr);
            surface_ = VK_NULL_HANDLE;
        }

        ::VulkanInstance instance_;
        ::PhysDevice phys_device_;
        ::LogiDevice logi_device_;
        ::SamplerManager samplers_;
        mirinae::VulkanMemoryAllocator mem_allocator_;
        mirinae::EngineCreateInfo create_info_;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    };

}  // namespace mirinae


// VulkanDevice
namespace mirinae {

    VulkanDevice::VulkanDevice(mirinae::EngineCreateInfo&& create_info)
        : pimpl_(std::make_unique<Pimpl>(std::move(create_info))) {}

    VulkanDevice::~VulkanDevice() {}

    VkDevice VulkanDevice::logi_device() { return pimpl_->logi_device_.get(); }

    VkQueue VulkanDevice::graphics_queue() {
        return pimpl_->logi_device_.graphics_queue();
    }

    VkQueue VulkanDevice::present_queue() {
        return pimpl_->logi_device_.present_queue();
    }

    void VulkanDevice::wait_idle() { pimpl_->logi_device_.wait_idle(); }

    std::optional<uint32_t> VulkanDevice::graphics_queue_family_index() {
        return pimpl_->phys_device_.graphics_family_index();
    }

    VkFormat VulkanDevice::select_first_supported_format(
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) const {
        return pimpl_->phys_device_.select_first_supported_format(
            candidates, tiling, features
        );
    }

    bool VulkanDevice::has_supp_depth_clamp() const {
        return pimpl_->phys_device_.is_depth_clamp_supported();
    }

    ISamplerManager& VulkanDevice::samplers() { return pimpl_->samplers_; }

    VulkanMemoryAllocator VulkanDevice::mem_alloc() {
        return pimpl_->mem_allocator_;
    }

    IFilesys& VulkanDevice::filesys() { return *pimpl_->create_info_.filesys_; }

    IOsIoFunctions& VulkanDevice::osio() { return *pimpl_->create_info_.osio_; }

}  // namespace mirinae


// Swapchain
namespace mirinae {

    void Swapchain::init(
        uint32_t fbuf_w, uint32_t fbuf_h, VulkanDevice& vulkan_device
    ) {
        auto logi_device = vulkan_device.logi_device();

        ::SwapChainSupportDetails s_details;
        s_details.init(
            vulkan_device.pimpl_->surface_,
            vulkan_device.pimpl_->phys_device_.get()
        );

        std::array<uint32_t, 2> queue_family_indices{
            vulkan_device.pimpl_->phys_device_.graphics_family_index().value(),
            vulkan_device.pimpl_->phys_device_.present_family_index().value()
        };
        VkSwapchainCreateInfoKHR cinfo{};

        // Fill in swapchain create info
        {
            cinfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            cinfo.surface = vulkan_device.pimpl_->surface_;
            cinfo.minImageCount = s_details.choose_image_count();
            cinfo.imageFormat = s_details.choose_format().format;
            cinfo.imageColorSpace = s_details.choose_format().colorSpace;
            cinfo.imageExtent = s_details.choose_extent(fbuf_w, fbuf_h);
            cinfo.imageArrayLayers = 1;
            cinfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            cinfo.preTransform = s_details.get_transform();
            cinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            cinfo.presentMode = s_details.choose_present_mode();
            cinfo.clipped = VK_TRUE;
            cinfo.oldSwapchain = VK_NULL_HANDLE;

            if (queue_family_indices[0] != queue_family_indices[1]) {
                cinfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                cinfo.queueFamilyIndexCount = queue_family_indices.size();
                cinfo.pQueueFamilyIndices = queue_family_indices.data();
            } else {
                cinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                cinfo.queueFamilyIndexCount = 0;      // Optional
                cinfo.pQueueFamilyIndices = nullptr;  // Optional
            }
        }

        VK_CHECK(vkCreateSwapchainKHR(logi_device, &cinfo, NULL, &swapchain_));

        // Store some data
        {
            format_ = cinfo.imageFormat;
            extent_ = cinfo.imageExtent;

            uint32_t image_count = s_details.choose_image_count();
            vkGetSwapchainImagesKHR(
                logi_device, swapchain_, &image_count, nullptr
            );
            images_.resize(image_count);
            vkGetSwapchainImagesKHR(
                logi_device, swapchain_, &image_count, images_.data()
            );
        }

        spdlog::info(
            "Swapchain created: format={}, extent=({}, {}), present_mode={}, "
            "image_count={}",
            static_cast<int>(format_),
            extent_.width,
            extent_.height,
            static_cast<int>(cinfo.presentMode),
            images_.size()
        );

        // Create views
        for (size_t i = 0; i < images_.size(); i++) {
            views_.push_back(mirinae::create_image_view(
                images_.at(i),
                1,
                format_,
                VK_IMAGE_ASPECT_COLOR_BIT,
                logi_device
            ));
        }
    }

    void Swapchain::destroy(VkDevice logi_device) {
        for (auto view : views_) vkDestroyImageView(logi_device, view, nullptr);
        views_.clear();

        if (VK_NULL_HANDLE != swapchain_) {
            vkDestroySwapchainKHR(logi_device, swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }
    }

    std::optional<ShainImageIndex> Swapchain::acquire_next_image(
        VkSemaphore img_avaiable_semaphore, VkDevice logi_device
    ) {
        uint32_t imageIndex;
        const auto result = vkAcquireNextImageKHR(
            logi_device,
            swapchain_,
            UINT64_MAX,
            img_avaiable_semaphore,
            VK_NULL_HANDLE,
            &imageIndex
        );

        switch (result) {
            case VK_SUCCESS:
            case VK_SUBOPTIMAL_KHR:
                return ShainImageIndex{ imageIndex };
            case VK_ERROR_OUT_OF_DATE_KHR:
            default:
                return std::nullopt;
        }
    }

}  // namespace mirinae
