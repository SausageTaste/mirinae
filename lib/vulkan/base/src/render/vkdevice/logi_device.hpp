#pragma once

#include "render/vkdevice/phys_device.hpp"


namespace mirinae {

    class LogiDevice {

    public:
        LogiDevice() = default;
        ~LogiDevice();

        void init(PhysDevice& phys_dev, const std::vector<std::string>& ext);
        void destroy();
        void wait_idle();

        VkDevice get() { return device_; }
        VkQueue graphics_queue() { return graphics_queue_; }
        VkQueue present_queue() { return present_queue_; }
        const VkPhysicalDeviceFeatures2& features() const { return features_; }

    private:
        VkDevice device_ = nullptr;
        VkQueue graphics_queue_ = nullptr;
        VkQueue present_queue_ = nullptr;
        VkPhysicalDeviceFeatures2 features_;
        VkPhysicalDeviceVulkan11Features feat11_;
    };

}  // namespace mirinae
