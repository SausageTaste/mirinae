#pragma once

#include <vulkan/vulkan.h>


namespace mirinae {

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

}  // namespace mirinae
