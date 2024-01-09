#pragma once

#include "mirinae/render/vkdevice.hpp"


namespace mirinae {

    class RenderPassBundle {

    private:
        VkRenderPass renderpass_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout> desclayouts_;

    };


    std::optional<RenderPassBundle> create_unorthodox(VulkanDevice& device);

}
