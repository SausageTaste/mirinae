#pragma once

#include <memory>
#include <optional>

#include <imgui_impl_vulkan.h>

#include "mirinae/lightweight/create_info.hpp"
#include "mirinae/lightweight/lightweights.hpp"
#include "mirinae/render/mem_alloc.hpp"


namespace mirinae {

    constexpr static int MAX_FRAMES_IN_FLIGHT = 2;

    using FrameIndex = mirinae::StrongType<int, struct FrameIndexStrongTypeTag>;


    const char* to_str(VkFormat format);
    const char* to_str(VkResult result);
    const char* to_str(VkPresentModeKHR present_mode);
    const char* to_str(VkSurfaceTransformFlagBitsKHR transform_flag);


    struct ISamplerManager {
        virtual ~ISamplerManager() = default;
        virtual VkSampler get_linear() = 0;
        virtual VkSampler get_nearest() = 0;
        virtual VkSampler get_cubemap() = 0;
        virtual VkSampler get_heightmap() = 0;
        virtual VkSampler get_shadow() = 0;
    };


    struct IImageFormats {
        virtual ~IImageFormats() = default;
        virtual VkFormat depth_map() const = 0;
        virtual VkFormat rgb_hdr() const = 0;
    };


    class VulkanDevice {

    public:
        VulkanDevice(mirinae::EngineCreateInfo& create_info);
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
        bool has_supp_depth_clamp() const;

        // Sub-systems
        ISamplerManager& samplers();
        IImageFormats& img_formats();

        // Misc
        VulkanMemoryAllocator mem_alloc();
        dal::Filesystem& filesys();
        IOsIoFunctions& osio();

        void fill_imgui_info(ImGui_ImplVulkan_InitInfo& info);

        class Pimpl;
        std::unique_ptr<Pimpl> pimpl_;
    };


    // It stands for Swapchain Image Index
    using ShainImageIndex =
        mirinae::StrongType<uint32_t, struct SwapchainImageIndexStrongTypeTag>;


    class Swapchain {

    public:
        bool init(VulkanDevice& vulkan_device);
        void destroy(VkDevice logi_device);
        bool is_ready() const;

        auto get() { return swapchain_; }
        auto format() const { return format_; }
        auto width() const { return extent_.width; }
        auto height() const { return extent_.height; }
        auto& extent() const { return extent_; }

        double calc_ratio() const {
            return static_cast<double>(extent_.width) / extent_.height;
        }

        std::optional<ShainImageIndex> acquire_next_image(
            VkSemaphore img_avaiable_semaphore, VkDevice logi_device
        );

        VkImageView view_at(size_t index) { return views_.at(index); }
        uint32_t views_count() const {
            return static_cast<uint32_t>(views_.size());
        }

    private:
        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        std::vector<VkImage> images_;
        std::vector<VkImageView> views_;
        VkFormat format_;
        VkExtent2D extent_;
    };

}  // namespace mirinae
