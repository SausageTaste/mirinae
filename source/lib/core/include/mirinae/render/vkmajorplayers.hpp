#pragma once

#include <array>
#include <vector>
#include <string>
#include <optional>

#include <vulkan/vulkan.h>

#include "mirinae/util/image.hpp"
#include "mirinae/util/lightweights.hpp"


namespace mirinae {

    class VulkanExtensionsLayers {

    public:
        void add_validation();

        bool are_layers_available() const;

        std::vector<std::string> extensions_;
        std::vector<std::string> layers_;

    };


    class InstanceFactory {

    public:
        InstanceFactory();

        VkInstance create();

        void enable_validation_layer();

        VulkanExtensionsLayers ext_layers_;

    private:
        VkApplicationInfo app_info{};
        VkInstanceCreateInfo create_info{};
        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
        bool validation_layer_enabled_ = false;

    };


    class SwapChainSupportDetails {

    public:
        void init(VkSurfaceKHR surface, VkPhysicalDevice phys_device);

        bool is_complete() const;
        VkSurfaceFormatKHR choose_format() const;
        VkPresentModeKHR choose_present_mode() const;
        VkExtent2D choose_extent(uint32_t fbuf_width, uint32_t fbuf_height) const;
        uint32_t choose_image_count() const;
        auto get_transform() const { return capabilities_.currentTransform; }

    private:
        VkSurfaceCapabilitiesKHR capabilities_;
        std::vector<VkSurfaceFormatKHR> formats_;
        std::vector<VkPresentModeKHR> present_modes_;

    };


    class PhysDevice {

    public:
        void set(VkPhysicalDevice handle, const VkSurfaceKHR surface);
        void clear();
        VkPhysicalDevice get() { return handle_; }

        std::string make_report_str() const;
        const char* name() const;
        std::optional<uint32_t> graphics_family_index() const;
        std::optional<uint32_t> present_family_index() const;

        bool is_descrete_gpu() const;
        bool is_anisotropic_filtering_supported() const;
        auto max_sampler_anisotropy() const { return properties_.limits.maxSamplerAnisotropy; }

        std::vector<VkExtensionProperties> get_extensions() const;
        size_t count_unsupported_extensions(const std::vector<std::string>& extensions) const;
        VkFormat find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

    private:
        VkPhysicalDevice handle_ = nullptr;
        VkPhysicalDeviceProperties properties_{};
        VkPhysicalDeviceFeatures features_{};
        std::optional<uint32_t> graphics_family_index_;
        std::optional<uint32_t> present_family_index_;

    };


    class LogiDevice {

    public:
        ~LogiDevice() {
            this->destroy();
        }

        void init(PhysDevice& phys_device, const std::vector<std::string>& extensions);
        void destroy();

        void wait_idle();

        VkDevice get() {
            return device_;
        }
        VkQueue graphics_queue() { return graphics_queue_; }
        VkQueue present_queue() { return present_queue_; }

    private:
        VkDevice device_ = nullptr;
        VkQueue graphics_queue_ = nullptr;
        VkQueue present_queue_ = nullptr;

    };


    class Semaphore {

    public:
        void init(LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

        VkSemaphore get() { return handle_; }

    private:
        VkSemaphore handle_ = nullptr;

    };


    class Fence {

    public:
        void init(bool init_signaled, LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

        VkFence get() { return handle_; }

        void wait(LogiDevice& logi_device);
        void reset(LogiDevice& logi_device);

    private:
        VkFence handle_ = nullptr;

    };


    // It stands for Swapchain Image Index
    using ShainImageIndex = mirinae::StrongType<uint32_t, struct SwapchainImageIndexStrongTypeTag>;


    class Swapchain {

    public:
        void init(uint32_t fbuf_width, uint32_t fbuf_height, VkSurfaceKHR surface, PhysDevice& phys_device, LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

        VkSwapchainKHR get() { return swapchain_; }
        VkFormat format() const { return format_; }
        const VkExtent2D& extent() const { return extent_; }

        std::optional<ShainImageIndex> acquire_next_image(Semaphore& img_avaiable_semaphore, LogiDevice& logi_device);

        VkImageView view_at(size_t index) { return views_.at(index); }
        size_t views_count() const { return views_.size(); }

    private:
        VkSwapchainKHR swapchain_ = nullptr;
        std::vector<VkImage> images_;
        std::vector<VkImageView> views_;
        VkFormat format_;
        VkExtent2D extent_;

    };


    class VulkanInstance {

    public:
        ~VulkanInstance() {
            this->destroy();
        }

        void init(InstanceFactory& factory);
        void destroy();

        VkInstance get() const { return instance_; }

        VkPhysicalDevice select_phys_device(const VkSurfaceKHR surface);

    private:
        VkInstance instance_ = nullptr;
        VkDebugUtilsMessengerEXT debug_messenger_ = nullptr;

    };


    class RenderPass {

    public:
        void init(VkFormat swapchain_format, VkFormat depth_format, LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

        VkRenderPass get() { return handle_; }

    private:
        VkRenderPass handle_ = nullptr;

    };


    class Pipeline {

    public:
        Pipeline() = default;
        Pipeline(VkPipeline pipeline, VkPipelineLayout layout);

        void destroy(LogiDevice& logi_device);

        VkPipeline pipeline() { return pipeline_; }
        VkPipelineLayout layout() { return layout_; }

    private:
        VkPipeline pipeline_ = nullptr;
        VkPipelineLayout layout_ = nullptr;

    };


    class Framebuffer {

    public:
        void init(const VkExtent2D& swapchain_extent, VkImageView view, VkImageView depth_view, RenderPass& renderpass, LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

        VkFramebuffer get() { return handle_; }

    private:
        VkFramebuffer handle_ = nullptr;

    };


    class CommandPool {

    public:
        void init(uint32_t graphics_queue, LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

        VkCommandBuffer alloc(LogiDevice& logi_device);
        void free(VkCommandBuffer cmdbuf, LogiDevice& logi_device);

        VkCommandBuffer begin_single_time(LogiDevice& logi_device);
        void end_single_time(VkCommandBuffer cmdbuf, LogiDevice& logi_device);

    private:
        VkCommandPool handle_ = nullptr;

    };


    class VertexStatic {

    public:
        VertexStatic() = default;

        VertexStatic(const glm::vec3& pos, const glm::vec2& texcoord, const glm::vec3& normal)
            : pos_(pos)
            , normal_(normal)
            , texcoord_(texcoord)
        {

        }

        static VkVertexInputBindingDescription make_binding_description() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(VertexStatic);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::vector<VkVertexInputAttributeDescription> make_attribute_descriptions() {
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

        glm::vec3 pos_;
        glm::vec3 normal_;
        glm::vec2 texcoord_;

    };


    class Buffer {

    public:
        void init(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, mirinae::PhysDevice& phys_device, LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

        VkBuffer buffer() { return buffer_; }
        VkDeviceSize size() const { return size_; }

        void set_data(const void* data, size_t size, LogiDevice& logi_device);
        void record_copy_cmd(const Buffer& src, VkCommandBuffer cmdbuf, LogiDevice& logi_device);

    private:
        VkBuffer buffer_ = nullptr;
        VkDeviceMemory memory_ = nullptr;
        VkDeviceSize size_ = 0;

    };


    class TextureImage {

    public:
        void init(
            uint32_t width,
            uint32_t height,
            VkFormat format,
            VkImageTiling tiling,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties,
            mirinae::PhysDevice& phys_device,
            mirinae::LogiDevice& logi_device
        );

        void destroy(LogiDevice& logi_device);

        void copy_and_transition(mirinae::Buffer& staging_buffer, mirinae::CommandPool& cmd_pool, mirinae::LogiDevice& logi_device);

        VkImage image() { return image_; }
        VkDeviceMemory memory() { return memory_; }

        VkFormat format() const { return format_; }

    private:
        VkImage image_ = VK_NULL_HANDLE;
        VkDeviceMemory memory_ = VK_NULL_HANDLE;

        VkFormat format_ = VK_FORMAT_UNDEFINED;
        uint32_t width_ = 0;
        uint32_t height_ = 0;

    };


    class ImageView {

    public:
        void init(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

        VkImageView get() { return handle_; }

    private:
        VkImageView handle_ = VK_NULL_HANDLE;

    };


    class Sampler {

    public:
        void init(PhysDevice& phys_device, LogiDevice& logi_device);
        void destroy(LogiDevice& logi_device);

        VkSampler get() { return handle_; }

    private:
        VkSampler handle_ = VK_NULL_HANDLE;

    };

}
