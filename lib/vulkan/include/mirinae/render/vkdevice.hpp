#pragma once

#include <memory>
#include <optional>

#include "mirinae/lightweight/create_info.hpp"
#include "mirinae/lightweight/lightweights.hpp"

#include "mem_alloc.hpp"


namespace mirinae {

    constexpr static int MAX_FRAMES_IN_FLIGHT = 2;


    VkImageView create_image_view(
        VkImage image,
        VkImageViewType view_type,
        uint32_t mip_levels,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkDevice device
    );


    struct ISamplerManager {
        virtual ~ISamplerManager() = default;
        virtual VkSampler get_linear() = 0;
        virtual VkSampler get_nearest() = 0;
        virtual VkSampler get_cubemap() = 0;
    };


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
        VkPhysicalDevice phys_device();
        const VkPhysicalDeviceFeatures& phys_device_features() const;
        std::optional<uint32_t> graphics_queue_family_index();
        VkFormat select_first_supported_format(
            const std::vector<VkFormat>& candidates,
            VkImageTiling tiling,
            VkFormatFeatureFlags features
        ) const;
        bool has_supp_depth_clamp() const;

        // Sub-systems
        ISamplerManager& samplers();

        // Misc
        VulkanMemoryAllocator mem_alloc();
        dal::Filesystem& filesys();
        IOsIoFunctions& osio();

        class Pimpl;
        std::unique_ptr<Pimpl> pimpl_;
    };


    // It stands for Swapchain Image Index
    using ShainImageIndex =
        mirinae::StrongType<uint32_t, struct SwapchainImageIndexStrongTypeTag>;


    class Swapchain {

    public:
        void init(
            uint32_t fbuf_width,
            uint32_t fbuf_height,
            VulkanDevice& vulkan_device
        );
        void destroy(VkDevice logi_device);

        auto get() { return swapchain_; }
        auto format() const { return format_; }
        auto width() const { return extent_.width; }
        auto height() const { return extent_.height; }
        auto& extent() const { return extent_; }

        std::optional<ShainImageIndex> acquire_next_image(
            VkSemaphore img_avaiable_semaphore, VkDevice logi_device
        );

        VkImageView view_at(size_t index) { return views_.at(index); }
        size_t views_count() const { return views_.size(); }

    private:
        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        std::vector<VkImage> images_;
        std::vector<VkImageView> views_;
        VkFormat format_;
        VkExtent2D extent_;
    };

}  // namespace mirinae
