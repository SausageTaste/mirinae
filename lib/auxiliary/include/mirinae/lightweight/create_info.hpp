#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <dal/parser/filesys/filesys.hpp>

#include "mirinae/platform/osio.hpp"


namespace mirinae {

    class VulkanPlatformFunctions;


    struct EngineCreateInfo {
        std::shared_ptr<dal::Filesystem> filesys_;
        std::vector<std::string> instance_extensions_;
        IOsIoFunctions* osio_ = nullptr;
        VulkanPlatformFunctions* vulkan_os_ = nullptr;
        double ui_scale_ = 1.0;
        int init_width_ = 0;
        int init_height_ = 0;
        bool enable_validation_layers_ = false;
    };

}  // namespace mirinae
