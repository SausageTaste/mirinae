#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <daltools/filesys/filesys.hpp>

#include "mirinae/platform/osio.hpp"


namespace mirinae {

    struct EngineCreateInfo {
        // VkInstance -> VkSurfaceKHR
        using surface_creator_t = std::function<uint64_t(void*)>;
        using imgui_new_frame_t = std::function<void(void)>;

        std::shared_ptr<dal::Filesystem> filesys_;
        std::shared_ptr<IOsIoFunctions> osio_;
        std::vector<std::string> instance_extensions_;
        surface_creator_t surface_creator_;
        imgui_new_frame_t imgui_new_frame_;
        double ui_scale_ = 1.0;
        int init_width_ = 0;
        int init_height_ = 0;
        bool enable_validation_layers_ = false;
    };

}  // namespace mirinae
