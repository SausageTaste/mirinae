#pragma once

#include <vulkan/vulkan.h>

#include "mirinae/lightweight/include_spdlog.hpp"


#define VK_CHECK(x)                        \
    do {                                   \
        const VkResult res = x;            \
        MIRINAE_ASSERT(VK_SUCCESS == res); \
    } while (0)
