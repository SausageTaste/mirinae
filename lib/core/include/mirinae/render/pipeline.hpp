#pragma once

#include "mirinae/platform/filesys.hpp"

#include "uniform.hpp"


namespace mirinae {

    Pipeline create_unorthodox_pipeline(
        RenderPass& renderpass,
        DescLayoutBundle& desclayout_bundle,
        VulkanDevice& device
    );

}
