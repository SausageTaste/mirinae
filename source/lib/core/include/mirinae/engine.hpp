#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "mirinae/util/uinput.hpp"


namespace mirinae {

    class IEngine {

    public:
        virtual ~IEngine() = default;

        virtual void do_frame() = 0;
        virtual bool is_ongoing() = 0;

        virtual void notify_window_resize(uint32_t width, uint32_t height) = 0;
        virtual void notify_key_event(const key::Event& e) = 0;

    };


    struct EngineCreateInfo {
        using surface_creator_t = std::function<VkSurfaceKHR(VkInstance)>;

        std::vector<std::string> instance_extensions_;
        surface_creator_t surface_creator_;
    };

    std::unique_ptr<IEngine> create_engine(const EngineCreateInfo& create_info);

}
