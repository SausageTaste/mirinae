#pragma once

#include <memory>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <daltools/img/img2d.hpp>


#define VK_CHECK(x)                \
    do {                           \
        const VkResult res = x;    \
        assert(VK_SUCCESS == res); \
    } while (0)

#define MIRINAE_DEFINE_OPAQUE_HANDLE(name) \
    class name##_T;                        \
    using name = name##_T*;


namespace mirinae {

    MIRINAE_DEFINE_OPAQUE_HANDLE(VulkanMemoryAllocator);

    VulkanMemoryAllocator create_vma_allocator(
        VkInstance instance, VkPhysicalDevice phys_device, VkDevice logi_device
    );

    void destroy_vma_allocator(VulkanMemoryAllocator allocator);


    struct BufferCinfoBundle {
        VkBufferCreateInfo buf_info_;
        VmaAllocationCreateInfo alloc_info_;
    };


    class Buffer {

    public:
        Buffer() = default;
        ~Buffer();

        Buffer(Buffer&& rhs) noexcept;
        Buffer& operator=(Buffer&& rhs) noexcept;

        void init(const BufferCinfoBundle& cinfo, VulkanMemoryAllocator);
        void destroy();

        VkBuffer get() const { return buffer_; }
        VkBuffer buffer() const { return buffer_; }
        VkDeviceSize size() const;

        void set_data(const void* data, size_t size);

        template <typename T>
        void set_data(const T& data) {
            this->set_data(&data, sizeof(T));
        }

        void record_copy_cmd(
            const Buffer& src, VkCommandBuffer cmdbuf, VkDevice logi_device
        );

    private:
        VulkanMemoryAllocator allocator_ = nullptr;
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VkDeviceSize size_ = 0;
    };


    class Image {

    public:
        Image() = default;
        ~Image();

        void init(
            const VkImageCreateInfo& create_info,
            VulkanMemoryAllocator allocator
        );
        void destroy(VulkanMemoryAllocator allocator);

        VkImage image() const { return image_; }
        VkFormat format() const { return img_info_.format; }
        uint32_t width() const { return img_info_.extent.width; }
        uint32_t height() const { return img_info_.extent.height; }
        uint32_t mip_levels() const { return img_info_.mipLevels; }
        VkExtent2D extent2d() const {
            VkExtent2D out;
            out.width = img_info_.extent.width;
            out.height = img_info_.extent.height;
            return out;
        }

    private:
        VkImageCreateInfo img_info_ = {};
        VkImage image_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
    };

}  // namespace mirinae
