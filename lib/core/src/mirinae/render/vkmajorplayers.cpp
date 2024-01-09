#include "mirinae/render/vkmajorplayers.hpp"

#include <array>

#include <spdlog/spdlog.h>


// Semaphore
namespace mirinae {

    void Semaphore::init(VkDevice logi_device) {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (vkCreateSemaphore(logi_device, &semaphoreInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }

    void Semaphore::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroySemaphore(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}


// Fence
namespace mirinae {

    void Fence::init(bool init_signaled, VkDevice logi_device) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        if (init_signaled)
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateFence(logi_device, &fenceInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }

    void Fence::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyFence(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    void Fence::wait(VkDevice logi_device) {
        if (VK_NULL_HANDLE == handle_) {
            spdlog::warn("Tried to wait on a fence that is not created");
            return;
        }

        vkWaitForFences(logi_device, 1, &handle_, VK_TRUE, UINT64_MAX);
    }

    void Fence::reset(VkDevice logi_device) {
        if (VK_NULL_HANDLE == handle_) {
            spdlog::warn("Tried to reset a fence that is not created");
            return;
        }

        vkResetFences(logi_device, 1, &handle_);
    }

}


// CommandPool
namespace mirinae {

    void CommandPool::init(uint32_t graphics_queue, VkDevice logi_device) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphics_queue;

        if (vkCreateCommandPool(logi_device, &poolInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void CommandPool::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyCommandPool(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    VkCommandBuffer CommandPool::alloc(VkDevice logi_device) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = handle_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(logi_device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        return commandBuffer;
    }

    void CommandPool::free(VkCommandBuffer cmdbuf, VkDevice logi_device) {
        vkFreeCommandBuffers(logi_device, handle_, 1, &cmdbuf);
    }

    VkCommandBuffer CommandPool::begin_single_time(VkDevice logi_device) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = handle_;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(logi_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void CommandPool::end_single_time(VkCommandBuffer cmdbuf, VkQueue graphics_q, VkDevice logi_device) {
        vkEndCommandBuffer(cmdbuf);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;

        vkQueueSubmit(graphics_q, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_q);

        vkFreeCommandBuffers(logi_device, handle_, 1, &cmdbuf);
    }

}


// Sampler
namespace mirinae {

    void Sampler::init(bool enable_anisotropy, float max_anisotropy, VkDevice logi_device) {
        this->destroy(logi_device);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = enable_anisotropy ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = max_anisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0;
        samplerInfo.minLod = 0;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

        if (vkCreateSampler(logi_device, &samplerInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void Sampler::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroySampler(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}
