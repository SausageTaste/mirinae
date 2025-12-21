#include "render/vkdevice/logi_device.hpp"

#include <set>

#include "mirinae/vulkan/base/render/vkcheck.hpp"


namespace {

    std::vector<const char*> make_char_vec(
        const std::vector<std::string>& strings
    ) {
        std::vector<const char*> output;
        for (auto& x : strings) output.push_back(x.c_str());
        return output;
    }

}  // namespace


namespace {

    void clear_features(VkPhysicalDeviceVulkan11Features& dst) {
        dst = {};
        dst.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    }

    void select_features(VkPhysicalDeviceVulkan11Features& dst) {
        dst.shaderDrawParameters = VK_TRUE;
    }

    void select_features(
        VkPhysicalDeviceFeatures& dst, const VkPhysicalDeviceFeatures& src
    ) {
        // Required
        dst.tessellationShader = src.tessellationShader;
        // Optional
        dst.depthClamp = src.depthClamp;
        dst.fillModeNonSolid = src.fillModeNonSolid;
        dst.samplerAnisotropy = src.samplerAnisotropy;
        // KTX
        dst.textureCompressionASTC_LDR = src.textureCompressionASTC_LDR;
        dst.textureCompressionBC = src.textureCompressionBC;
        dst.textureCompressionETC2 = src.textureCompressionETC2;
    }

}  // namespace


// LogiDevice
namespace mirinae {

    LogiDevice::~LogiDevice() { this->destroy(); }

    void LogiDevice::init(
        PhysDevice& phys_dev, const std::vector<std::string>& ext
    ) {
        std::set<uint32_t> unique_queue_families{
            phys_dev.graphics_family_index().value(),
            phys_dev.present_family_index().value(),
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

        // Features for Vulkan 1.1
        ::clear_features(feat11_);
        ::select_features(feat11_);

        features_ = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        ::select_features(features_.features, phys_dev.features());
        features_.pNext = &feat11_;

        const auto char_extension = make_char_vec(ext);

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = queueCreateInfos.size();
        createInfo.ppEnabledExtensionNames = char_extension.data();
        createInfo.enabledExtensionCount = char_extension.size();
        createInfo.pNext = &features_;

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
            device_, phys_dev.present_family_index().value(), 0, &present_queue_
        );
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

}  // namespace mirinae
