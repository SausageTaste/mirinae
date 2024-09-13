#include "mirinae/render/vkmajorplayers.hpp"

#include <array>

#include <spdlog/spdlog.h>


// Semaphore
namespace mirinae {

    void Semaphore::init(VkDevice logi_device) {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(
            vkCreateSemaphore(logi_device, &semaphoreInfo, nullptr, &handle_)
        );
    }

    void Semaphore::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroySemaphore(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}  // namespace mirinae


// Fence
namespace mirinae {

    void Fence::init(bool init_signaled, VkDevice logi_device) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        if (init_signaled)
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VK_CHECK(vkCreateFence(logi_device, &fenceInfo, nullptr, &handle_));
    }

    void Fence::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyFence(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    void Fence::wait(VkDevice logi_device) {
        if (VK_NULL_HANDLE == handle_) {
            spdlog::warn("Tried to wait on a fence that is not created");
            return;
        }

        vkWaitForFences(logi_device, 1, &handle_, VK_TRUE, UINT64_MAX);
    }

    void Fence::reset(VkDevice logi_device) {
        if (VK_NULL_HANDLE == handle_) {
            spdlog::warn("Tried to reset a fence that is not created");
            return;
        }

        vkResetFences(logi_device, 1, &handle_);
    }

}  // namespace mirinae


// CommandPool
namespace mirinae {

    void CommandPool::init(uint32_t graphics_queue, VkDevice logi_device) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphics_queue;
        VK_CHECK(vkCreateCommandPool(logi_device, &poolInfo, NULL, &handle_));
    }

    void CommandPool::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyCommandPool(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    VkCommandBuffer CommandPool::alloc(VkDevice logi_device) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = handle_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        VK_CHECK(
            vkAllocateCommandBuffers(logi_device, &allocInfo, &commandBuffer)
        );

        return commandBuffer;
    }

    void CommandPool::free(VkCommandBuffer cmdbuf, VkDevice logi_device) {
        vkFreeCommandBuffers(logi_device, handle_, 1, &cmdbuf);
    }

    VkCommandBuffer CommandPool::begin_single_time(VkDevice logi_device) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = handle_;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(logi_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void CommandPool::end_single_time(
        VkCommandBuffer cmdbuf, VkQueue graphics_q, VkDevice logi_device
    ) {
        vkEndCommandBuffer(cmdbuf);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;

        vkQueueSubmit(graphics_q, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_q);

        vkFreeCommandBuffers(logi_device, handle_, 1, &cmdbuf);
    }

}  // namespace mirinae


// ImageViewBuilder
namespace mirinae {

    ImageViewBuilder::ImageViewBuilder() { this->reset(); }

    void ImageViewBuilder::reset() {
        cinfo_ = {};
        cinfo_.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        cinfo_.viewType = VK_IMAGE_VIEW_TYPE_2D;
        cinfo_.format = VK_FORMAT_UNDEFINED;
        cinfo_.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        cinfo_.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        cinfo_.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        cinfo_.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        cinfo_.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cinfo_.subresourceRange.baseMipLevel = 0;
        cinfo_.subresourceRange.levelCount = 1;
        cinfo_.subresourceRange.baseArrayLayer = 0;
        cinfo_.subresourceRange.layerCount = 1;
    }

    ImageViewBuilder& ImageViewBuilder::image(VkImage image) {
        cinfo_.image = image;
        return *this;
    }

    ImageViewBuilder& ImageViewBuilder::view_type(VkImageViewType view_type) {
        cinfo_.viewType = view_type;
        return *this;
    }

    ImageViewBuilder& ImageViewBuilder::format(VkFormat format) {
        cinfo_.format = format;
        return *this;
    }

    ImageViewBuilder& ImageViewBuilder::aspect_mask(
        VkImageAspectFlags aspect_mask
    ) {
        cinfo_.subresourceRange.aspectMask = aspect_mask;
        return *this;
    }

    ImageViewBuilder& ImageViewBuilder::base_mip_level(uint32_t value) {
        cinfo_.subresourceRange.baseMipLevel = value;
        return *this;
    }

    ImageViewBuilder& ImageViewBuilder::mip_levels(uint32_t value) {
        cinfo_.subresourceRange.levelCount = value;
        return *this;
    }

    ImageViewBuilder& ImageViewBuilder::base_arr_layer(uint32_t value) {
        cinfo_.subresourceRange.baseArrayLayer = value;
        return *this;
    }

    ImageViewBuilder& ImageViewBuilder::arr_layers(uint32_t value) {
        cinfo_.subresourceRange.layerCount = value;
        return *this;
    }

    VkImageView ImageViewBuilder::build(VulkanDevice& device) const {
        VkImageView img_view;
        const auto res = vkCreateImageView(
            device.logi_device(), &cinfo_, nullptr, &img_view
        );
        return img_view;
    }

}  // namespace mirinae


// ImageView
namespace mirinae {

    ImageView::ImageView(VkImageView handle, VulkanDevice& device) {
        this->reset(handle, device);
    }

    ImageView::~ImageView() {
        if (VK_NULL_HANDLE != handle_)
            spdlog::warn(
                "ImageView object is being destroyed without being reset"
            );
    }

    ImageView::ImageView(ImageView&& other) noexcept {
        std::swap(handle_, other.handle_);
    }

    ImageView& ImageView::operator=(ImageView&& other) noexcept {
        std::swap(handle_, other.handle_);
        return *this;
    }

    void ImageView::reset(VkImageView handle, VulkanDevice& device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyImageView(device.logi_device(), handle_, nullptr);
        }
        handle_ = handle;
    }

    void ImageView::reset(
        const ImageViewBuilder& builder, VulkanDevice& device
    ) {
        this->reset(builder.build(device), device);
    }

    void ImageView::destroy(VulkanDevice& device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyImageView(device.logi_device(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}  // namespace mirinae


// FbufCinfo
namespace mirinae {

    FbufCinfo::FbufCinfo() {
        cinfo_ = {};
        cinfo_.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        cinfo_.layers = 1;
    }

    bool FbufCinfo::is_valid() const {
        if (VK_NULL_HANDLE == cinfo_.renderPass) {
            spdlog::error("renderPass is not set");
            return false;
        }

        if (0 == cinfo_.attachmentCount) {
            spdlog::error("attachmentCount is not set");
            return false;
        }

        if (nullptr == cinfo_.pAttachments) {
            spdlog::error("pAttachments is not set");
            return false;
        }

        if (0 == cinfo_.width || 0 == cinfo_.height) {
            spdlog::error("Framebuffer dimensions are not set");
            return false;
        }

        return true;
    }

    const VkFramebufferCreateInfo& FbufCinfo::get() const {
        assert(this->is_valid());
        return cinfo_;
    }

    FbufCinfo& FbufCinfo::set_rp(VkRenderPass rp) {
        cinfo_.renderPass = rp;
        return *this;
    }

    FbufCinfo& FbufCinfo::set_attach_count(uint32_t count) {
        cinfo_.attachmentCount = count;
        return *this;
    }

    FbufCinfo& FbufCinfo::set_attach(const VkImageView* attachments) {
        cinfo_.pAttachments = attachments;
        return *this;
    }

    FbufCinfo& FbufCinfo::set_dim(uint32_t width, uint32_t height) {
        cinfo_.width = width;
        cinfo_.height = height;
        return *this;
    }

    FbufCinfo& FbufCinfo::set_dim(const VkExtent2D& extent) {
        cinfo_.width = extent.width;
        cinfo_.height = extent.height;
        return *this;
    }

    FbufCinfo& FbufCinfo::set_layers(uint32_t layers) {
        cinfo_.layers = layers;
        return *this;
    }

    FbufCinfo& FbufCinfo::set_single_attach(const VkImageView& attachments) {
        cinfo_.attachmentCount = 1;
        cinfo_.pAttachments = &attachments;
        return *this;
    }

    FbufCinfo& FbufCinfo::set_attachments(const std::vector<VkImageView>& arr) {
        cinfo_.attachmentCount = static_cast<uint32_t>(arr.size());
        cinfo_.pAttachments = arr.data();
        return *this;
    }

}  // namespace mirinae


// Fbuf
namespace mirinae {

    Fbuf::~Fbuf() {
        if (VK_NULL_HANDLE != handle_) {
            spdlog::warn("Fbuf object is being destroyed without being reset");
        }
    }

    void Fbuf::init(
        const VkFramebufferCreateInfo& cinfo, VkDevice logi_device
    ) {
        this->destroy(logi_device);
        VK_CHECK(vkCreateFramebuffer(logi_device, &cinfo, nullptr, &handle_));
    }

    void Fbuf::reset(VkFramebuffer handle, VkDevice logi_device) {
        this->destroy(logi_device);
        handle_ = handle;
    }

    void Fbuf::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyFramebuffer(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}  // namespace mirinae
