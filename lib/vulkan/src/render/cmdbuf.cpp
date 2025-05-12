#include "mirinae/vulkan_pch.h"

#include "mirinae/render/cmdbuf.hpp"

#include "mirinae/render/enum_str.hpp"
#include "mirinae/render/mem_alloc.hpp"
#include "mirinae/render/vkdevice.hpp"


// Viewport
namespace mirinae {

    static_assert(sizeof(Viewport) == sizeof(VkViewport));

    Viewport::Viewport() {
        info_ = {};
        info_.minDepth = 0;
        info_.maxDepth = 1;
    }

    Viewport::Viewport(const VkExtent2D& wh) {
        info_ = {};
        info_.minDepth = 0;
        info_.maxDepth = 1;
        info_.width = static_cast<float>(wh.width);
        info_.height = static_cast<float>(wh.height);
    }

    Viewport& Viewport::set_wh(const VkExtent2D& wh) {
        return this->set_wh(wh.width, wh.height);
    }

    const VkViewport& Viewport::get() const { return info_; }

    void Viewport::record_single(VkCommandBuffer cmdbuf) const {
        vkCmdSetViewport(cmdbuf, 0, 1, &info_);
    }

}  // namespace mirinae


// ImageMemoryBarrier
namespace mirinae {

    static_assert(sizeof(ImageMemoryBarrier) == sizeof(VkImageMemoryBarrier));

#define CLS ImageMemoryBarrier

    CLS::CLS() {
        info_ = {};
        info_.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        info_.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        info_.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }

    CLS& CLS::set_src_access(VkAccessFlags flags) {
        info_.srcAccessMask = flags;
        return *this;
    }

    CLS& CLS::add_src_access(VkAccessFlags flags) {
        info_.srcAccessMask |= flags;
        return *this;
    }

    CLS& CLS::set_dst_access(VkAccessFlags flags) {
        info_.dstAccessMask = flags;
        return *this;
    }

    CLS& CLS::add_dst_access(VkAccessFlags flags) {
        info_.dstAccessMask |= flags;
        return *this;
    }

    CLS& CLS::old_layout(VkImageLayout layout) {
        info_.oldLayout = layout;
        return *this;
    }

    CLS& CLS::new_layout(VkImageLayout layout) {
        info_.newLayout = layout;
        return *this;
    }

    CLS& CLS::image(VkImage img) {
        info_.image = img;
        return *this;
    }

    CLS& CLS::set_aspect_mask(VkImageAspectFlags flags) {
        info_.subresourceRange.aspectMask = flags;
        return *this;
    }

    CLS& CLS::add_aspect_mask(VkImageAspectFlags flags) {
        info_.subresourceRange.aspectMask |= flags;
        return *this;
    }

    CLS& CLS::mip_base(uint32_t level) {
        info_.subresourceRange.baseMipLevel = level;
        return *this;
    }

    CLS& CLS::mip_count(uint32_t count) {
        info_.subresourceRange.levelCount = count;
        return *this;
    }

    CLS& CLS::layer_base(uint32_t layer) {
        info_.subresourceRange.baseArrayLayer = layer;
        return *this;
    }

    CLS& CLS::layer_count(uint32_t count) {
        info_.subresourceRange.layerCount = count;
        return *this;
    }

    const VkImageMemoryBarrier& CLS::get() const { return info_; }

    void CLS::record_single(
        VkCommandBuffer cmdbuf,
        VkPipelineStageFlags src_stage,
        VkPipelineStageFlags dst_stage
    ) const {
        vkCmdPipelineBarrier(
            cmdbuf, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &info_
        );
    }

#undef CLS

}  // namespace mirinae


// ImageBlit
namespace mirinae {

    static_assert(sizeof(ImageBlit) == sizeof(VkImageBlit));

#define CLS ImageBlit

    CLS::CLS() { info_ = {}; }

    ImageSubresourceLayers& CLS::src_subres() {
        return *reinterpret_cast<ImageSubresourceLayers*>(&info_.srcSubresource
        );
    }

    ImageSubresourceLayers& CLS::dst_subres() {
        return *reinterpret_cast<ImageSubresourceLayers*>(&info_.dstSubresource
        );
    }

    CLS& CLS::set_src_offsets_full(int32_t w, int32_t h) {
        info_.srcOffsets[0] = { 0, 0, 0 };
        info_.srcOffsets[1] = { w, h, 1 };
        return *this;
    }

    CLS& CLS::set_dst_offsets_full(int32_t w, int32_t h) {
        info_.dstOffsets[0] = { 0, 0, 0 };
        info_.dstOffsets[1] = { w, h, 1 };
        return *this;
    }

    const VkImageBlit& CLS::get() const { return info_; }

#undef CLS

}  // namespace mirinae


// DescSetBindInfo
namespace mirinae {

#define CLS DescSetBindInfo

    CLS::CLS(VkPipelineLayout layout) : layout_(layout) {}

    DescSetBindInfo& CLS::bind_point(VkPipelineBindPoint point) {
        bind_point_ = point;
        return *this;
    }

    DescSetBindInfo& CLS::layout(VkPipelineLayout layout) {
        layout_ = layout;
        return *this;
    }

    DescSetBindInfo& CLS::first_set(uint32_t set) {
        first_set_ = set;
        return *this;
    }

    DescSetBindInfo& CLS::set(VkDescriptorSet set) {
        desc_sets_.resize(1);
        desc_sets_[0] = set;
        return *this;
    }

    DescSetBindInfo& CLS::add(VkDescriptorSet set) {
        desc_sets_.push_back(set);
        return *this;
    }

    DescSetBindInfo& CLS::clear() {
        desc_sets_.clear();
        return *this;
    }

    void CLS::record(VkCommandBuffer cmdbuf) {
        vkCmdBindDescriptorSets(
            cmdbuf,
            bind_point_,
            layout_,
            first_set_,
            static_cast<uint32_t>(desc_sets_.size()),
            desc_sets_.data(),
            0,
            nullptr
        );
    }

#undef CLS

}  // namespace mirinae


// PushConstInfo
namespace mirinae {

    PushConstInfo& PushConstInfo::layout(VkPipelineLayout layout) {
        layout_ = layout;
        return *this;
    }

    PushConstInfo& PushConstInfo::set_stage(VkShaderStageFlags stages) {
        stages_ = stages;
        return *this;
    }

    PushConstInfo& PushConstInfo::add_stage(VkShaderStageFlags stage) {
        stages_ |= stage;
        return *this;
    }

    PushConstInfo& PushConstInfo::add_stage_vert() {
        return this->add_stage(VK_SHADER_STAGE_VERTEX_BIT);
    }

    PushConstInfo& PushConstInfo::add_stage_tesc() {
        return this->add_stage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    }

    PushConstInfo& PushConstInfo::add_stage_tese() {
        return this->add_stage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    }

    PushConstInfo& PushConstInfo::add_stage_frag() {
        return this->add_stage(VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    void PushConstInfo::record(
        VkCommandBuffer cmdbuf, const void* data, uint32_t size, uint32_t offset
    ) const {
        vkCmdPushConstants(cmdbuf, layout_, stages_, offset, size, data);
    }

}  // namespace mirinae


// SubmitInfo
namespace mirinae {

    SubmitInfo::SubmitInfo() {
        info_ = {};
        info_.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    }

    SubmitInfo& SubmitInfo::add_wait_semaph(
        VkSemaphore semaph, VkPipelineStageFlags stage
    ) {
        wait_semaph_.push_back(semaph);
        wait_stages_.push_back(stage);

        info_.waitSemaphoreCount = static_cast<uint32_t>(wait_semaph_.size());
        info_.pWaitSemaphores = wait_semaph_.data();
        info_.pWaitDstStageMask = wait_stages_.data();
        return *this;
    }

    SubmitInfo& SubmitInfo::add_wait_semaph_color_attach_out(
        VkSemaphore semaph
    ) {
        return this->add_wait_semaph(
            semaph, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        );
    }

    SubmitInfo& SubmitInfo::add_cmdbuf(VkCommandBuffer cmdbuf) {
        cmdbufs_.push_back(cmdbuf);

        info_.commandBufferCount = static_cast<uint32_t>(cmdbufs_.size());
        info_.pCommandBuffers = cmdbufs_.data();
        return *this;
    }

    SubmitInfo& SubmitInfo::add_cmdbuf(
        const VkCommandBuffer* cmdbuf, size_t count
    ) {
        for (size_t i = 0; i < count; i++) {
            cmdbufs_.push_back(cmdbuf[i]);
        }

        info_.commandBufferCount = static_cast<uint32_t>(cmdbufs_.size());
        info_.pCommandBuffers = cmdbufs_.data();
        return *this;
    }

    SubmitInfo& SubmitInfo::add_signal_semaph(VkSemaphore semaph) {
        signal_semaphores_.push_back(semaph);

        info_.signalSemaphoreCount = static_cast<uint32_t>(
            signal_semaphores_.size()
        );
        info_.pSignalSemaphores = signal_semaphores_.data();
        return *this;
    }

    const VkSubmitInfo* SubmitInfo::get() const { return &info_; }

    void SubmitInfo::queue_submit_single(VkQueue queue, VkFence fence) {
        VK_CHECK(vkQueueSubmit(queue, 1, &info_, fence));
    }

}  // namespace mirinae


// PresentInfo
namespace mirinae {

    PresentInfo::PresentInfo() {
        info_ = {};
        info_.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    }

    PresentInfo& PresentInfo::add_wait_semaph(const VkSemaphore& semaph) {
        wait_semaphores_.push_back(semaph);

        info_.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores_.size()
        );
        info_.pWaitSemaphores = wait_semaphores_.data();
        return *this;
    }

    PresentInfo& PresentInfo::add_swapchain(const VkSwapchainKHR& swapchain) {
        swapchains_.push_back(swapchain);

        info_.swapchainCount = static_cast<uint32_t>(swapchains_.size());
        info_.pSwapchains = swapchains_.data();
        return *this;
    }

    PresentInfo& PresentInfo::add_image_index(uint32_t index) {
        image_indices_.push_back(index);

        info_.pImageIndices = image_indices_.data();
        return *this;
    }

    void PresentInfo::queue_present(VkQueue queue) {
        const auto res = vkQueuePresentKHR(queue, &info_);

        switch (res) {
            case VK_SUCCESS:
                break;
            case VK_SUBOPTIMAL_KHR:
                SPDLOG_WARN("Queue present failed: Swapchain is suboptimal");
                break;
            case VK_ERROR_OUT_OF_DATE_KHR:
                SPDLOG_WARN("Queue present failed: Swapchain is invalid");
                break;
            default:
                SPDLOG_WARN("Queue present failed: {}", to_str(res));
                break;
        }
    }

}  // namespace mirinae


namespace mirinae {

    void begin_cmdbuf(VkCommandBuffer cmdbuf) {
        VkCommandBufferBeginInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags = 0;
        info.pInheritanceInfo = nullptr;

        const auto res = vkBeginCommandBuffer(cmdbuf, &info);
        MIRINAE_ASSERT(VK_SUCCESS == res);
    }

    void end_cmdbuf(VkCommandBuffer cmdbuf) {
        const auto res = vkEndCommandBuffer(cmdbuf);
        MIRINAE_ASSERT(VK_SUCCESS == res);
    }

}  // namespace mirinae
