#include "mirinae/render/vkmajorplayers.hpp"

#include <array>

#include <spdlog/spdlog.h>


// Semaphore
namespace mirinae {

    void Semaphore::init(VkDevice logi_device) {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (vkCreateSemaphore(logi_device, &semaphoreInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }

    void Semaphore::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroySemaphore(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}


// Fence
namespace mirinae {

    void Fence::init(bool init_signaled, VkDevice logi_device) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        if (init_signaled)
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateFence(logi_device, &fenceInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
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

}


namespace {

    class AttachmentDescBuilder {

    public:
        const VkAttachmentDescription* data() const {
            return attachments_.data();
        }

        uint32_t size() const {
            return static_cast<uint32_t>(attachments_.size());
        }

        VkAttachmentDescription& add(
            const VkFormat format,
            const VkImageLayout final_layout,
            const VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            const VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
            const VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE,
            const VkAttachmentLoadOp stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            const VkAttachmentStoreOp stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            const VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT
        ) {
            auto& added = attachments_.emplace_back();
            added = {};

            added.format = format;
            added.samples = samples;
            added.loadOp = load_op;
            added.storeOp = store_op;
            added.stencilLoadOp = stencil_load_op;
            added.stencilStoreOp = stencil_store_op;
            added.initialLayout = initial_layout;
            added.finalLayout = final_layout;

            return added;
        }

    private:
        std::vector<VkAttachmentDescription> attachments_;

    };


    class AttachmentRefBuilder {

    public:
        const VkAttachmentReference* data() const {
            return attachments_.data();
        }

        uint32_t size() const {
            return static_cast<uint32_t>(attachments_.size());
        }

        void add(uint32_t index, VkImageLayout layout) {
            auto& added = attachments_.emplace_back();
            added = {};

            added.attachment = index;
            added.layout = layout;
        }

    private:
        std::vector<VkAttachmentReference> attachments_;

    };

}


// RenderPass
namespace mirinae {

    void RenderPass::init(VkFormat swapchain_format, VkFormat depth_format, VkDevice logi_device) {
        this->destroy(logi_device);

        ::AttachmentDescBuilder attachments;
        attachments.add(swapchain_format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        attachments.add(depth_format, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        ::AttachmentRefBuilder color_attachment_refs;
        color_attachment_refs.add(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        const VkAttachmentReference depth_attachment_ref{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = color_attachment_refs.size();
        subpass.pColorAttachments = color_attachment_refs.data();
        subpass.pDepthStencilAttachment = &depth_attachment_ref;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        create_info.attachmentCount = attachments.size();
        create_info.pAttachments = attachments.data();
        create_info.subpassCount = 1;
        create_info.pSubpasses = &subpass;
        create_info.dependencyCount = 1;
        create_info.pDependencies = &dependency;

        if (VK_SUCCESS != vkCreateRenderPass(logi_device, &create_info, nullptr, &handle_)) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void RenderPass::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyRenderPass(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}


// Pipeline
namespace mirinae {

    Pipeline::Pipeline(VkPipeline pipeline, VkPipelineLayout layout) {
        pipeline_ = pipeline;
        layout_ = layout;
    }

    void Pipeline::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != pipeline_) {
            vkDestroyPipeline(logi_device, pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }

        if (VK_NULL_HANDLE != layout_) {
            vkDestroyPipelineLayout(logi_device, layout_, nullptr);
            layout_ = VK_NULL_HANDLE;
        }
    }

}


// Framebuffer
namespace mirinae {

    void Framebuffer::init(const VkExtent2D& swapchain_extent, VkImageView view, VkImageView depth_view, RenderPass& renderpass, VkDevice logi_device) {
        std::array<VkImageView, 2> attachments{
            view,
            depth_view,
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderpass.get();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchain_extent.width;
        framebufferInfo.height = swapchain_extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(logi_device, &framebufferInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }

    void Framebuffer::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroyFramebuffer(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}


// CommandPool
namespace mirinae {

    void CommandPool::init(uint32_t graphics_queue, VkDevice logi_device) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphics_queue;

        if (vkCreateCommandPool(logi_device, &poolInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
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
        if (vkAllocateCommandBuffers(logi_device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }

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

    void CommandPool::end_single_time(VkCommandBuffer cmdbuf, VkQueue graphics_q, VkDevice logi_device) {
        vkEndCommandBuffer(cmdbuf);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;

        vkQueueSubmit(graphics_q, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_q);

        vkFreeCommandBuffers(logi_device, handle_, 1, &cmdbuf);
    }

}


// Sampler
namespace mirinae {

    void Sampler::init(bool enable_anisotropy, float max_anisotropy, VkDevice logi_device) {
        this->destroy(logi_device);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = enable_anisotropy ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = max_anisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0;
        samplerInfo.minLod = 0;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

        if (vkCreateSampler(logi_device, &samplerInfo, nullptr, &handle_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void Sampler::destroy(VkDevice logi_device) {
        if (VK_NULL_HANDLE != handle_) {
            vkDestroySampler(logi_device, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}


// Free functions
namespace mirinae {

    VkVertexInputBindingDescription make_vertex_static_binding_description() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(VertexStatic);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    std::vector<VkVertexInputAttributeDescription> make_vertex_static_attribute_descriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 0;
            description.format = VK_FORMAT_R32G32B32_SFLOAT;
            description.offset = offsetof(VertexStatic, pos_);
        }

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 1;
            description.format = VK_FORMAT_R32G32B32_SFLOAT;
            description.offset = offsetof(VertexStatic, normal_);
        }

        {
            auto& description = attributeDescriptions.emplace_back();
            description.binding = 0;
            description.location = 2;
            description.format = VK_FORMAT_R32G32_SFLOAT;
            description.offset = offsetof(VertexStatic, texcoord_);
        }

        return attributeDescriptions;
    }

}
