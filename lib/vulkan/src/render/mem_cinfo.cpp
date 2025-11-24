#include "render/mem_cinfo.hpp"

#include "mirinae/lightweight/include_spdlog.hpp"


// BufferCreateInfo
namespace mirinae {

    BufferCreateInfo::BufferCreateInfo() { this->reset(); }

    BufferCreateInfo& BufferCreateInfo::reset() {
        buf_info_ = {};
        buf_info_.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

        alloc_info_ = {};
        alloc_info_.usage = VMA_MEMORY_USAGE_AUTO;

        return *this;
    }

    BufferCreateInfo& BufferCreateInfo::set_size(VkDeviceSize size) {
        buf_info_.size = size;
        return *this;
    }

    BufferCreateInfo& BufferCreateInfo::set_usage(VkBufferUsageFlags usage) {
        buf_info_.usage = usage;
        return *this;
    }

    BufferCreateInfo& BufferCreateInfo::add_usage(VkBufferUsageFlags usage) {
        buf_info_.usage |= usage;
        return *this;
    }

    BufferCreateInfo& BufferCreateInfo::reset_usage() {
        buf_info_.usage = 0;
        return *this;
    }

    BufferCreateInfo& BufferCreateInfo::prefer_device() {
        alloc_info_.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        return *this;
    }

    BufferCreateInfo& BufferCreateInfo::prefer_host() {
        alloc_info_.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        return *this;
    }

    BufferCreateInfo& BufferCreateInfo::add_alloc_flag_host_access_seq_write() {
        alloc_info_.flags |=
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        return *this;
    }

    BufferCreateInfo& BufferCreateInfo::preset_staging(VkDeviceSize size) {
        return this->reset()
            .set_size(size)
            .add_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
            .add_alloc_flag_host_access_seq_write();
    }

    BufferCreateInfo& BufferCreateInfo::preset_ubuf(VkDeviceSize size) {
        return this->reset()
            .set_size(size)
            .add_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
            .add_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
            .add_alloc_flag_host_access_seq_write();
    }

    BufferCreateInfo& BufferCreateInfo::preset_vertices(VkDeviceSize size) {
        return this->reset()
            .set_size(size)
            .add_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
            .add_usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }

    BufferCreateInfo& BufferCreateInfo::preset_indices(VkDeviceSize size) {
        return this->reset()
            .set_size(size)
            .add_usage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
            .add_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT);
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

    ImageCreateInfo& ImageCreateInfo::set_dim3(
        uint32_t x, uint32_t y, uint32_t z
    ) {
        info_.extent.width = x;
        info_.extent.height = y;
        info_.extent.depth = z;
        return *this;
    }

    ImageCreateInfo& ImageCreateInfo::set_type(VkImageType type) {
        info_.imageType = type;
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
            MIRINAE_ABORT(
                "Cannot determine image format for IImage2D{{ channel={}, "
                "value_type_size={}, sRGB={} }}",
                img.channels(),
                img.value_type_size(),
                srgb
            );
        }

        return *this;
    }

}  // namespace mirinae
