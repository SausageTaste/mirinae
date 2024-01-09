#pragma once

#include "mirinae/render/vkdevice.hpp"


namespace mirinae {

    class IRenderPassBundle {

    public:
        virtual ~IRenderPassBundle() = default;
        virtual void destroy() = 0;

    };


    std::unique_ptr<IRenderPassBundle> create_unorthodox(VkFormat swapchain_format, VkFormat depthmap_format, VulkanDevice& device);

}
