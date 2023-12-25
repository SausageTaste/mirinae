#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mirinae/platform/filesys.hpp"


namespace mirinae {

    struct EngineCreateInfo {
        using surface_creator_t = std::function<uint64_t(void*)>; // VkInstance -> VkSurfaceKHR

        std::unique_ptr<IFilesys> filesys_;

        std::vector<std::string> instance_extensions_;
        surface_creator_t surface_creator_;
        bool enable_validation_layers_ = false;
    };

}
