#pragma once

#include <memory>

#include <vk_mem_alloc.h>


#define MIRINAE_DEFINE_OPAQUE_HANDLE(name) class name##_T; using name = name##_T*;


class VmaAllocation_T;


namespace mirinae {

    MIRINAE_DEFINE_OPAQUE_HANDLE(VulkanMemoryAllocator);

    VulkanMemoryAllocator create_vma_allocator(VkInstance instance, VkPhysicalDevice phys_device, VkDevice logi_device);
    void destroy_vma_allocator(VulkanMemoryAllocator allocator);


    class Buffer {

    public:
        void init_staging(VkDeviceSize size, VulkanMemoryAllocator allocator);
        void init_ubuf(VkDeviceSize size, VulkanMemoryAllocator allocator);
        void init_vertices(VkDeviceSize size, VulkanMemoryAllocator allocator);
        void init_indices(VkDeviceSize size, VulkanMemoryAllocator allocator);

        void destroy(VulkanMemoryAllocator allocator);

        VkBuffer get() const { return buffer_; }
        VkBuffer buffer() const { return buffer_; }
        VkDeviceSize size() const;

        void set_data(const void* data, size_t size, VulkanMemoryAllocator allocator);
        void record_copy_cmd(const Buffer& src, VkCommandBuffer cmdbuf, VkDevice logi_device);

    private:
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;

    };

}
