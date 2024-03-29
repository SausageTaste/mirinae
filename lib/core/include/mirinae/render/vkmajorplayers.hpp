#pragma once

#include <array>
#include <vector>
#include <string>
#include <optional>

#include <vulkan/vulkan.h>

#include "mirinae/util/image.hpp"
#include "mirinae/util/lightweights.hpp"

#include "meshdata.hpp"
#include "vkdevice.hpp"


namespace mirinae {

    class Semaphore {

    public:
        void init(VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkSemaphore get() { return handle_; }

    private:
        VkSemaphore handle_ = VK_NULL_HANDLE;

    };


    class Fence {

    public:
        void init(bool init_signaled, VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkFence get() { return handle_; }

        void wait(VkDevice logi_device);
        void reset(VkDevice logi_device);

    private:
        VkFence handle_ = VK_NULL_HANDLE;

    };


    class CommandPool {

    public:
        void init(uint32_t graphics_queue, VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkCommandBuffer alloc(VkDevice logi_device);
        void free(VkCommandBuffer cmdbuf, VkDevice logi_device);

        VkCommandBuffer begin_single_time(VkDevice logi_device);
        void end_single_time(VkCommandBuffer cmdbuf, VkQueue graphics_q, VkDevice logi_device);

    private:
        VkCommandPool handle_ = VK_NULL_HANDLE;

    };

}
