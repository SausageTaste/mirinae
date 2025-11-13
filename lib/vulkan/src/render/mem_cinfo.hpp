#pragma once

#include <memory>

#include "render/mem_alloc.hpp"


namespace mirinae {

    class BufferCreateInfo : public BufferCinfoBundle {

    public:
        BufferCreateInfo();

        BufferCreateInfo& reset();

        BufferCreateInfo& set_size(VkDeviceSize size);
        BufferCreateInfo& set_usage(VkBufferUsageFlags usage);
        BufferCreateInfo& add_usage(VkBufferUsageFlags usage);
        BufferCreateInfo& reset_usage();

        BufferCreateInfo& prefer_device();
        BufferCreateInfo& prefer_host();

        // VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
        BufferCreateInfo& add_alloc_flag_host_access_seq_write();

        BufferCreateInfo& preset_staging(VkDeviceSize size);
        BufferCreateInfo& preset_ubuf(VkDeviceSize size);
        BufferCreateInfo& preset_vertices(VkDeviceSize size);
        BufferCreateInfo& preset_indices(VkDeviceSize size);

        template <typename T>
        BufferCreateInfo& preset_ubuf() {
            static_assert(std::is_standard_layout<T>::value, "");
            static_assert(std::is_trivially_copyable<T>::value, "");
            return this->preset_ubuf(sizeof(T));
        }
    };


    class ImageCreateInfo {

    public:
        ImageCreateInfo();
        const VkImageCreateInfo& get() const { return info_; }

        VkFormat format() const { return info_.format; }
        uint32_t mip_levels() const { return info_.mipLevels; }

        ImageCreateInfo& set_dimensions(uint32_t len) {
            return this->set_dimensions(len, len);
        }
        ImageCreateInfo& set_dimensions(uint32_t width, uint32_t height);
        ImageCreateInfo& set_dim3(uint32_t x, uint32_t y, uint32_t z);
        ImageCreateInfo& set_type(VkImageType type);
        ImageCreateInfo& set_format(VkFormat format);
        ImageCreateInfo& set_arr_layers(uint32_t arr_layers);

        ImageCreateInfo& reset_usage();
        ImageCreateInfo& add_usage(VkImageUsageFlags usage);
        ImageCreateInfo& add_usage_sampled();

        ImageCreateInfo& reset_flags();
        ImageCreateInfo& add_flag(VkImageCreateFlags flags);

        ImageCreateInfo& set_mip_levels(uint32_t v);
        ImageCreateInfo& deduce_mip_levels();

        ImageCreateInfo& fetch_from_image(const dal::IImage2D& img, bool srgb);

    private:
        VkImageCreateInfo info_ = {};
    };


}  // namespace mirinae
