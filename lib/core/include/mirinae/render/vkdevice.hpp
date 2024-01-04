#pragma once

#include <memory>
#include <optional>

#include "mirinae/util/create_info.hpp"
#include "mirinae/util/lightweights.hpp"

#include "mem_alloc.hpp"


namespace mirinae {

    constexpr static int MAX_FRAMES_IN_FLIGHT = 2;


    VkImageView create_image_view(VkImage image, uint32_t mip_levels, VkFormat format, VkImageAspectFlags aspect_flags, VkDevice device);


    class VulkanDevice {

    public:
        VulkanDevice(mirinae::EngineCreateInfo&& create_info);
        ~VulkanDevice();

        // Logical device
        VkDevice logi_device();
        VkQueue graphics_queue();
        VkQueue present_queue();
        void wait_idle();

        // Physical device
        std::optional<uint32_t> graphics_queue_family_index();
        bool is_anisotropic_filtering_supported() const;
        float max_sampler_anisotropy() const;
        VkFormat find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

        // Misc
        VulkanMemoryAllocator mem_alloc();
        IFilesys& filesys();

        class Pimpl;
        std::unique_ptr<Pimpl> pimpl_;

    };


    // It stands for Swapchain Image Index
    using ShainImageIndex = mirinae::StrongType<uint32_t, struct SwapchainImageIndexStrongTypeTag>;


    class Swapchain {

    public:
        void init(uint32_t fbuf_width, uint32_t fbuf_height, VulkanDevice& vulkan_device);
        void destroy(VkDevice logi_device);

        VkSwapchainKHR get() { return swapchain_; }
        VkFormat format() const { return format_; }
        const VkExtent2D& extent() const { return extent_; }

        std::optional<ShainImageIndex> acquire_next_image(VkSemaphore img_avaiable_semaphore, VkDevice logi_device);

        VkImageView view_at(size_t index) { return views_.at(index); }
        size_t views_count() const { return views_.size(); }

    private:
        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        std::vector<VkImage> images_;
        std::vector<VkImageView> views_;
        VkFormat format_;
        VkExtent2D extent_;

    };

}
