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


    class Rect2D {

    public:
        Rect2D() { info_ = {}; }

        Rect2D(const VkExtent2D& wh) {
            info_ = {};
            info_.extent = wh;
        }

        template <typename T>
        Rect2D& set_xy(T x, T y) {
            info_.offset.x = static_cast<int32_t>(x);
            info_.offset.y = static_cast<int32_t>(y);
            return *this;
        }

        template <typename TArr>
        Rect2D& set_xy(const TArr& x) {
            return this->set_xy(x[0], x[1]);
        }

        template <typename T>
        Rect2D& set_wh(T w, T h) {
            info_.extent.width = static_cast<uint32_t>(w);
            info_.extent.height = static_cast<uint32_t>(h);
            return *this;
        }

        template <typename TArr>
        Rect2D& set_wh(const TArr& wh) {
            return this->set_wh(wh[0], wh[1]);
        }

        Rect2D& set_wh(const VkExtent2D& wh) {
            info_.extent = wh;
            return *this;
        }

        const VkRect2D& get() const { return info_; }

        const VkExtent2D& extent2d() const { return info_.extent; }

        void record_scissor(VkCommandBuffer cmdbuf) const {
            vkCmdSetScissor(cmdbuf, 0, 1, &info_);
        }

    private:
        VkRect2D info_ = {};
    };


    class Viewport {

    public:
        Viewport() {
            info_ = {};
            info_.minDepth = 0;
            info_.maxDepth = 1;
        }

        Viewport(const VkExtent2D& wh) {
            info_ = {};
            info_.minDepth = 0;
            info_.maxDepth = 1;
            info_.width = static_cast<float>(wh.width);
            info_.height = static_cast<float>(wh.height);
        }

        template <typename T>
        Viewport& set_xy(T x, T y) {
            info_.x = static_cast<float>(x);
            info_.y = static_cast<float>(y);
            return *this;
        }

        template <typename TArr>
        Viewport& set_xy(const TArr& x) {
            return this->set_xy(x[0], x[1]);
        }

        template <typename T>
        Viewport& set_wh(T w, T h) {
            info_.width = static_cast<float>(w);
            info_.height = static_cast<float>(h);
            return *this;
        }

        template <typename TArr>
        Viewport& set_wh(const TArr& wh) {
            return this->set_wh(wh[0], wh[1]);
        }

        Viewport& set_wh(const VkExtent2D& wh) {
            return this->set_wh(wh.width, wh.height);
        }

        const VkViewport& get() const { return info_; }

        void record_single(VkCommandBuffer cmdbuf) const {
            vkCmdSetViewport(cmdbuf, 0, 1, &info_);
        }

    private:
        VkViewport info_;
    };


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
