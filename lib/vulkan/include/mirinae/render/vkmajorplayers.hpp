#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "mirinae/lightweight/lightweights.hpp"

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


    class CommandPool {

    public:
        void init(uint32_t graphics_queue, VkDevice logi_device);
        void init(VulkanDevice& device) {
            this->init(
                device.graphics_queue_family_index().value(),
                device.logi_device()
            );
        }

        void destroy(VkDevice logi_device);

        VkCommandPool get() const { return handle_; }

        VkCommandBuffer alloc(VkDevice logi_device);
        void free(VkCommandBuffer cmdbuf, VkDevice logi_device);

        VkCommandBuffer begin_single_time(VkDevice logi_device);
        VkCommandBuffer begin_single_time(VulkanDevice& device) {
            return this->begin_single_time(device.logi_device());
        }
        void end_single_time(
            VkCommandBuffer cmdbuf, VkQueue graphics_q, VkDevice logi_device
        );
        void end_single_time(VkCommandBuffer cmdbuf, VulkanDevice& device) {
            this->end_single_time(
                cmdbuf, device.graphics_queue(), device.logi_device()
            );
        }

    private:
        VkCommandPool handle_ = VK_NULL_HANDLE;
    };


    class ImageViewBuilder {

    public:
        ImageViewBuilder();
        void reset();

        ImageViewBuilder& image(VkImage image);
        ImageViewBuilder& view_type(VkImageViewType view_type);
        ImageViewBuilder& format(VkFormat format);

        ImageViewBuilder& aspect_mask(VkImageAspectFlags aspect_mask);
        ImageViewBuilder& base_mip_level(uint32_t value);
        ImageViewBuilder& mip_levels(uint32_t value);
        ImageViewBuilder& base_arr_layer(uint32_t value);
        ImageViewBuilder& arr_layers(uint32_t value);

        VkImageView build(VulkanDevice& device) const;

    private:
        VkImageViewCreateInfo cinfo_ = {};
    };


    class ImageView {

    public:
        ImageView() = default;
        ImageView(VkImageView handle, VulkanDevice& device);
        ~ImageView();

        ImageView(const ImageView&) = delete;
        ImageView& operator=(const ImageView&) = delete;
        ImageView(ImageView&& other) noexcept;
        ImageView& operator=(ImageView&& other) noexcept;

        bool is_ready() const { return VK_NULL_HANDLE != handle_; }
        VkImageView get() const { return handle_; }

        void reset(VkImageView handle, VulkanDevice& device);
        void reset(const ImageViewBuilder& builder, VulkanDevice& device);
        void destroy(VulkanDevice& device);

    private:
        VkImageView handle_ = VK_NULL_HANDLE;
    };


    class FbufCinfo {

    public:
        FbufCinfo();

        bool is_valid() const;
        const VkFramebufferCreateInfo& get() const;

        FbufCinfo& set_rp(VkRenderPass rp);
        FbufCinfo& set_dim(uint32_t width, uint32_t height);
        FbufCinfo& set_dim(const VkExtent2D& extent);
        FbufCinfo& set_layers(uint32_t layers);

        FbufCinfo& clear_attach();
        FbufCinfo& add_attach(const VkImageView& attachments);
        FbufCinfo& add_attach(const VkImageView* arr, size_t count);
        FbufCinfo& add_attach(const std::vector<VkImageView>& arr);

        template <size_t TSize>
        FbufCinfo& add_attach(const std::array<VkImageView, TSize>& arr) {
            return this->add_attach(arr.data(), arr.size());
        }

        VkFramebuffer build(VulkanDevice& device) const;

    private:
        VkFramebufferCreateInfo cinfo_ = {};
        std::vector<VkImageView> attachments_;
    };


    class Fbuf {

    public:
        Fbuf() = default;
        ~Fbuf();

        void init(const VkFramebufferCreateInfo& cinfo, VkDevice logi_device);
        void reset(VkFramebuffer handle, VkDevice logi_device);
        void destroy(VkDevice logi_device);

        VkFramebuffer get() const { return handle_; }

    private:
        VkFramebuffer handle_ = VK_NULL_HANDLE;
    };

}  // namespace mirinae
