#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mirinae/platform/filesys.hpp"
#include "mirinae/platform/osio.hpp"


namespace mirinae {

    struct EngineCreateInfo {
        // VkInstance -> VkSurfaceKHR
        using surface_creator_t = std::function<uint64_t(void*)>;

        std::shared_ptr<IFilesys> filesys_;
        std::shared_ptr<IOsIoFunctions> osio_;
        std::vector<std::string> instance_extensions_;
        surface_creator_t surface_creator_;
        int init_width_ = 0;
        int init_height_ = 0;
        bool enable_validation_layers_ = false;
    };

}  // namespace mirinae
