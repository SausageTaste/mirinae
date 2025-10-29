#include "mirinae/vulkan_pch.h"

#include "mirinae/render/mem_alloc.hpp"

#include <array>
#include <cmath>

#include "mirinae/lightweight/include_spdlog.hpp"

#define VMA_LEAK_LOG_FORMAT(format, ...)             \
    do {                                             \
        std::array<char, 512> buffer{};              \
        sprintf(buffer.data(), format, __VA_ARGS__); \
        SPDLOG_WARN("{}", buffer.data());            \
    } while (0)

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>


// VulkanMemoryAllocator_T
namespace mirinae {

    class VulkanMemoryAllocator_T {

    public:
        VulkanMemoryAllocator_T(
            VkInstance instance,
            VkPhysicalDevice phys_device,
            VkDevice logi_device
        ) {
            VmaVulkanFunctions vma_vulkan_funcs = {};
            vma_vulkan_funcs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
            vma_vulkan_funcs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

            VmaAllocatorCreateInfo cinfo = {};
            cinfo.physicalDevice = phys_device;
            cinfo.device = logi_device;
            cinfo.instance = instance;
            cinfo.pVulkanFunctions = &vma_vulkan_funcs;

            VK_CHECK(vmaCreateAllocator(&cinfo, &allocator_));
        }

        ~VulkanMemoryAllocator_T() { this->destroy(); }

        void destroy() {
            if (allocator_ != VK_NULL_HANDLE) {
                vmaDestroyAllocator(allocator_);
                allocator_ = VK_NULL_HANDLE;
            }
        }

        VmaAllocator get() { return allocator_; }

    private:
        VmaAllocator allocator_ = VK_NULL_HANDLE;
    };

}  // namespace mirinae


// VulkanMemoryAllocator
namespace mirinae {

    VulkanMemoryAllocator create_vma_allocator(
        VkInstance instance, VkPhysicalDevice phys_device, VkDevice logi_device
    ) {
        return new VulkanMemoryAllocator_T(instance, phys_device, logi_device);
    }

    void destroy_vma_allocator(VulkanMemoryAllocator allocator) {
        delete allocator;
    }

}  // namespace mirinae


// Buffer
namespace mirinae {

    Buffer::~Buffer() { this->destroy(); }

    Buffer::Buffer(Buffer&& rhs) noexcept {
        std::swap(allocator_, rhs.allocator_);
        std::swap(buffer_, rhs.buffer_);
        std::swap(allocation_, rhs.allocation_);
        std::swap(size_, rhs.size_);
    }

    Buffer& Buffer::operator=(Buffer&& rhs) noexcept {
        std::swap(allocator_, rhs.allocator_);
        std::swap(buffer_, rhs.buffer_);
        std::swap(allocation_, rhs.allocation_);
        std::swap(size_, rhs.size_);
        return *this;
    }

    void Buffer::init(
        const BufferCinfoBundle& cinfo, VulkanMemoryAllocator allocator
    ) {
        this->destroy();
        allocator_ = allocator;

        const auto res = vmaCreateBuffer(
            allocator->get(),
            &cinfo.buf_info_,
            &cinfo.alloc_info_,
            &buffer_,
            &allocation_,
            nullptr
        );

        if (VK_SUCCESS != res)
            MIRINAE_ABORT("failed to init a VMA buffer");
        size_ = cinfo.buf_info_.size;
    }

    void Buffer::destroy() {
        if (!allocator_)
            return;

        if (buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator_->get(), buffer_, allocation_);
            buffer_ = VK_NULL_HANDLE;
        }

        allocator_ = nullptr;
    }

    VkDeviceSize Buffer::size() const { return size_; }

    void Buffer::set_data(const void* data, size_t size) {
        MIRINAE_ASSERT(nullptr != allocator_);

        void* ptr;
        vmaMapMemory(allocator_->get(), allocation_, &ptr);
        memcpy(ptr, data, std::min<size_t>(size, this->size()));
        vmaUnmapMemory(allocator_->get(), allocation_);
    }

    void Buffer::record_copy_cmd(
        const Buffer& src, VkCommandBuffer cmdbuf, VkDevice logi_device
    ) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdbuf, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = std::min<VkDeviceSize>(this->size(), src.size());
        vkCmdCopyBuffer(cmdbuf, src.buffer_, buffer_, 1, &copyRegion);

        vkEndCommandBuffer(cmdbuf);
    }

}  // namespace mirinae


// BufferSpan
namespace mirinae {

    bool BufferSpan::operator==(const BufferSpan& rhs) const {
        return buf_ == rhs.buf_ && offset_ == rhs.offset_ && size_ == rhs.size_;
    }

    bool BufferSpan::operator!=(const BufferSpan& rhs) const {
        return !(*this == rhs);
    }

}  // namespace mirinae


// Image
namespace mirinae {

    Image::~Image() {
        if (image_ != VK_NULL_HANDLE)
            SPDLOG_WARN("VMA Image object leaking");
    }

    void Image::init(
        const VkImageCreateInfo& create_info, VulkanMemoryAllocator allocator
    ) {
        this->destroy(allocator);

        img_info_ = create_info;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        // Without this the ocean flickers when window is resized
        alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        VK_CHECK(vmaCreateImage(
            allocator->get(),
            &img_info_,
            &alloc_info,
            &image_,
            &allocation_,
            nullptr
        ));

        assert(image_ != VK_NULL_HANDLE);
        assert(allocation_ != VK_NULL_HANDLE);
    }

    void Image::destroy(VulkanMemoryAllocator allocator) {
        img_info_ = {};

        if (image_ != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator->get(), image_, allocation_);
            image_ = VK_NULL_HANDLE;
            allocation_ = VK_NULL_HANDLE;
        }
    }

}  // namespace mirinae
