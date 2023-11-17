#include "mirinae/render/mem_alloc.hpp"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>


// VulkanMemoryAllocator_T
namespace mirinae {

    class VulkanMemoryAllocator_T {

    public:
        VulkanMemoryAllocator_T(VkInstance instance, VkPhysicalDevice phys_device, VkDevice logi_device) {
            VmaVulkanFunctions vma_vulkan_funcs = {};
            vma_vulkan_funcs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
            vma_vulkan_funcs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

            VmaAllocatorCreateInfo cinfo = {};
            cinfo.physicalDevice = phys_device;
            cinfo.device = logi_device;
            cinfo.instance = instance;
            cinfo.pVulkanFunctions = &vma_vulkan_funcs;

            if (vmaCreateAllocator(&cinfo, &allocator_) != VK_SUCCESS) {
                throw std::runtime_error("failed to create VMA allocator");
            }
        }

        ~VulkanMemoryAllocator_T() {
            this->destroy();
        }

        void destroy() {
            if (allocator_ != VK_NULL_HANDLE) {
                vmaDestroyAllocator(allocator_);
                allocator_ = VK_NULL_HANDLE;
            }
        }

        VmaAllocator get() {
            return allocator_;
        }

    private:
        VmaAllocator allocator_ = VK_NULL_HANDLE;

    };

}


// VulkanMemoryAllocator
namespace mirinae {

    VulkanMemoryAllocator create_vma_allocator(VkInstance instance, VkPhysicalDevice phys_device, VkDevice logi_device) {
        return new VulkanMemoryAllocator_T(instance, phys_device, logi_device);
    }

    void destroy_vma_allocator(VulkanMemoryAllocator allocator) {
        delete allocator;
    }

}


// Buffer
namespace mirinae {

    void Buffer::init_staging(VkDeviceSize size, VulkanMemoryAllocator allocator) {
        this->destroy(allocator);

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if (VK_SUCCESS != vmaCreateBuffer(allocator->get(), &buffer_info, &alloc_info, &buffer_, &allocation_, nullptr)) {
            throw std::runtime_error("failed to create VMA buffer as staging buffer");
        }
    }

    void Buffer::init_ubuf(VkDeviceSize size, VulkanMemoryAllocator allocator) {
        this->destroy(allocator);

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if (VK_SUCCESS != vmaCreateBuffer(allocator->get(), &buffer_info, &alloc_info, &buffer_, &allocation_, nullptr)) {
            throw std::runtime_error("failed to create VMA buffer as uniform buffer");
        }
    }

    void Buffer::init_vertices(VkDeviceSize size, VulkanMemoryAllocator allocator) {
        this->destroy(allocator);

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

        if (VK_SUCCESS != vmaCreateBuffer(allocator->get(), &buffer_info, &alloc_info, &buffer_, &allocation_, nullptr)) {
            throw std::runtime_error("failed to create VMA buffer as vertex buffer");
        }
    }

    void Buffer::init_indices(VkDeviceSize size, VulkanMemoryAllocator allocator) {
        this->destroy(allocator);

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

        if (VK_SUCCESS != vmaCreateBuffer(allocator->get(), &buffer_info, &alloc_info, &buffer_, &allocation_, nullptr)) {
            throw std::runtime_error("failed to create VMA buffer as index buffer");
        }
    }


    void Buffer::destroy(VulkanMemoryAllocator allocator) {
        if (buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator->get(), buffer_, allocation_);
            buffer_ = VK_NULL_HANDLE;
        }
    }

    VkDeviceSize Buffer::size() const {
        return allocation_->GetSize();
    }

    void Buffer::set_data(const void* data, size_t size, VulkanMemoryAllocator allocator) {
        void* ptr;
        vmaMapMemory(allocator->get(), allocation_, &ptr);
        memcpy(ptr, data, std::min<size_t>(size, this->size()));
        vmaUnmapMemory(allocator->get(), allocation_);
    }

    void Buffer::record_copy_cmd(const Buffer& src, VkCommandBuffer cmdbuf, VkDevice logi_device) {
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

}
