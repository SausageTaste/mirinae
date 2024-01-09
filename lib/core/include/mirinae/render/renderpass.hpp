#pragma once

#include "mirinae/render/vkdevice.hpp"


namespace mirinae {

    class IRenderPassBundle {

    public:
        void destroy();

    };


    std::unique_ptr<IRenderPassBundle> create_unorthodox(VkFormat swapchain_format, VkFormat depthmap_format, VulkanDevice& device);

}
