#pragma once

#include <vector>

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

    static_assert(sizeof(Rect2D) == sizeof(VkRect2D));


    class Viewport {

    public:
        Viewport();
        Viewport(const VkExtent2D& wh);

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

        Viewport& set_wh(const VkExtent2D& wh);

        const VkViewport& get() const;

        void record_single(VkCommandBuffer cmdbuf) const;

    private:
        VkViewport info_;
    };


    class RenderPassBeginInfo {

    public:
        RenderPassBeginInfo() {
            info_ = {};
            info_.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        }

        RenderPassBeginInfo& rp(VkRenderPass pass) {
            info_.renderPass = pass;
            return *this;
        }

        RenderPassBeginInfo& fbuf(VkFramebuffer fbuf) {
            info_.framebuffer = fbuf;
            return *this;
        }

        template <typename T>
        RenderPassBeginInfo& xy(T x, T y) {
            info_.renderArea.offset.x = static_cast<int32_t>(x);
            info_.renderArea.offset.y = static_cast<int32_t>(y);
            return *this;
        }

        template <typename TArr>
        RenderPassBeginInfo& xy(const TArr& x) {
            return this->xy(x[0], x[1]);
        }

        template <typename T>
        RenderPassBeginInfo& wh(T w, T h) {
            info_.renderArea.extent.width = static_cast<uint32_t>(w);
            info_.renderArea.extent.height = static_cast<uint32_t>(h);
            return *this;
        }

        template <typename TArr>
        RenderPassBeginInfo& wh(const TArr& wh) {
            return this->wh(wh[0], wh[1]);
        }

        RenderPassBeginInfo& wh(const VkExtent2D& wh) {
            info_.renderArea.extent = wh;
            return *this;
        }

        RenderPassBeginInfo& clear_value_count(uint32_t count) {
            info_.clearValueCount = count;
            return *this;
        }

        RenderPassBeginInfo& clear_values(const VkClearValue* values) {
            info_.pClearValues = values;
            return *this;
        }

        void record_begin(VkCommandBuffer cmdbuf) const {
            vkCmdBeginRenderPass(cmdbuf, &info_, VK_SUBPASS_CONTENTS_INLINE);
        }

    private:
        VkRenderPassBeginInfo info_;
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
        ImageMemoryBarrier();

        ImageMemoryBarrier& set_src_access(VkAccessFlags flags);
        ImageMemoryBarrier& add_src_access(VkAccessFlags flags);

        ImageMemoryBarrier& set_dst_access(VkAccessFlags flags);
        ImageMemoryBarrier& add_dst_access(VkAccessFlags flags);

        ImageMemoryBarrier& old_layout(VkImageLayout layout);
        ImageMemoryBarrier& new_layout(VkImageLayout layout);

        ImageMemoryBarrier& image(VkImage img);

        ImageMemoryBarrier& set_aspect_mask(VkImageAspectFlags flags);
        ImageMemoryBarrier& add_aspect_mask(VkImageAspectFlags flags);

        ImageMemoryBarrier& mip_base(uint32_t level);
        ImageMemoryBarrier& mip_count(uint32_t count);
        ImageMemoryBarrier& layer_base(uint32_t layer);
        ImageMemoryBarrier& layer_count(uint32_t count);

        const VkImageMemoryBarrier& get() const;

        void record_single(
            VkCommandBuffer cmdbuf,
            VkPipelineStageFlags src_stage,
            VkPipelineStageFlags dst_stage
        ) const;

    private:
        VkImageMemoryBarrier info_;
    };


    class ImageBlit {

    public:
        ImageBlit();

        ImageSubresourceLayers& src_subres();
        ImageSubresourceLayers& dst_subres();

        ImageBlit& set_src_offsets_full(int32_t w, int32_t h);
        ImageBlit& set_dst_offsets_full(int32_t w, int32_t h);

        const VkImageBlit& get() const;

    private:
        VkImageBlit info_;
    };


    class DescSetBindInfo {

    public:
        DescSetBindInfo() = default;
        DescSetBindInfo(VkPipelineLayout layout);

        DescSetBindInfo& layout(VkPipelineLayout layout);

        DescSetBindInfo& first_set(uint32_t set);

        DescSetBindInfo& set(VkDescriptorSet set);
        DescSetBindInfo& add(VkDescriptorSet set);
        DescSetBindInfo& clear();

        void record(VkCommandBuffer cmdbuf);

    private:
        std::vector<VkDescriptorSet> desc_sets_;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        uint32_t first_set_ = 0;
    };


    class PushConstInfo {

    public:
        PushConstInfo& layout(VkPipelineLayout layout);

        PushConstInfo& set_stage(VkShaderStageFlags stages);
        PushConstInfo& add_stage(VkShaderStageFlags stage);
        PushConstInfo& add_stage_vert();
        PushConstInfo& add_stage_tesc();
        PushConstInfo& add_stage_tese();
        PushConstInfo& add_stage_frag();

        void record(
            VkCommandBuffer cmdbuf,
            const void* data,
            uint32_t size,
            uint32_t offset
        ) const;

        template <typename T>
        void record(VkCommandBuffer cmdbuf, const T& data, uint32_t offset = 0)
            const {
            this->record(cmdbuf, &data, sizeof(data), offset);
        }

    private:
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        VkShaderStageFlags stages_ = 0;
    };


    class SubmitInfo {

    public:
        SubmitInfo();

        SubmitInfo& add_wait_semaph(
            VkSemaphore semaph, VkPipelineStageFlags stage
        );
        SubmitInfo& add_wait_semaph_color_attach_out(VkSemaphore semaph);

        SubmitInfo& add_cmdbuf(VkCommandBuffer cmdbuf);
        SubmitInfo& add_signal_semaph(VkSemaphore semaph);

        const VkSubmitInfo* get() const;
        void queue_submit_single(VkQueue queue, VkFence fence);

    private:
        VkSubmitInfo info_;
        std::vector<VkSemaphore> wait_semaph_;
        std::vector<VkPipelineStageFlags> wait_stages_;
        std::vector<VkSemaphore> signal_semaphores_;
        std::vector<VkCommandBuffer> cmdbufs_;
    };


    class PresentInfo {

    public:
        PresentInfo();

        PresentInfo& add_wait_semaph(const VkSemaphore& semaph);
        PresentInfo& add_swapchain(const VkSwapchainKHR& swapchain);
        PresentInfo& add_image_index(uint32_t index);

        void queue_present(VkQueue queue);

    private:
        VkPresentInfoKHR info_;
        std::vector<VkSemaphore> wait_semaphores_;
        std::vector<VkSwapchainKHR> swapchains_;
        std::vector<uint32_t> image_indices_;
    };

}  // namespace mirinae
