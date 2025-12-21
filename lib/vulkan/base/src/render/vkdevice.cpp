#include "mirinae/vulkan/base/render/vkdevice.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <sstream>

#include <fmt/ranges.h>
#include <imgui_impl_vulkan.h>
#include <sung/basic/stringtool.hpp>

#include "mirinae/lightweight/konsts.hpp"
#include "mirinae/vulkan/base/platform_func.hpp"
#include "mirinae/vulkan/base/render/cmdbuf.hpp"
#include "mirinae/vulkan/base/render/enum_str.hpp"
#include "mirinae/vulkan/base/render/vkcheck.hpp"
#include "mirinae/vulkan/base/render/vkmajorplayers.hpp"
#include "render/device/phys_device.hpp"
#include "render/device/vk_instance.hpp"


namespace {

    VkSurfaceKHR surface_cast(uint64_t value) {
        static_assert(sizeof(VkSurfaceKHR) == sizeof(uint64_t));
        return *reinterpret_cast<VkSurfaceKHR*>(&value);
    }

    void check_imgui_result(VkResult err) {
        if (err != VK_SUCCESS) {
            SPDLOG_ERROR("ImGui result is error: {}", static_cast<int>(err));
        }
    }

    void select_features(
        VkPhysicalDeviceFeatures& dst, const VkPhysicalDeviceFeatures& src
    ) {
        // Required
        dst.tessellationShader = src.tessellationShader;
        // Optional
        dst.depthClamp = src.depthClamp;
        dst.fillModeNonSolid = src.fillModeNonSolid;
        dst.samplerAnisotropy = src.samplerAnisotropy;
        // KTX
        dst.textureCompressionASTC_LDR = src.textureCompressionASTC_LDR;
        dst.textureCompressionBC = src.textureCompressionBC;
        dst.textureCompressionETC2 = src.textureCompressionETC2;
    }

}  // namespace


namespace {

    class SwapChainSupportDetails {

    public:
        void init(VkSurfaceKHR surface, VkPhysicalDevice phys_device) {
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                phys_device, surface, &caps_
            );

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                phys_device, surface, &formatCount, nullptr
            );
            if (formatCount != 0) {
                formats_.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(
                    phys_device, surface, &formatCount, formats_.data()
                );
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                phys_device, surface, &presentModeCount, nullptr
            );
            if (presentModeCount != 0) {
                present_modes_.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    phys_device,
                    surface,
                    &presentModeCount,
                    present_modes_.data()
                );
            }
        }

        bool is_complete() const {
            if (formats_.empty())
                return false;
            if (present_modes_.empty())
                return false;
            return true;
        }

        VkSurfaceFormatKHR choose_format() const {
            for (const auto& f : formats_) {
                if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    return f;
                }
            }
            return formats_[0];
        }

        VkPresentModeKHR choose_present_mode() const {
            for (const auto& mode : present_modes_) {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    return mode;
                }
            }
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        VkExtent2D choose_extent() const {
            MIRINAE_ASSERT(caps_.currentExtent.width != UINT32_MAX);
            return caps_.currentExtent;
        }

        uint32_t choose_image_count() const {
            auto image_count = caps_.minImageCount + 1;
            if (caps_.maxImageCount > 0 && image_count > caps_.maxImageCount) {
                image_count = caps_.maxImageCount;
            }
            return image_count;
        }

        auto get_transform() const { return caps_.currentTransform; }

    private:
        VkSurfaceCapabilitiesKHR caps_;
        std::vector<VkSurfaceFormatKHR> formats_;
        std::vector<VkPresentModeKHR> present_modes_;
    };


    class LogiDevice {

    public:
        ~LogiDevice() { this->destroy(); }

        void init(
            mirinae::PhysDevice& phys_dev, const std::vector<std::string>& ext
        ) {
            std::set<uint32_t> unique_queue_families{
                phys_dev.graphics_family_index().value(),
                phys_dev.present_family_index().value(),
            };

            float queue_priority = 1;
            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            for (auto queue_fam : unique_queue_families) {
                auto& queueCreateInfo = queueCreateInfos.emplace_back();
                queueCreateInfo.sType =
                    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = queue_fam;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queue_priority;
            }

            features_ = {};
            ::select_features(features_, phys_dev.features());

            const auto char_extension = mirinae::make_char_vec(ext);

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.queueCreateInfoCount = queueCreateInfos.size();
            createInfo.ppEnabledExtensionNames = char_extension.data();
            createInfo.enabledExtensionCount = char_extension.size();
            createInfo.pEnabledFeatures = &features_;

            VK_CHECK(
                vkCreateDevice(phys_dev.get(), &createInfo, nullptr, &device_)
            );

            vkGetDeviceQueue(
                device_,
                phys_dev.graphics_family_index().value(),
                0,
                &graphics_queue_
            );
            vkGetDeviceQueue(
                device_,
                phys_dev.present_family_index().value(),
                0,
                &present_queue_
            );
        }

        void destroy() {
            if (nullptr != device_) {
                vkDeviceWaitIdle(device_);
                vkDestroyDevice(device_, nullptr);
                device_ = nullptr;
            }

            graphics_queue_ = nullptr;
        }

        void wait_idle() {
            if (nullptr != device_) {
                vkDeviceWaitIdle(device_);
            }
        }

        VkDevice get() { return device_; }
        VkQueue graphics_queue() { return graphics_queue_; }
        VkQueue present_queue() { return present_queue_; }
        const VkPhysicalDeviceFeatures& features() const { return features_; }

    private:
        VkDevice device_ = nullptr;
        VkQueue graphics_queue_ = nullptr;
        VkQueue present_queue_ = nullptr;
        VkPhysicalDeviceFeatures features_;
    };


    class ImageFormats : public mirinae::IImageFormats {

    public:
        void init(mirinae::PhysDevice& phys_device) {
            depth_map_ = this->select_depth_map_format(phys_device);
        }

        virtual VkFormat depth_map() const override { return depth_map_; }

        virtual VkFormat rgb_hdr() const override {
            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        }

    private:
        static VkFormat select_depth_map_format(
            mirinae::PhysDevice& phys_device
        ) {
            const std::vector<VkFormat> candidates = {
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT,
            };

            return phys_device.select_first_supported_format(
                candidates,
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            );
        }

        VkFormat depth_map_ = VK_FORMAT_UNDEFINED;
    };

}  // namespace


// Samplers
namespace {

    class SamplerRaii {

    public:
        SamplerRaii(::LogiDevice& logi_d) : logi_d_(logi_d) {}

        ~SamplerRaii() { this->reset(); }

        void reset(VkSampler sampler = VK_NULL_HANDLE) {
            this->destroy();
            handle_ = sampler;
        }

        void destroy() {
            if (VK_NULL_HANDLE != handle_) {
                vkDestroySampler(logi_d_.get(), handle_, nullptr);
                handle_ = VK_NULL_HANDLE;
            }
        }

        VkSampler get() const { return handle_; }

    private:
        ::LogiDevice& logi_d_;
        VkSampler handle_ = VK_NULL_HANDLE;
    };


    class SamplerBuilder {

    public:
        SamplerBuilder() : cinfo_({}) {
            cinfo_.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            cinfo_.magFilter = VK_FILTER_LINEAR;
            cinfo_.minFilter = VK_FILTER_LINEAR;
            cinfo_.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            cinfo_.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            cinfo_.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            cinfo_.anisotropyEnable = VK_FALSE;
            cinfo_.maxAnisotropy = 0;
            cinfo_.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            cinfo_.unnormalizedCoordinates = VK_FALSE;
            cinfo_.compareEnable = VK_FALSE;
            cinfo_.compareOp = VK_COMPARE_OP_ALWAYS;
            cinfo_.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            cinfo_.mipLodBias = 0;
            cinfo_.minLod = 0;
            cinfo_.maxLod = VK_LOD_CLAMP_NONE;
        }

        VkSampler build(const mirinae::PhysDevice& pd, ::LogiDevice& ld) {
            cinfo_.anisotropyEnable = pd.features().samplerAnisotropy;
            cinfo_.maxAnisotropy = pd.max_sampler_anisotropy();

            VkSampler output = VK_NULL_HANDLE;
            const auto result = vkCreateSampler(
                ld.get(), &cinfo_, nullptr, &output
            );

            if (VK_SUCCESS == result)
                return output;
            else
                return VK_NULL_HANDLE;
        }

        SamplerBuilder& mag_filter(VkFilter filter) {
            cinfo_.magFilter = filter;
            return *this;
        }
        SamplerBuilder& mag_filter_nearest() {
            return this->mag_filter(VK_FILTER_NEAREST);
        }
        SamplerBuilder& mag_filter_linear() {
            return this->mag_filter(VK_FILTER_LINEAR);
        }

        SamplerBuilder& min_filter(VkFilter filter) {
            cinfo_.minFilter = filter;
            return *this;
        }
        SamplerBuilder& min_filter_nearest() {
            return this->min_filter(VK_FILTER_NEAREST);
        }
        SamplerBuilder& min_filter_linear() {
            return this->min_filter(VK_FILTER_LINEAR);
        }

        SamplerBuilder& address_mode(VkSamplerAddressMode mode) {
            cinfo_.addressModeU = mode;
            cinfo_.addressModeV = mode;
            cinfo_.addressModeW = mode;
            return *this;
        }
        SamplerBuilder& compare_enable(bool v) {
            cinfo_.compareEnable = v ? VK_TRUE : VK_FALSE;
            return *this;
        }
        SamplerBuilder& compare_op(VkCompareOp op) {
            cinfo_.compareOp = op;
            return *this;
        }
        SamplerBuilder& border_color(VkBorderColor color) {
            cinfo_.borderColor = color;
            return *this;
        }

        SamplerBuilder& mipmap_mode(VkSamplerMipmapMode mode) {
            cinfo_.mipmapMode = mode;
            return *this;
        }

        SamplerBuilder& max_lod(float lod) {
            cinfo_.maxLod = lod;
            return *this;
        }
        SamplerBuilder& min_lod(float lod) {
            cinfo_.minLod = lod;
            return *this;
        }

    private:
        VkSamplerCreateInfo cinfo_;
    };


    class SamplerManager : public mirinae::ISamplerManager {

    public:
        void init(const mirinae::PhysDevice& pd, ::LogiDevice& ld) {
            {
                ::SamplerBuilder sampler_builder;
                data_.push_back(sampler_builder.build(pd, ld));
            }

            {
                ::SamplerBuilder sampler_builder;
                sampler_builder.mag_filter_nearest();
                sampler_builder.min_filter_nearest();
                data_.push_back(sampler_builder.build(pd, ld));
            }

            {
                ::SamplerBuilder sampler_builder;
                sampler_builder
                    .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                    .compare_op(VK_COMPARE_OP_NEVER)
                    .border_color(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);
                data_.push_back(sampler_builder.build(pd, ld));
            }

            {
                ::SamplerBuilder sampler_builder;
                sampler_builder.address_mode(
                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                );
                data_.push_back(sampler_builder.build(pd, ld));
            }

            {
                ::SamplerBuilder sampler_builder;
                sampler_builder
                    .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
                    .border_color(VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK)
                    .compare_enable(true)
                    .compare_op(VK_COMPARE_OP_GREATER_OR_EQUAL)
                    .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
                    .max_lod(0)
                    .min_lod(0);
                data_.push_back(sampler_builder.build(pd, ld));
            }

            {
                ::SamplerBuilder sampler_builder;
                sampler_builder.address_mode(
                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                );
                data_.push_back(sampler_builder.build(pd, ld));
            }
        }

        void destroy(::LogiDevice& ld) {
            for (auto& sampler : data_) {
                if (VK_NULL_HANDLE != sampler) {
                    vkDestroySampler(ld.get(), sampler, nullptr);
                }
            }
            data_.clear();
        }

        VkSampler get_linear() override { return data_[0]; }
        VkSampler get_linear_clamp() override { return data_[5]; }
        VkSampler get_nearest() override { return data_[1]; }
        VkSampler get_cubemap() override { return data_[2]; }
        VkSampler get_heightmap() override { return data_[3]; }
        VkSampler get_shadow() override { return data_[4]; }

    private:
        std::vector<VkSampler> data_;
    };

}  // namespace


// VulkanDevice::Pimpl
namespace mirinae {

    class VulkanDevice::Pimpl {

    public:
        Pimpl(mirinae::EngineCreateInfo& create_info)
            : create_info_(create_info) {
            InstanceFactory instance_factory;
            if (create_info.enable_validation_layers_) {
                instance_factory.enable_validation_layer();
                instance_factory.ext_layers_.add_validation();
            }
            instance_factory.ext_layers_.extensions_.insert(
                instance_factory.ext_layers_.extensions_.end(),
                create_info.instance_extensions_.begin(),
                create_info.instance_extensions_.end()
            );

            SPDLOG_DEBUG(
                "Vulkan instance extensions: {}",
                fmt::join(instance_factory.ext_layers_.extensions_, ", ")
            );

            instance_.init(instance_factory);
            surface_ = create_info.vulkan_os_->create_surface(instance_.get());
            phys_device_.set(instance_.select_phys_device(surface_), surface_);
            SPDLOG_INFO(
                "Physical device selected: {}\n{}",
                phys_device_.name(),
                phys_device_.make_report_str()
            );

            std::vector<std::string> device_extensions;
            device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            if (phys_device_.count_unsupported_extensions(device_extensions))
                MIRINAE_ABORT("Some extensions are not supported");

            logi_device_.init(phys_device_, device_extensions);
            mem_allocator_ = mirinae::create_vma_allocator(
                instance_.get(), phys_device_.get(), logi_device_.get()
            );

            mirinae::DebugLabel::load_funcs(logi_device_.get());
            mirinae::DebugAnnoName::load_funcs(logi_device_.get());

            ::SwapChainSupportDetails swapchain_details;
            swapchain_details.init(surface_, phys_device_.get());
            if (!swapchain_details.is_complete())
                MIRINAE_ABORT("The swapchain is not complete");

            samplers_.init(phys_device_, logi_device_);
            img_formats_.init(phys_device_);
        }

        ~Pimpl() {
            samplers_.destroy(logi_device_);

            mirinae::destroy_vma_allocator(mem_allocator_);
            mem_allocator_ = nullptr;

            logi_device_.destroy();

            vkDestroySurfaceKHR(instance_.get(), surface_, nullptr);
            surface_ = VK_NULL_HANDLE;
        }

        void fill_imgui_info(ImGui_ImplVulkan_InitInfo& info) {
            info.Instance = instance_.get();
            info.PhysicalDevice = phys_device_.get();
            info.Device = logi_device_.get();
            info.QueueFamily = phys_device_.graphics_family_index().value();
            info.Queue = logi_device_.graphics_queue();
            info.PipelineCache = VK_NULL_HANDLE;
            info.Allocator = VK_NULL_HANDLE;
            info.CheckVkResultFn = ::check_imgui_result;
        }

        mirinae::VulkanInstance instance_;
        mirinae::PhysDevice phys_device_;
        ::LogiDevice logi_device_;
        ::SamplerManager samplers_;
        ::ImageFormats img_formats_;
        EngineCreateInfo create_info_;
        VulkanMemoryAllocator mem_allocator_;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    };

}  // namespace mirinae


// VulkanDevice
namespace mirinae {

    VulkanDevice::VulkanDevice(mirinae::EngineCreateInfo& create_info)
        : pimpl_(std::make_unique<Pimpl>(create_info)) {}

    VulkanDevice::~VulkanDevice() {}

    VkDevice VulkanDevice::logi_device() { return pimpl_->logi_device_.get(); }

    VkQueue VulkanDevice::graphics_queue() {
        return pimpl_->logi_device_.graphics_queue();
    }

    VkQueue VulkanDevice::present_queue() {
        return pimpl_->logi_device_.present_queue();
    }

    const VkPhysicalDeviceFeatures& VulkanDevice::features() const {
        return pimpl_->logi_device_.features();
    }

    void VulkanDevice::wait_idle() { pimpl_->logi_device_.wait_idle(); }

    VkPhysicalDevice VulkanDevice::phys_device() {
        return pimpl_->phys_device_.get();
    }

    std::optional<uint32_t> VulkanDevice::graphics_queue_family_index() {
        return pimpl_->phys_device_.graphics_family_index();
    }

    const VkPhysicalDeviceLimits& VulkanDevice::limits() const {
        return pimpl_->phys_device_.limits();
    }

    size_t VulkanDevice::pad_ubuf(size_t original) const {
        return align_up(
            original, this->limits().minUniformBufferOffsetAlignment
        );
    }

    ISamplerManager& VulkanDevice::samplers() { return pimpl_->samplers_; }

    IImageFormats& VulkanDevice::img_formats() { return pimpl_->img_formats_; }

    VulkanMemoryAllocator VulkanDevice::mem_alloc() {
        return pimpl_->mem_allocator_;
    }

    dal::Filesystem& VulkanDevice::filesys() {
        return *pimpl_->create_info_.filesys_;
    }

    IOsIoFunctions& VulkanDevice::osio() { return *pimpl_->create_info_.osio_; }

    void VulkanDevice::fill_imgui_info(ImGui_ImplVulkan_InitInfo& info) {
        pimpl_->fill_imgui_info(info);
    }

}  // namespace mirinae


// Swapchain
namespace mirinae {

    bool Swapchain::init(VulkanDevice& vulkan_device) {
        auto logi_device = vulkan_device.logi_device();
        this->destroy(logi_device);

        ::SwapChainSupportDetails s_details;
        s_details.init(
            vulkan_device.pimpl_->surface_,
            vulkan_device.pimpl_->phys_device_.get()
        );

        const auto extent = s_details.choose_extent();
        if (extent.width == 0 || extent.height == 0)
            return false;

        std::array<uint32_t, 2> queue_family_indices{
            vulkan_device.pimpl_->phys_device_.graphics_family_index().value(),
            vulkan_device.pimpl_->phys_device_.present_family_index().value()
        };
        VkSwapchainCreateInfoKHR cinfo{};

        // Fill in swapchain create info
        {
            cinfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            cinfo.surface = vulkan_device.pimpl_->surface_;
            cinfo.minImageCount = s_details.choose_image_count();
            cinfo.imageFormat = s_details.choose_format().format;
            cinfo.imageColorSpace = s_details.choose_format().colorSpace;
            cinfo.imageExtent = s_details.choose_extent();
            cinfo.imageArrayLayers = 1;
            cinfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            cinfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            cinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            cinfo.presentMode = s_details.choose_present_mode();
            cinfo.clipped = VK_TRUE;
            cinfo.oldSwapchain = VK_NULL_HANDLE;

            if (queue_family_indices[0] != queue_family_indices[1]) {
                cinfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                cinfo.queueFamilyIndexCount = queue_family_indices.size();
                cinfo.pQueueFamilyIndices = queue_family_indices.data();
            } else {
                cinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                cinfo.queueFamilyIndexCount = 0;      // Optional
                cinfo.pQueueFamilyIndices = nullptr;  // Optional
            }
        }

        if (VK_SUCCESS !=
            vkCreateSwapchainKHR(logi_device, &cinfo, NULL, &swapchain_)) {
            SPDLOG_ERROR("Failed to create swapchain!");
            return false;
        }

        // Store some data
        {
            format_ = cinfo.imageFormat;
            extent_ = cinfo.imageExtent;

            uint32_t image_count = s_details.choose_image_count();
            vkGetSwapchainImagesKHR(
                logi_device, swapchain_, &image_count, nullptr
            );
            images_.resize(image_count);
            vkGetSwapchainImagesKHR(
                logi_device, swapchain_, &image_count, images_.data()
            );
        }

        SPDLOG_INFO(
            "Swapchain created: {}*{}, {}, {} images, {}, {}",
            extent_.width,
            extent_.height,
            sung::lstrip(to_str(format_), "VK_FORMAT_"),
            images_.size(),
            to_str(cinfo.presentMode),
            to_str(cinfo.preTransform)
        );

        // Create views
        ImageViewBuilder iv_builder;
        iv_builder.format(format_);
        for (size_t i = 0; i < images_.size(); i++) {
            iv_builder.image(images_.at(i));
            views_.push_back(iv_builder.build(vulkan_device));
        }

        return true;
    }

    void Swapchain::destroy(VkDevice logi_device) {
        for (auto view : views_) vkDestroyImageView(logi_device, view, nullptr);
        views_.clear();
        images_.clear();
        format_ = VK_FORMAT_UNDEFINED;
        extent_ = { 0, 0 };

        if (VK_NULL_HANDLE != swapchain_) {
            vkDestroySwapchainKHR(logi_device, swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }
    }

    bool Swapchain::is_ready() const {
        if (VK_NULL_HANDLE == swapchain_)
            return false;
        if (images_.empty())
            return false;
        if (views_.empty())
            return false;
        return true;
    }

    std::optional<ShainImageIndex> Swapchain::acquire_next_image(
        VkSemaphore img_avaiable_semaphore, VkDevice logi_device
    ) {
        uint32_t imageIndex;
        const auto result = vkAcquireNextImageKHR(
            logi_device,
            swapchain_,
            UINT64_MAX,
            img_avaiable_semaphore,
            VK_NULL_HANDLE,
            &imageIndex
        );

        switch (result) {
            case VK_SUCCESS:
                return ShainImageIndex{ imageIndex };
            case VK_SUBOPTIMAL_KHR:
                SPDLOG_INFO("Swapchain suboptimal");
                return std::nullopt;
            case VK_ERROR_OUT_OF_DATE_KHR:
                SPDLOG_INFO("Swapchain out of date");
                return std::nullopt;
            default:
                SPDLOG_ERROR(
                    "Failed to acquire next image: {}", static_cast<int>(result)
                );
                return std::nullopt;
        }
    }

}  // namespace mirinae


// Free functions
namespace mirinae {

    // https://vkguide.dev/docs/chapter-4/descriptors_code_more/
    size_t align_up(size_t original_size, size_t min_alignment) {
        // Calculate required alignment based on minimum device offset alignment
        size_t aligned_size = original_size;
        if (min_alignment > 0) {
            aligned_size = (aligned_size + min_alignment - 1) &
                           ~(min_alignment - 1);
        }
        return aligned_size;
    }

}  // namespace mirinae
