#pragma once

#include <memory>

#include <vk_mem_alloc.h>

#include "mirinae/util/image.hpp"


#define MIRINAE_DEFINE_OPAQUE_HANDLE(name) class name##_T; using name = name##_T*;


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
        VkDeviceSize data_size_ = 0;

    };


    class ImageCreateInfo {

    public:
        ImageCreateInfo();
        const VkImageCreateInfo& get() const { return info_; }

        ImageCreateInfo& set_dimensions(uint32_t width, uint32_t height);
        ImageCreateInfo& set_format(VkFormat format);
        ImageCreateInfo& set_usage(VkImageUsageFlags usage);

        ImageCreateInfo& deduce_mip_levels();
        ImageCreateInfo& fetch_from_image(const IImage2D& img, bool srgb);

    private:
        VkImageCreateInfo info_ = {};

    };


    class Image {

    public:
        void init(const VkImageCreateInfo& create_info, VulkanMemoryAllocator allocator);
        void destroy(VulkanMemoryAllocator allocator);

        VkImage image() const { return image_; }
        VkFormat format() const { return img_info_.format; }
        uint32_t width() const { return img_info_.extent.width; }
        uint32_t height() const { return img_info_.extent.height; }
        uint32_t mip_levels() const { return img_info_.mipLevels; }

    private:
        VkImageCreateInfo img_info_ = {};
        VkImage image_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;

    };

}
