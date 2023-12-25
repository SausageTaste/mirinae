#pragma once

#include <array>
#include <vector>
#include <string>
#include <optional>

#include <vulkan/vulkan.h>

#include "mirinae/util/image.hpp"
#include "mirinae/util/lightweights.hpp"

#include "meshdata.hpp"
#include "vkdevice.hpp"


namespace mirinae {

    class Semaphore {

    public:
        void init(VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkSemaphore get() { return handle_; }

    private:
        VkSemaphore handle_ = VK_NULL_HANDLE;

    };


    class Fence {

    public:
        void init(bool init_signaled, VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkFence get() { return handle_; }

        void wait(VkDevice logi_device);
        void reset(VkDevice logi_device);

    private:
        VkFence handle_ = VK_NULL_HANDLE;

    };


    class RenderPass {

    public:
        void init(VkFormat swapchain_format, VkFormat depth_format, VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkRenderPass get() { return handle_; }

    private:
        VkRenderPass handle_ = VK_NULL_HANDLE;

    };


    class Pipeline {

    public:
        Pipeline() = default;
        Pipeline(VkPipeline pipeline, VkPipelineLayout layout);

        void destroy(VkDevice logi_device);

        VkPipeline pipeline() { return pipeline_; }
        VkPipelineLayout layout() { return layout_; }

    private:
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;

    };


    class Framebuffer {

    public:
        void init(const VkExtent2D& swapchain_extent, VkImageView view, VkImageView depth_view, RenderPass& renderpass, VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkFramebuffer get() { return handle_; }

    private:
        VkFramebuffer handle_ = VK_NULL_HANDLE;

    };


    class CommandPool {

    public:
        void init(uint32_t graphics_queue, VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkCommandBuffer alloc(VkDevice logi_device);
        void free(VkCommandBuffer cmdbuf, VkDevice logi_device);

        VkCommandBuffer begin_single_time(VkDevice logi_device);
        void end_single_time(VkCommandBuffer cmdbuf, VkQueue graphics_q, VkDevice logi_device);

    private:
        VkCommandPool handle_ = VK_NULL_HANDLE;

    };


    class ImageView {

    public:
        void init(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkImageView get() { return handle_; }

    private:
        VkImageView handle_ = VK_NULL_HANDLE;

    };


    class Sampler {

    public:
        void init(bool enable_anisotropy, float max_anisotropy, VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkSampler get() { return handle_; }

    private:
        VkSampler handle_ = VK_NULL_HANDLE;

    };


    VkVertexInputBindingDescription make_vertex_static_binding_description();
    std::vector<VkVertexInputAttributeDescription> make_vertex_static_attribute_descriptions();

    void copy_to_img_and_transition(
        VkImage image,
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkBuffer staging_buffer,
        mirinae::CommandPool& cmd_pool,
        VkQueue graphics_queue,
        VkDevice logi_device
    );

}
