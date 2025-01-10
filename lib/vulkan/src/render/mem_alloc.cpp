#include "mirinae/render/mem_alloc.hpp"

#include <cmath>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include "mirinae/lightweight/include_spdlog.hpp"


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

    void Buffer::init_staging(
        VkDeviceSize size, VulkanMemoryAllocator allocator
    ) {
        this->destroy(allocator);

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if (VK_SUCCESS != vmaCreateBuffer(
                              allocator->get(),
                              &buffer_info,
                              &alloc_info,
                              &buffer_,
                              &allocation_,
                              nullptr
                          )) {
            throw std::runtime_error(
                "failed to create VMA buffer as staging buffer"
            );
        }

        data_size_ = size;
    }

    void Buffer::init_ubuf(VkDeviceSize size, VulkanMemoryAllocator allocator) {
        this->destroy(allocator);

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if (VK_SUCCESS != vmaCreateBuffer(
                              allocator->get(),
                              &buffer_info,
                              &alloc_info,
                              &buffer_,
                              &allocation_,
                              nullptr
                          )) {
            throw std::runtime_error(
                "failed to create VMA buffer as uniform buffer"
            );
        }

        data_size_ = size;
    }

    void Buffer::init_vertices(
        VkDeviceSize size, VulkanMemoryAllocator allocator
    ) {
        this->destroy(allocator);

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

        if (VK_SUCCESS != vmaCreateBuffer(
                              allocator->get(),
                              &buffer_info,
                              &alloc_info,
                              &buffer_,
                              &allocation_,
                              nullptr
                          )) {
            throw std::runtime_error(
                "failed to create VMA buffer as vertex buffer"
            );
        }

        data_size_ = size;
    }

    void Buffer::init_indices(
        VkDeviceSize size, VulkanMemoryAllocator allocator
    ) {
        this->destroy(allocator);

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

        if (VK_SUCCESS != vmaCreateBuffer(
                              allocator->get(),
                              &buffer_info,
                              &alloc_info,
                              &buffer_,
                              &allocation_,
                              nullptr
                          )) {
            throw std::runtime_error(
                "failed to create VMA buffer as index buffer"
            );
        }

        data_size_ = size;
    }

    void Buffer::destroy(VulkanMemoryAllocator allocator) {
        if (buffer_ != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator->get(), buffer_, allocation_);
            buffer_ = VK_NULL_HANDLE;
        }
    }

    VkDeviceSize Buffer::size() const { return data_size_; }

    void Buffer::set_data(
        const void* data, size_t size, VulkanMemoryAllocator allocator
    ) {
        void* ptr;
        vmaMapMemory(allocator->get(), allocation_, &ptr);
        memcpy(ptr, data, std::min<size_t>(size, this->size()));
        vmaUnmapMemory(allocator->get(), allocation_);
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


// ImageCreateInfo
namespace mirinae {

    ImageCreateInfo::ImageCreateInfo() {
        info_.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info_.imageType = VK_IMAGE_TYPE_2D;
        info_.extent.depth = 1;
        info_.arrayLayers = 1;
        info_.mipLevels = 1;
        info_.samples = VK_SAMPLE_COUNT_1_BIT;
    }

    ImageCreateInfo& ImageCreateInfo::set_dimensions(
        uint32_t width, uint32_t height
    ) {
        info_.extent.width = width;
        info_.extent.height = height;
        return *this;
    }

    ImageCreateInfo& ImageCreateInfo::set_format(VkFormat format) {
        info_.format = format;
        return *this;
    }

    ImageCreateInfo& ImageCreateInfo::set_arr_layers(uint32_t arr_layers) {
        info_.arrayLayers = arr_layers;
        return *this;
    }

    // Usages

    ImageCreateInfo& ImageCreateInfo::reset_usage() {
        info_.usage = 0;
        return *this;
    }

    ImageCreateInfo& ImageCreateInfo::add_usage(VkImageUsageFlags usage) {
        info_.usage |= usage;
        return *this;
    }

    ImageCreateInfo& ImageCreateInfo::add_usage_sampled() {
        return this->add_usage(VK_IMAGE_USAGE_SAMPLED_BIT);
    }

    // Flags

    ImageCreateInfo& ImageCreateInfo::reset_flags() {
        info_.flags = 0;
        return *this;
    }

    ImageCreateInfo& ImageCreateInfo::add_flag(VkImageCreateFlags flags) {
        info_.flags |= flags;
        return *this;
    }

    // Mip levels

    ImageCreateInfo& ImageCreateInfo::set_mip_levels(uint32_t v) {
        info_.mipLevels = v;
        return *this;
    }

    ImageCreateInfo& ImageCreateInfo::deduce_mip_levels() {
        const auto maxdim = (std::max)(info_.extent.width, info_.extent.height);
        const auto log_val = std::floor(std::log2(maxdim));
        info_.mipLevels = static_cast<uint32_t>(log_val) + 1;
        return *this;
    }

    // Complex

    ImageCreateInfo& ImageCreateInfo::fetch_from_image(
        const dal::IImage2D& img, bool srgb
    ) {
        info_.extent.width = img.width();
        info_.extent.height = img.height();

        if (img.channels() == 4 && img.value_type_size() == sizeof(uint8_t) &&
            srgb)
            info_.format = VK_FORMAT_R8G8B8A8_SRGB;
        else if (img.channels() == 4 &&
                 img.value_type_size() == sizeof(uint8_t) && !srgb)
            info_.format = VK_FORMAT_R8G8B8A8_UNORM;
        else if (img.channels() == 3 &&
                 img.value_type_size() == sizeof(uint8_t) && srgb)
            info_.format = VK_FORMAT_R8G8B8_SRGB;
        else if (img.channels() == 3 &&
                 img.value_type_size() == sizeof(uint8_t) && !srgb)
            info_.format = VK_FORMAT_R8G8B8_UNORM;
        else if (img.channels() == 1 &&
                 img.value_type_size() == sizeof(uint8_t))
            info_.format = VK_FORMAT_R8_UNORM;
        else if (img.channels() == 4 && img.value_type_size() == sizeof(float))
            info_.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        else if (img.channels() == 3 && img.value_type_size() == sizeof(float))
            info_.format = VK_FORMAT_R32G32B32_SFLOAT;
        else if (img.channels() == 1 && img.value_type_size() == sizeof(float))
            info_.format = VK_FORMAT_R32_SFLOAT;
        else {
            const auto msg = fmt::format(
                "Cannot determine image format for IImage2D{{ channel={}, "
                "value_type_size={}, sRGB={} }}",
                img.channels(),
                img.value_type_size(),
                srgb
            );
            throw std::runtime_error(msg);
        }

        return *this;
    }

}  // namespace mirinae


// Image
namespace mirinae {

    void Image::init(
        const VkImageCreateInfo& create_info, VulkanMemoryAllocator allocator
    ) {
        this->destroy(allocator);

        img_info_ = create_info;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

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
