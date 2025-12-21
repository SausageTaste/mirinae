#include "render/vkdevice/phys_device.hpp"

#include <set>

#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/vulkan/base/render/enum_str.hpp"


namespace {

    std::vector<VkQueueFamilyProperties> get_queue_family_props(
        VkPhysicalDevice phys_device
    ) {
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

    const char* get_format_feature_name(VkFormatFeatureFlags f) {
        switch (f) {
            case VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT:
                return "Sampled img";
            case VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT:
                return "Strg img";
            case VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT:
                return "Strg img atomic";
            case VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT:
                return "Uniform texel buf";
            case VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT:
                return "Strg texel buf";
            case VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT:
                return "Strg texel buf atomic";
            case VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT:
                return "Vtx buf";
            case VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT:
                return "Color attach";
            case VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT:
                return "Color attach blend";
            case VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT:
                return "Depth attach";
            case VK_FORMAT_FEATURE_BLIT_SRC_BIT:
                return "Blit src";
            case VK_FORMAT_FEATURE_BLIT_DST_BIT:
                return "Blit dst";
            case VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT:
                return "Sample linear";
            case VK_FORMAT_FEATURE_TRANSFER_SRC_BIT:
                return "Transfer src";
            case VK_FORMAT_FEATURE_TRANSFER_DST_BIT:
                return "Transfer dst";
            default:
                return "Unknown";
        }
    }

}  // namespace


// PhysDevice
namespace mirinae {

    void PhysDevice::set(VkPhysicalDevice handle, const VkSurfaceKHR surface) {
        if (nullptr == handle) {
            SPDLOG_ERROR("PhysDevice::set has recieved a nullptr");
            this->clear();
            return;
        }

        handle_ = handle;
        features_.init(handle);
        vkGetPhysicalDeviceProperties(handle_, &properties_);

        const auto queue_family = ::get_queue_family_props(handle_);
        for (int i = 0; i < queue_family.size(); ++i) {
            if (this->is_queue_flag_applicable(queue_family[i].queueFlags))
                graphics_family_index_ = i;

            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(
                handle_, i, surface, &present_support
            );
            if (present_support)
                present_family_index_ = i;
        }
    }

    void PhysDevice::clear() {
        handle_ = nullptr;
        properties_ = {};
        features_.clear();
        graphics_family_index_ = std::nullopt;
        present_family_index_ = std::nullopt;
    }

    std::string PhysDevice::make_report_str() const {
        dal::ValuesReport report;
        report.set_title(properties_.deviceName);
        report.add(
            0,
            "API version",
            fmt::format(
                "{}.{}.{}",
                VK_VERSION_MAJOR(properties_.apiVersion),
                VK_VERSION_MINOR(properties_.apiVersion),
                VK_VERSION_PATCH(properties_.apiVersion)
            )
        );

        switch (properties_.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                report.add(0, "Device type", "Integrated");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                report.add(0, "Device type", "Discrete");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                report.add(0, "Device type", "Virtual");
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                report.add(0, "Device type", "CPU");
                break;
            default:
                report.new_entry(0, "Device type")
                    .set_value("Unknown")
                    .add_value(properties_.deviceType);
                break;
        }

        features_.fill_report(report);

        const auto queue_family = ::get_queue_family_props(handle_);
        report.add(0, "Queue family count", queue_family.size());
        for (int i = 0; i < queue_family.size(); ++i) {
            auto& q = queue_family[i];

            report.new_entry(2, "Queue family")
                .set_value(i)
                .add_value(q.queueCount);
            if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                report.add_value("Graphics");
            if (q.queueFlags & VK_QUEUE_COMPUTE_BIT)
                report.add_value("Compute");
            if (q.queueFlags & VK_QUEUE_TRANSFER_BIT)
                report.add_value("Transfer");
        }

        auto& limits = properties_.limits;

        report.add(0, "Compute shader")
            .new_entry(2, "Max work group size")
            .set_value(limits.maxComputeWorkGroupSize[0])
            .add_value(limits.maxComputeWorkGroupSize[1])
            .add_value(limits.maxComputeWorkGroupSize[2])
            .new_entry(2, "Max work group count")
            .set_value(limits.maxComputeWorkGroupCount[0])
            .add_value(limits.maxComputeWorkGroupCount[1])
            .add_value(limits.maxComputeWorkGroupCount[2])
            .new_entry(2, "Max work group invocations")
            .set_value(limits.maxComputeWorkGroupInvocations);

        report.add(0, "Limits")
            .new_entry(2, "Image 2D dimension")
            .set_value(limits.maxImageDimension2D)
            .new_entry(2, "Push constant")
            .set_value(limits.maxPushConstantsSize)
            .new_entry(2, "Memory alloc count")
            .set_value(limits.maxMemoryAllocationCount)
            .new_entry(2, "Sampler alloc count")
            .set_value(limits.maxSamplerAllocationCount)
            .new_entry(2, "Bound descriptor sets")
            .set_value(limits.maxBoundDescriptorSets)
            .new_entry(2, "Per stage descriptor samplers")
            .set_value(limits.maxPerStageDescriptorSamplers)
            .new_entry(2, "Per stage descriptor uniform buffers")
            .set_value(limits.maxPerStageDescriptorUniformBuffers)
            .new_entry(2, "Ubuf alignment")
            .set_value(limits.minUniformBufferOffsetAlignment);

        static const std::vector<VkFormat> formats{
            VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK,
            VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
            VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
            VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
            VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
            VK_FORMAT_BC3_SRGB_BLOCK,
            VK_FORMAT_BC3_UNORM_BLOCK,
            VK_FORMAT_BC7_SRGB_BLOCK,
            VK_FORMAT_BC7_UNORM_BLOCK,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_R16_SFLOAT,
            VK_FORMAT_R16G16_SFLOAT,
            VK_FORMAT_R16G16B16_SFLOAT,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_R32_SFLOAT,
            VK_FORMAT_R32G32_SFLOAT,
            VK_FORMAT_R32G32B32_SFLOAT,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_R8G8B8A8_UNORM,
        };

        static const std::vector<VkFormatFeatureFlags> feature_flags{
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
            VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
            VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT,
            // VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT,
            // VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT,
            // VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT,
            VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_FORMAT_FEATURE_BLIT_SRC_BIT,
            VK_FORMAT_FEATURE_BLIT_DST_BIT,
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
            VK_FORMAT_FEATURE_TRANSFER_SRC_BIT,
            VK_FORMAT_FEATURE_TRANSFER_DST_BIT,
        };

        report.add(0, "Formats");
        for (auto format : formats) {
            const FormatProperties fmt_prop(handle_, format);

            report.new_entry(2, to_str(format)).add(4, "[Linear tiling]");
            for (auto& flag : feature_flags) {
                if (fmt_prop.check_linear_tiling_feature(flag))
                    report.add_value(::get_format_feature_name(flag));
            }
            report.add(4, "[Optimal tiling]");
            for (auto& flag : feature_flags) {
                if (fmt_prop.check_optimal_tiling_feature(flag))
                    report.add_value(::get_format_feature_name(flag));
            }
        }

        return report.build_str();
    }

    std::optional<uint32_t> PhysDevice::graphics_family_index() const {
        return graphics_family_index_;
    }

    std::optional<uint32_t> PhysDevice::present_family_index() const {
        return present_family_index_;
    }

    bool PhysDevice::is_descrete_gpu() const {
        return properties_.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    }

    bool PhysDevice::is_texture_format_supported(VkFormat format) const {
        constexpr auto common_texture_ops =
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
            VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
            VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

        FormatProperties props(handle_, format);
        return props.check_optimal_tiling_feature(common_texture_ops);
    }

    float PhysDevice::max_sampler_anisotropy() const {
        return properties_.limits.maxSamplerAnisotropy;
    }

    std::vector<VkExtensionProperties> PhysDevice::get_extensions() const {
        std::vector<VkExtensionProperties> output;

        uint32_t count;
        vkEnumerateDeviceExtensionProperties(handle_, nullptr, &count, nullptr);
        if (0 == count)
            return output;

        output.resize(count);
        vkEnumerateDeviceExtensionProperties(
            handle_, nullptr, &count, output.data()
        );
        return output;
    }

    size_t PhysDevice::count_unsupported_extensions(
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
            SPDLOG_WARN(
                "Required extension not available in physical device: {}",
                ext_name
            );

        return required_extensions.size();
    }

    VkFormat PhysDevice::select_first_supported_format(
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) const {
        for (VkFormat format : candidates) {
            FormatProperties props(handle_, format);

            if (tiling == VK_IMAGE_TILING_LINEAR) {
                if (props.check_linear_tiling_feature(features))
                    return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL) {
                if (props.check_optimal_tiling_feature(features))
                    return format;
            }
        }

        MIRINAE_ABORT("Failed to find supported format!");
    }

    bool PhysDevice::is_queue_flag_applicable(const VkQueueFlags flags) {
        if (0 == (flags & VK_QUEUE_GRAPHICS_BIT))
            return false;
        else if (0 == (flags & VK_QUEUE_COMPUTE_BIT))
            return false;

        return true;
    }

}  // namespace mirinae
