#pragma once

#include <vulkan/vulkan.h>
#include <sung/general/linalg.hpp>


namespace mirinae {

    template <typename T>
    T make_half_dim(T dim) {
        // check if T is integer
        static_assert(std::is_integral<T>::value, "T must be an integer type");

        if (dim <= 1)
            return 1;

        return dim / 2;
    }


    class ImageSubresourceLayers {

    public:
        ImageSubresourceLayers() { info_ = {}; }

        ImageSubresourceLayers& set_aspect_mask(VkImageAspectFlags flags) {
            info_.aspectMask = flags;
            return *this;
        }
        ImageSubresourceLayers& add_aspect_mask(VkImageAspectFlags flags) {
            info_.aspectMask |= flags;
            return *this;
        }

        ImageSubresourceLayers& mip_level(uint32_t level) {
            info_.mipLevel = level;
            return *this;
        }

        ImageSubresourceLayers& layer_base(uint32_t layer) {
            info_.baseArrayLayer = layer;
            return *this;
        }

        ImageSubresourceLayers& layer_count(uint32_t count) {
            info_.layerCount = count;
            return *this;
        }

        const VkImageSubresourceLayers& get() const { return info_; }

    private:
        VkImageSubresourceLayers info_;
    };

    static_assert(
        sizeof(ImageSubresourceLayers) == sizeof(VkImageSubresourceLayers)
    );


    class ImageMemoryBarrier {

    public:
        ImageMemoryBarrier() {
            info_ = {};
            info_.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            info_.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            info_.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        }

        ImageMemoryBarrier& set_src_access(VkAccessFlags flags) {
            info_.srcAccessMask = flags;
            return *this;
        }
        ImageMemoryBarrier& add_src_access(VkAccessFlags flags) {
            info_.srcAccessMask |= flags;
            return *this;
        }

        ImageMemoryBarrier& set_dst_access(VkAccessFlags flags) {
            info_.dstAccessMask = flags;
            return *this;
        }
        ImageMemoryBarrier& add_dst_access(VkAccessFlags flags) {
            info_.dstAccessMask |= flags;
            return *this;
        }

        ImageMemoryBarrier& old_layout(VkImageLayout layout) {
            info_.oldLayout = layout;
            return *this;
        }

        ImageMemoryBarrier& new_layout(VkImageLayout layout) {
            info_.newLayout = layout;
            return *this;
        }

        ImageMemoryBarrier& image(VkImage img) {
            info_.image = img;
            return *this;
        }

        ImageMemoryBarrier& set_aspect_mask(VkImageAspectFlags flags) {
            info_.subresourceRange.aspectMask = flags;
            return *this;
        }
        ImageMemoryBarrier& add_aspect_mask(VkImageAspectFlags flags) {
            info_.subresourceRange.aspectMask |= flags;
            return *this;
        }

        ImageMemoryBarrier& mip_base(uint32_t level) {
            info_.subresourceRange.baseMipLevel = level;
            return *this;
        }

        ImageMemoryBarrier& mip_count(uint32_t count) {
            info_.subresourceRange.levelCount = count;
            return *this;
        }

        ImageMemoryBarrier& layer_base(uint32_t layer) {
            info_.subresourceRange.baseArrayLayer = layer;
            return *this;
        }

        ImageMemoryBarrier& layer_count(uint32_t count) {
            info_.subresourceRange.layerCount = count;
            return *this;
        }

        const VkImageMemoryBarrier& get() const { return info_; }

        void record_single(
            VkCommandBuffer cmdbuf,
            VkPipelineStageFlags src_stage,
            VkPipelineStageFlags dst_stage
        ) const {
            vkCmdPipelineBarrier(
                cmdbuf,
                src_stage,
                dst_stage,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &info_
            );
        }

    private:
        VkImageMemoryBarrier info_;
    };

    static_assert(sizeof(ImageMemoryBarrier) == sizeof(VkImageMemoryBarrier));


    class ImageBlit {

    public:
        ImageBlit() { info_ = {}; }

        ImageSubresourceLayers& src_subres() {
            return *reinterpret_cast<ImageSubresourceLayers*>(
                &info_.srcSubresource
            );
        }

        ImageSubresourceLayers& dst_subres() {
            return *reinterpret_cast<ImageSubresourceLayers*>(
                &info_.dstSubresource
            );
        }

        sung::TVec3<int32_t>& src_offset0() {
            static_assert(sizeof(sung::TVec3<int32_t>) == sizeof(VkOffset3D));

            return *reinterpret_cast<sung::TVec3<int32_t>*>(&info_.srcOffsets[0]
            );
        }

        sung::TVec3<int32_t>& src_offset1() {
            return *reinterpret_cast<sung::TVec3<int32_t>*>(&info_.srcOffsets[1]
            );
        }

        ImageBlit& set_src_offsets_full(int32_t w, int32_t h) {
            info_.srcOffsets[0] = { 0, 0, 0 };
            info_.srcOffsets[1] = { w, h, 1 };
            return *this;
        }

        sung::TVec3<int32_t>& dst_offset0() {
            return *reinterpret_cast<sung::TVec3<int32_t>*>(&info_.dstOffsets[0]
            );
        }

        sung::TVec3<int32_t>& dst_offset1() {
            return *reinterpret_cast<sung::TVec3<int32_t>*>(&info_.dstOffsets[1]
            );
        }

        ImageBlit& set_dst_offsets_full(int32_t w, int32_t h) {
            info_.dstOffsets[0] = { 0, 0, 0 };
            info_.dstOffsets[1] = { w, h, 1 };
            return *this;
        }

        const VkImageBlit& get() const { return info_; }

    private:
        VkImageBlit info_;
    };

    static_assert(sizeof(ImageBlit) == sizeof(VkImageBlit));

}  // namespace mirinae
