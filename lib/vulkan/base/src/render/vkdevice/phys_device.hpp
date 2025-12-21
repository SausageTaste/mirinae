#pragma once

#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>
#include <dal/auxiliary/util.hpp>


namespace mirinae {

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


    class PhysDeviceFeatures {

    public:
        void init(VkPhysicalDevice phys_device) {
            vkGetPhysicalDeviceFeatures(phys_device, &features_);
        }

        void clear() { features_ = {}; }

        const VkPhysicalDeviceFeatures& get() const { return features_; }

        void fill_report(dal::ValuesReport& report) const {
            report.add(0, "Features");
            if (features_.samplerAnisotropy)
                report.add_value("Anisotropic");
            if (features_.geometryShader)
                report.add_value("Geometry");
            if (features_.tessellationShader)
                report.add_value("Tessellation");
            if (features_.textureCompressionETC2)
                report.add_value("ETC2");
            if (features_.textureCompressionASTC_LDR)
                report.add_value("ASTC");
        }

    private:
        VkPhysicalDeviceFeatures features_{};
    };


    class PhysDevice {

    public:
        void set(VkPhysicalDevice handle, const VkSurfaceKHR surface);
        void clear();

        auto get() { return handle_; }
        auto name() const { return properties_.deviceName; }
        auto& features() const { return features_.get(); }
        auto& limits() const { return properties_.limits; }

        std::string make_report_str() const;

        std::optional<uint32_t> graphics_family_index() const;
        std::optional<uint32_t> present_family_index() const;

        bool is_descrete_gpu() const;
        bool is_texture_format_supported(VkFormat format) const;
        float max_sampler_anisotropy() const;

        std::vector<VkExtensionProperties> get_extensions() const;

        size_t count_unsupported_extensions(
            const std::vector<std::string>& extensions
        ) const;

        VkFormat select_first_supported_format(
            const std::vector<VkFormat>& candidates,
            VkImageTiling tiling,
            VkFormatFeatureFlags features
        ) const;

    private:
        static bool is_queue_flag_applicable(const VkQueueFlags flags);
        VkPhysicalDevice handle_ = nullptr;
        VkPhysicalDeviceProperties properties_{};
        PhysDeviceFeatures features_;
        std::optional<uint32_t> graphics_family_index_;
        std::optional<uint32_t> present_family_index_;
    };

}  // namespace mirinae
