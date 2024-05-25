#include "mirinae/render/renderee.hpp"

#include <spdlog/spdlog.h>

#include <daltools/model_parser.h>


namespace {

    void calc_tangents(
        mirinae::VertexStatic& p0,
        mirinae::VertexStatic& p1,
        mirinae::VertexStatic& p2
    ) {
        glm::vec3 edge1 = p1.pos_ - p0.pos_;
        glm::vec3 edge2 = p2.pos_ - p0.pos_;
        glm::vec2 delta_uv1 = p1.texcoord_ - p0.texcoord_;
        glm::vec2 delta_uv2 = p2.texcoord_ - p0.texcoord_;
        const auto deno = delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y;
        if (0 == deno)
            return;

        const auto f = 1 / deno;
        glm::vec3 tangent;
        tangent.x = f * (delta_uv2.y * edge1.x - delta_uv1.y * edge2.x);
        tangent.y = f * (delta_uv2.y * edge1.y - delta_uv1.y * edge2.y);
        tangent.z = f * (delta_uv2.y * edge1.z - delta_uv1.y * edge2.z);
        tangent = glm::normalize(tangent);

        p0.tangent_ = tangent;
        p1.tangent_ = tangent;
        p2.tangent_ = tangent;
    }

    void calc_tangents(
        mirinae::VertexSkinned& p0,
        mirinae::VertexSkinned& p1,
        mirinae::VertexSkinned& p2
    ) {
        glm::vec3 edge1 = p1.pos_ - p0.pos_;
        glm::vec3 edge2 = p2.pos_ - p0.pos_;
        glm::vec2 delta_uv1 = p1.uv_ - p0.uv_;
        glm::vec2 delta_uv2 = p2.uv_ - p0.uv_;
        const auto deno = delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y;
        if (0 == deno)
            return;

        const auto f = 1 / deno;
        glm::vec3 tangent;
        tangent.x = f * (delta_uv2.y * edge1.x - delta_uv1.y * edge2.x);
        tangent.y = f * (delta_uv2.y * edge1.y - delta_uv1.y * edge2.y);
        tangent.z = f * (delta_uv2.y * edge1.z - delta_uv1.y * edge2.z);
        tangent = glm::normalize(tangent);

        p0.tangent_ = tangent;
        p1.tangent_ = tangent;
        p2.tangent_ = tangent;
    }


    class MaterialResources {

    public:
        void fetch(
            const mirinae::respath_t& res_id,
            const dal::parser::Material& src_material,
            mirinae::TextureManager& tex_man
        ) {
            albedo_map_ = this->request_texture(
                res_id,
                src_material.albedo_map_,
                "asset/textures/missing_texture.png",
                true,
                tex_man
            );

            normal_map_ = this->request_texture(
                res_id,
                src_material.normal_map_,
                "asset/textures/null_normal_map.png",
                false,
                tex_man
            );

            model_ubuf_.roughness = src_material.roughness_;
            model_ubuf_.metallic = src_material.metallic_;
        }

        mirinae::U_GbufModel model_ubuf_;
        std::shared_ptr<mirinae::ITexture> albedo_map_;
        std::shared_ptr<mirinae::ITexture> normal_map_;

    private:
        static std::shared_ptr<mirinae::ITexture> request_texture(
            const mirinae::respath_t& res_id,
            const std::string& file_name,
            const std::string& fallback_path,
            const bool srgb,
            mirinae::TextureManager& tex_man
        ) {
            if (file_name.empty())
                return tex_man.request(fallback_path, srgb);

            const auto full_path = mirinae::replace_file_name_ext(
                res_id, file_name
            );

            auto output = tex_man.request(full_path, srgb);
            if (!output)
                return tex_man.request(fallback_path, srgb);

            return output;
        }
    };


    auto interpret_fbuf_usage(const mirinae::FbufUsage usage) {
        VkImageAspectFlags aspect_mask = 0;
        VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageUsageFlags flag = 0;

        switch (usage) {
            case mirinae::FbufUsage::color_attachment:
                aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
                image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                flag = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
                break;

            case mirinae::FbufUsage::depth_map:
                aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
                image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                flag = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
                break;

            case mirinae::FbufUsage::depth_stencil_attachment:
                aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT |
                              VK_IMAGE_ASPECT_STENCIL_BIT;
                image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                flag = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
                break;

            case mirinae::FbufUsage::depth_attachment:
                aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
                image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                flag = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
                break;

            default:
                throw std::runtime_error(
                    fmt::format("unsupported framebuffer usage: {}", (int)usage)
                );
        }

        return std::make_tuple(flag, aspect_mask, image_layout);
    }

    uint32_t find_memory_type(
        uint32_t typeFilter,
        VkMemoryPropertyFlags properties,
        VkPhysicalDevice phys_device
    ) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(phys_device, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if (0 == typeFilter & (1 << i))
                continue;

            const auto& mem_type = memProperties.memoryTypes[i];
            if ((mem_type.propertyFlags & properties) != properties)
                continue;

            return i;
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    bool create_vk_image(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory,
        VkPhysicalDevice phys_device,
        VkDevice logi_device
    ) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(logi_device, &imageInfo, nullptr, &image) !=
            VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(logi_device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = ::find_memory_type(
            memRequirements.memoryTypeBits, properties, phys_device
        );

        if (vkAllocateMemory(logi_device, &allocInfo, nullptr, &imageMemory) !=
            VK_SUCCESS) {
            return false;
        }

        vkBindImageMemory(logi_device, image, imageMemory, 0);
        return true;
    }

    void transition_image_layout(
        const VkImage image,
        const uint32_t mip_levels,
        const VkFormat format,
        const VkImageLayout old_layout,
        const VkImageLayout new_layout,
        mirinae::CommandPool& cmd_pool,
        VkQueue graphics_queue,
        VkDevice logi_device
    ) {
        auto cmd_buf = cmd_pool.begin_single_time(logi_device);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.srcAccessMask = 0;  // TODO
        barrier.dstAccessMask = 0;  // TODO
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mip_levels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags src_stage;
        VkPipelineStageFlags dst_stage;
        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
            new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                   new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            spdlog::error("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            cmd_buf,
            src_stage,
            dst_stage,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );

        cmd_pool.end_single_time(cmd_buf, graphics_queue, logi_device);
    }

    void copy_buffer_to_image(
        const VkImage dst_image,
        const VkBuffer src_buffer,
        const uint32_t width,
        const uint32_t height,
        const uint32_t mip_level,
        mirinae::CommandPool& cmd_pool,
        VkQueue graphics_queue,
        VkDevice logi_device
    ) {
        auto cmd_buf = cmd_pool.begin_single_time(logi_device);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = mip_level;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(
            cmd_buf,
            src_buffer,
            dst_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        cmd_pool.end_single_time(cmd_buf, graphics_queue, logi_device);
    }

    void generate_mipmaps(
        VkImage image,
        uint32_t width,
        uint32_t height,
        uint32_t mip_levels,
        mirinae::CommandPool& cmd_pool,
        VkQueue graphics_queue,
        VkDevice logi_device
    ) {
        auto cmd_buf = cmd_pool.begin_single_time(logi_device);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = width;
        int32_t mipHeight = height;
        for (uint32_t i = 1; i < mip_levels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(
                cmd_buf,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier
            );

            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1,
                                   mipHeight > 1 ? mipHeight / 2 : 1,
                                   1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(
                cmd_buf,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &blit,
                VK_FILTER_LINEAR
            );

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                cmd_buf,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier
            );

            if (mipWidth > 1)
                mipWidth /= 2;
            if (mipHeight > 1)
                mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mip_levels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd_buf,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );

        cmd_pool.end_single_time(cmd_buf, graphics_queue, logi_device);
    }

    void copy_to_img_and_transition(
        VkImage image,
        uint32_t width,
        uint32_t height,
        uint32_t mip_levels,
        VkFormat format,
        VkBuffer staging_buffer,
        mirinae::CommandPool& cmd_pool,
        VkQueue graphics_queue,
        VkDevice logi_device
    ) {
        ::transition_image_layout(
            image,
            mip_levels,
            format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            cmd_pool,
            graphics_queue,
            logi_device
        );

        ::copy_buffer_to_image(
            image,
            staging_buffer,
            width,
            height,
            0,
            cmd_pool,
            graphics_queue,
            logi_device
        );

        ::generate_mipmaps(
            image,
            width,
            height,
            mip_levels,
            cmd_pool,
            graphics_queue,
            logi_device
        );
    }


    class ImageView {

    public:
        void init(
            VkImage image,
            uint32_t mip_levels,
            VkFormat format,
            VkImageAspectFlags aspect_flags,
            VkDevice logi_device
        ) {
            this->destroy(logi_device);
            this->handle_ = mirinae::create_image_view(
                image, mip_levels, format, aspect_flags, logi_device
            );
        }

        void destroy(VkDevice logi_device) {
            if (VK_NULL_HANDLE != handle_) {
                vkDestroyImageView(logi_device, handle_, nullptr);
                handle_ = VK_NULL_HANDLE;
            }
        }

        VkImageView get() { return handle_; }

    private:
        VkImageView handle_ = VK_NULL_HANDLE;
    };

}  // namespace


// TextureManager
namespace mirinae {

    class TextureData : public ITexture {

    public:
        TextureData(VulkanDevice& device) : device_(device) {}

        ~TextureData() { this->destroy(); }

        void init_iimage2d(
            const std::string& id,
            const IImage2D& image,
            bool srgb,
            CommandPool& cmd_pool
        ) {
            id_ = id;

            Buffer staging_buffer;
            staging_buffer.init_staging(image.data_size(), device_.mem_alloc());
            staging_buffer.set_data(
                image.data(), image.data_size(), device_.mem_alloc()
            );

            mirinae::ImageCreateInfo img_info;
            img_info.fetch_from_image(image, srgb)
                .deduce_mip_levels()
                .set_usage(
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                );
            texture_.init(img_info.get(), device_.mem_alloc());

            ::copy_to_img_and_transition(
                texture_.image(),
                texture_.width(),
                texture_.height(),
                texture_.mip_levels(),
                texture_.format(),
                staging_buffer.buffer(),
                cmd_pool,
                device_.graphics_queue(),
                device_.logi_device()
            );
            staging_buffer.destroy(device_.mem_alloc());

            texture_view_.init(
                texture_.image(),
                texture_.mip_levels(),
                texture_.format(),
                VK_IMAGE_ASPECT_COLOR_BIT,
                device_.logi_device()
            );
        }

        void init_depth(uint32_t width, uint32_t height) {
            id_ = "<depth>";

            const auto depth_format = device_.select_first_supported_format(
                { VK_FORMAT_D32_SFLOAT,
                  VK_FORMAT_D32_SFLOAT_S8_UINT,
                  VK_FORMAT_D24_UNORM_S8_UINT },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            );

            mirinae::ImageCreateInfo img_info;
            img_info.set_dimensions(width, height)
                .set_format(depth_format)
                .set_usage(
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT
                );
            texture_.init(img_info.get(), device_.mem_alloc());
            texture_view_.init(
                texture_.image(),
                1,
                depth_format,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                device_.logi_device()
            );
        }

        void init_attachment(
            uint32_t width,
            uint32_t height,
            VkFormat format,
            FbufUsage usage,
            const char* name
        ) {
            id_ = name;

            const auto [usage_flag, aspect_mask, image_layout] =
                ::interpret_fbuf_usage(usage);
            mirinae::ImageCreateInfo img_info;
            img_info.set_dimensions(width, height)
                .set_format(format)
                .set_usage(usage_flag);
            texture_.init(img_info.get(), device_.mem_alloc());
            texture_view_.init(
                texture_.image(), 1, format, aspect_mask, device_.logi_device()
            );
        }

        void destroy() {
            texture_view_.destroy(device_.logi_device());
            texture_.destroy(device_.mem_alloc());
        }

        VkFormat format() const override { return texture_.format(); }

        VkImageView image_view() override { return texture_view_.get(); }

        uint32_t width() const override { return texture_.width(); }
        uint32_t height() const override { return texture_.height(); }

        auto& id() const { return id_; }

    private:
        VulkanDevice& device_;
        Image texture_;
        ::ImageView texture_view_;
        std::string id_;
    };


    class TextureManager::Pimpl {

    public:
        Pimpl(VulkanDevice& device) : device_(device) {
            cmd_pool_.init(
                device_.graphics_queue_family_index().value(),
                device_.logi_device()
            );
        }

        ~Pimpl() {
            this->destroy_all();
            cmd_pool_.destroy(device_.logi_device());
        }

        std::shared_ptr<TextureData> request(
            const respath_t& res_id, bool srgb
        ) {
            if (auto index = this->find_index(res_id))
                return textures_.at(index.value());

            const auto img_data = device_.filesys().read_file_to_vector(
                res_id.c_str()
            );
            if (!img_data.has_value()) {
                spdlog::warn(
                    "Failed to read image file: {}", res_id.u8string()
                );
                return nullptr;
            }

            const auto image = mirinae::parse_image(
                img_data->data(), img_data->size(), true
            );
            auto& output = textures_.emplace_back(new TextureData{ device_ });
            output->init_iimage2d(res_id.u8string(), *image, srgb, cmd_pool_);
            return output;
        }

        std::unique_ptr<ITexture> create_image(
            const std::string& id, const IImage2D& image, bool srgb
        ) {
            auto output = std::make_unique<TextureData>(device_);
            output->init_iimage2d(id, image, srgb, cmd_pool_);
            return output;
        }

        std::unique_ptr<ITexture> create_depth(
            uint32_t width, uint32_t height
        ) {
            auto output = std::make_unique<TextureData>(device_);
            output->init_depth(width, height);
            return output;
        }

        std::unique_ptr<ITexture> create_attachment(
            uint32_t width,
            uint32_t height,
            VkFormat format,
            FbufUsage usage,
            const char* name
        ) {
            auto output = std::make_unique<TextureData>(device_);
            output->init_attachment(width, height, format, usage, name);
            return output;
        }

    private:
        std::optional<size_t> find_index(const respath_t& id) {
            for (size_t i = 0; i < textures_.size(); ++i) {
                if (textures_.at(i)->id() == id.u8string())
                    return i;
            }
            return std::nullopt;
        }

        void destroy_all() {
            for (auto& tex : textures_) {
                if (tex.use_count() > 1)
                    spdlog::warn(
                        "Want to destroy texture '{}' is still in use",
                        tex->id()
                    );
                tex->destroy();
            }
            textures_.clear();
        }

        VulkanDevice& device_;
        CommandPool cmd_pool_;
        std::vector<std::shared_ptr<TextureData>> textures_;
    };


    TextureManager::TextureManager(VulkanDevice& device)
        : pimpl_(std::make_unique<Pimpl>(device)) {}

    TextureManager::~TextureManager() {}

    std::shared_ptr<ITexture> TextureManager::request(
        const respath_t& res_id, bool srgb
    ) {
        return pimpl_->request(res_id, srgb);
    }

    std::unique_ptr<ITexture> TextureManager::create_image(
        const std::string& id, const IImage2D& image, bool srgb
    ) {
        return pimpl_->create_image(id, image, srgb);
    }

    std::unique_ptr<ITexture> TextureManager::create_depth(
        uint32_t width, uint32_t height
    ) {
        return pimpl_->create_depth(width, height);
    }

    std::unique_ptr<ITexture> TextureManager::create_attachment(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        FbufUsage usage,
        const char* name
    ) {
        return pimpl_->create_attachment(width, height, format, usage, name);
    }

}  // namespace mirinae


// RenderUnit
namespace mirinae {

    void RenderUnit::init(
        uint32_t max_flight_count,
        const VerticesStaticPair& vertices,
        const U_GbufModel& ubuf_data,
        VkImageView albedo_map,
        VkImageView normal_map,
        VkSampler texture_sampler,
        CommandPool& cmd_pool,
        DesclayoutManager& desclayouts,
        VulkanDevice& vulkan_device
    ) {
        desc_pool_.init(max_flight_count * 4, vulkan_device.logi_device());
        desc_sets_ = desc_pool_.alloc(
            max_flight_count,
            desclayouts.get("gbuf:model"),
            vulkan_device.logi_device()
        );

        uniform_buf_.init_ubuf(sizeof(U_GbufModel), vulkan_device.mem_alloc());
        uniform_buf_.set_data(
            &ubuf_data, sizeof(U_GbufModel), vulkan_device.mem_alloc()
        );

        for (size_t i = 0; i < max_flight_count; i++) {
            DescWriteInfoBuilder builder;
            builder.add_uniform_buffer(uniform_buf_, desc_sets_.at(i))
                .add_combinded_image_sampler(
                    albedo_map, texture_sampler, desc_sets_.at(i)
                )
                .add_combinded_image_sampler(
                    normal_map, texture_sampler, desc_sets_.at(i)
                )
                .apply_all(vulkan_device.logi_device());
        }

        vert_index_pair_.init(
            vertices,
            cmd_pool,
            vulkan_device.mem_alloc(),
            vulkan_device.graphics_queue(),
            vulkan_device.logi_device()
        );
    }

    void RenderUnit::destroy(
        VulkanMemoryAllocator mem_alloc, VkDevice logi_device
    ) {
        vert_index_pair_.destroy(mem_alloc);
        uniform_buf_.destroy(mem_alloc);
        desc_pool_.destroy(logi_device);
    }

    VkDescriptorSet RenderUnit::get_desc_set(size_t index) {
        return desc_sets_.at(index);
    }

    void RenderUnit::record_bind_vert_buf(VkCommandBuffer cmdbuf) {
        vert_index_pair_.record_bind(cmdbuf);
    }

    uint32_t RenderUnit::vertex_count() const {
        return vert_index_pair_.vertex_count();
    }

}  // namespace mirinae


// RenderUnitSkinned
namespace mirinae {

    void RenderUnitSkinned::init(
        uint32_t max_flight_count,
        const VerticesSkinnedPair& vertices,
        const U_GbufModel& ubuf_data,
        VkImageView albedo_map,
        VkImageView normal_map,
        VkSampler texture_sampler,
        CommandPool& cmd_pool,
        DesclayoutManager& desclayouts,
        VulkanDevice& vulkan_device
    ) {
        desc_pool_.init(max_flight_count * 4, vulkan_device.logi_device());
        desc_sets_ = desc_pool_.alloc(
            max_flight_count,
            desclayouts.get("gbuf:model"),
            vulkan_device.logi_device()
        );

        uniform_buf_.init_ubuf(sizeof(U_GbufModel), vulkan_device.mem_alloc());
        uniform_buf_.set_data(
            &ubuf_data, sizeof(U_GbufModel), vulkan_device.mem_alloc()
        );

        for (size_t i = 0; i < max_flight_count; i++) {
            DescWriteInfoBuilder builder;
            builder.add_uniform_buffer(uniform_buf_, desc_sets_.at(i))
                .add_combinded_image_sampler(
                    albedo_map, texture_sampler, desc_sets_.at(i)
                )
                .add_combinded_image_sampler(
                    normal_map, texture_sampler, desc_sets_.at(i)
                )
                .apply_all(vulkan_device.logi_device());
        }

        vert_index_pair_.init(
            vertices,
            cmd_pool,
            vulkan_device.mem_alloc(),
            vulkan_device.graphics_queue(),
            vulkan_device.logi_device()
        );
    }

    void RenderUnitSkinned::destroy(
        VulkanMemoryAllocator mem_alloc, VkDevice logi_device
    ) {
        vert_index_pair_.destroy(mem_alloc);
        uniform_buf_.destroy(mem_alloc);
        desc_pool_.destroy(logi_device);
    }

    VkDescriptorSet RenderUnitSkinned::get_desc_set(size_t index) {
        return desc_sets_.at(index);
    }

    void RenderUnitSkinned::record_bind_vert_buf(VkCommandBuffer cmdbuf) {
        vert_index_pair_.record_bind(cmdbuf);
    }

    uint32_t RenderUnitSkinned::vertex_count() const {
        return vert_index_pair_.vertex_count();
    }

}  // namespace mirinae


// OverlayRenderUnit
namespace mirinae {

    OverlayRenderUnit::OverlayRenderUnit(VulkanDevice& device)
        : device_(device) {}

    OverlayRenderUnit::~OverlayRenderUnit() { this->destroy(); }

    void OverlayRenderUnit::init(
        uint32_t max_flight_count,
        VkImageView color_view,
        VkImageView mask_view,
        VkSampler sampler,
        DesclayoutManager& desclayouts,
        TextureManager& tex_man
    ) {
        desc_pool_.init(max_flight_count * 2, device_.logi_device());
        desc_sets_ = desc_pool_.alloc(
            max_flight_count,
            desclayouts.get("overlay:main"),
            device_.logi_device()
        );

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& ubuf = uniform_buf_.emplace_back();
            ubuf.init_ubuf(sizeof(U_OverlayMain), device_.mem_alloc());
        }

        for (size_t i = 0; i < max_flight_count; i++) {
            DescWriteInfoBuilder builder;
            builder.add_uniform_buffer(uniform_buf_.at(i), desc_sets_.at(i))
                .add_combinded_image_sampler(
                    color_view, sampler, desc_sets_.at(i)
                )
                .add_combinded_image_sampler(
                    mask_view, sampler, desc_sets_.at(i)
                )
                .apply_all(device_.logi_device());
        }
    }

    void OverlayRenderUnit::destroy() {
        for (auto& ubuf : uniform_buf_) ubuf.destroy(device_.mem_alloc());
        uniform_buf_.clear();

        desc_pool_.destroy(device_.logi_device());
    }

    void OverlayRenderUnit::udpate_ubuf(uint32_t index) {
        auto& ubuf = uniform_buf_.at(index);
        ubuf.set_data(&ubuf_data_, sizeof(U_OverlayMain), device_.mem_alloc());
    }

    VkDescriptorSet OverlayRenderUnit::get_desc_set(size_t index) {
        return desc_sets_.at(index);
    }

}  // namespace mirinae


// RenderModel
namespace mirinae {

    RenderModel::~RenderModel() {
        for (auto& unit : render_units_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        render_units_.clear();

        for (auto& unit : render_units_alpha_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        render_units_alpha_.clear();
    }

}  // namespace mirinae


// RenderModelSkinned
namespace mirinae {

    RenderModelSkinned::~RenderModelSkinned() {
        for (auto& unit : runits_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        runits_.clear();

        for (auto& unit : runits_alpha_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        runits_alpha_.clear();
    }

}  // namespace mirinae


// ModelManager
namespace mirinae {

    class ModelManager::Pimpl {

    public:
        Pimpl(VulkanDevice& device)
            : device_(device), texture_sampler_(device) {
            SamplerBuilder sampler_builder;
            texture_sampler_.reset(sampler_builder.build(device_));
            cmd_pool_.init(
                device_.graphics_queue_family_index().value(),
                device_.logi_device()
            );
        }

        ~Pimpl() { cmd_pool_.destroy(device_.logi_device()); }

        std::shared_ptr<RenderModel> request_static(
            const mirinae::respath_t& res_id,
            DesclayoutManager& desclayouts,
            TextureManager& tex_man
        ) {
            auto found = models_.find(res_id);
            if (models_.end() != found)
                return found->second;

            const auto content = device_.filesys().read_file_to_vector(res_id);
            if (!content.has_value()) {
                spdlog::warn("Failed to read dmd file: {}", res_id.u8string());
                return nullptr;
            }

            dal::parser::Model parsed_model;
            const auto parse_result = dal::parser::parse_dmd(
                parsed_model, content->data(), content->size()
            );
            if (dal::parser::ModelParseResult::success != parse_result) {
                spdlog::warn(
                    "Failed to parse dmd file: {}",
                    static_cast<int>(parse_result)
                );
                return nullptr;
            }

            auto output = std::make_shared<RenderModel>(device_);

            for (const auto& src_unit : parsed_model.units_indexed_) {
                VerticesStaticPair dst_vertices;
                dst_vertices.indices_.assign(
                    src_unit.mesh_.indices_.begin(),
                    src_unit.mesh_.indices_.end()
                );

                for (auto& src_vertex : src_unit.mesh_.vertices_) {
                    auto& dst_vertex = dst_vertices.vertices_.emplace_back();
                    dst_vertex.pos_ = src_vertex.pos_;
                    dst_vertex.normal_ = src_vertex.normal_;
                    dst_vertex.texcoord_ = src_vertex.uv_;
                }

                {
                    const auto tri_count = dst_vertices.indices_.size() / 3;
                    for (size_t i = 0; i < tri_count; ++i) {
                        const auto i0 = dst_vertices.indices_.at(i * 3 + 0);
                        const auto i1 = dst_vertices.indices_.at(i * 3 + 1);
                        const auto i2 = dst_vertices.indices_.at(i * 3 + 2);

                        auto& v0 = dst_vertices.vertices_.at(i0);
                        auto& v1 = dst_vertices.vertices_.at(i1);
                        auto& v2 = dst_vertices.vertices_.at(i2);

                        ::calc_tangents(v0, v1, v2);
                    }
                }

                MaterialResources mat_res;
                mat_res.fetch(res_id, src_unit.material_, tex_man);

                auto& dst_unit =
                    ((src_unit.material_.transparency_)
                         ? output->render_units_alpha_.emplace_back()
                         : output->render_units_.emplace_back());
                dst_unit.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    dst_vertices,
                    mat_res.model_ubuf_,
                    mat_res.albedo_map_->image_view(),
                    mat_res.normal_map_->image_view(),
                    texture_sampler_.get(),
                    cmd_pool_,
                    desclayouts,
                    device_
                );
            }

            for (const auto& src_unit : parsed_model.units_indexed_joint_) {
                VerticesStaticPair dst_vertices;
                dst_vertices.indices_.assign(
                    src_unit.mesh_.indices_.begin(),
                    src_unit.mesh_.indices_.end()
                );

                for (auto& src_vertex : src_unit.mesh_.vertices_) {
                    auto& dst_vertex = dst_vertices.vertices_.emplace_back();
                    dst_vertex.pos_ = src_vertex.pos_;
                    dst_vertex.normal_ = src_vertex.normal_;
                    dst_vertex.texcoord_ = src_vertex.uv_;
                }

                MaterialResources mat_res;
                mat_res.fetch(res_id, src_unit.material_, tex_man);

                auto& dst_unit =
                    ((src_unit.material_.transparency_)
                         ? output->render_units_alpha_.emplace_back()
                         : output->render_units_.emplace_back());
                dst_unit.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    dst_vertices,
                    mat_res.model_ubuf_,
                    mat_res.albedo_map_->image_view(),
                    mat_res.normal_map_->image_view(),
                    texture_sampler_.get(),
                    cmd_pool_,
                    desclayouts,
                    device_
                );
            }

            models_[res_id] = output;
            return output;
        }

        std::shared_ptr<RenderModelSkinned> request_skinned(
            const mirinae::respath_t& res_id,
            DesclayoutManager& desclayouts,
            TextureManager& tex_man
        ) {
            auto found = skin_models_.find(res_id);
            if (skin_models_.end() != found)
                return found->second;

            const auto content = device_.filesys().read_file_to_vector(res_id);
            if (!content.has_value()) {
                spdlog::warn("Failed to read dmd file: {}", res_id.u8string());
                return nullptr;
            }

            dal::parser::Model parsed_model;
            const auto parse_result = dal::parser::parse_dmd(
                parsed_model, content->data(), content->size()
            );
            if (dal::parser::ModelParseResult::success != parse_result) {
                spdlog::warn(
                    "Failed to parse dmd file: {}",
                    static_cast<int>(parse_result)
                );
                return nullptr;
            }

            auto output = std::make_shared<RenderModelSkinned>(device_);

            if (!parsed_model.units_indexed_.empty()) {
                spdlog::warn(
                    "Skinned model '{}' has static units, which are ignored",
                    res_id.u8string()
                );
            }

            for (const auto& src_unit : parsed_model.units_indexed_joint_) {
                VerticesSkinnedPair dst_vertices;

                dst_vertices.indices_.assign(
                    src_unit.mesh_.indices_.begin(),
                    src_unit.mesh_.indices_.end()
                );

                for (auto& src_vertex : src_unit.mesh_.vertices_) {
                    auto& dst_vertex = dst_vertices.vertices_.emplace_back();
                    dst_vertex.pos_ = src_vertex.pos_;
                    dst_vertex.normal_ = src_vertex.normal_;
                    dst_vertex.uv_ = src_vertex.uv_;
                    dst_vertex.joint_indices_ = src_vertex.joint_indices_;
                    dst_vertex.joint_weights_ = src_vertex.joint_weights_;
                }

                {
                    const auto tri_count = dst_vertices.indices_.size() / 3;
                    for (size_t i = 0; i < tri_count; ++i) {
                        const auto i0 = dst_vertices.indices_.at(i * 3 + 0);
                        const auto i1 = dst_vertices.indices_.at(i * 3 + 1);
                        const auto i2 = dst_vertices.indices_.at(i * 3 + 2);

                        auto& v0 = dst_vertices.vertices_.at(i0);
                        auto& v1 = dst_vertices.vertices_.at(i1);
                        auto& v2 = dst_vertices.vertices_.at(i2);

                        ::calc_tangents(v0, v1, v2);
                    }
                }

                MaterialResources mat_res;
                mat_res.fetch(res_id, src_unit.material_, tex_man);

                auto& dst_unit = src_unit.material_.transparency_
                                     ? output->runits_alpha_.emplace_back()
                                     : output->runits_.emplace_back();
                dst_unit.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    dst_vertices,
                    mat_res.model_ubuf_,
                    mat_res.albedo_map_->image_view(),
                    mat_res.normal_map_->image_view(),
                    texture_sampler_.get(),
                    cmd_pool_,
                    desclayouts,
                    device_
                );
            }

            output->skel_anim_->skel_ = parsed_model.skeleton_;
            output->skel_anim_->anims_ = parsed_model.animations_;

            skin_models_[res_id] = output;
            return output;
        }

    private:
        VulkanDevice& device_;
        Sampler texture_sampler_;
        CommandPool cmd_pool_;

        std::map<respath_t, std::shared_ptr<RenderModel>> models_;
        std::map<respath_t, std::shared_ptr<RenderModelSkinned>> skin_models_;
    };


    ModelManager::ModelManager(VulkanDevice& device)
        : pimpl_(std::make_unique<Pimpl>(device)) {}

    ModelManager::~ModelManager() {}

    std::shared_ptr<RenderModel> ModelManager::request_static(
        const mirinae::respath_t& res_id,
        DesclayoutManager& desclayouts,
        TextureManager& tex_man
    ) {
        return pimpl_->request_static(res_id, desclayouts, tex_man);
    }

    std::shared_ptr<RenderModelSkinned> ModelManager::request_skinned(
        const mirinae::respath_t& res_id,
        DesclayoutManager& desclayouts,
        TextureManager& tex_man
    ) {
        return pimpl_->request_skinned(res_id, desclayouts, tex_man);
    }

}  // namespace mirinae


// RenderActor
namespace mirinae {

    void RenderActor::init(
        uint32_t max_flight_count, DesclayoutManager& desclayouts
    ) {
        desc_pool_.init(max_flight_count, device_.logi_device());
        desc_sets_ = desc_pool_.alloc(
            max_flight_count,
            desclayouts.get("gbuf:actor"),
            device_.logi_device()
        );

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& ubuf = uniform_buf_.emplace_back();
            ubuf.init_ubuf(sizeof(U_GbufActor), device_.mem_alloc());
        }

        for (size_t i = 0; i < max_flight_count; i++) {
            DescWriteInfoBuilder builder;
            builder.add_uniform_buffer(uniform_buf_.at(i), desc_sets_.at(i))
                .apply_all(device_.logi_device());
        }
    }

    void RenderActor::destroy() {
        for (auto& ubuf : uniform_buf_) ubuf.destroy(device_.mem_alloc());
        uniform_buf_.clear();

        desc_pool_.destroy(device_.logi_device());
    }

    void RenderActor::udpate_ubuf(
        uint32_t index, const U_GbufActor& data, VulkanMemoryAllocator mem_alloc
    ) {
        auto& ubuf = uniform_buf_.at(index);
        ubuf.set_data(&data, sizeof(U_GbufActor), mem_alloc);
    }

    VkDescriptorSet RenderActor::get_desc_set(size_t index) {
        return desc_sets_.at(index);
    }

}  // namespace mirinae


// RenderActorSkinned
namespace mirinae {

    void RenderActorSkinned::init(
        uint32_t max_flight_count, DesclayoutManager& desclayouts
    ) {
        desc_pool_.init(max_flight_count, device_.logi_device());
        desc_sets_ = desc_pool_.alloc(
            max_flight_count,
            desclayouts.get("gbuf:actor_skinned"),
            device_.logi_device()
        );

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& ubuf = uniform_buf_.emplace_back();
            ubuf.init_ubuf(sizeof(U_GbufActorSkinned), device_.mem_alloc());
        }

        for (size_t i = 0; i < max_flight_count; i++) {
            DescWriteInfoBuilder builder;
            builder.add_uniform_buffer(uniform_buf_.at(i), desc_sets_.at(i))
                .apply_all(device_.logi_device());
        }
    }

    void RenderActorSkinned::destroy() {
        for (auto& ubuf : uniform_buf_) ubuf.destroy(device_.mem_alloc());
        uniform_buf_.clear();

        desc_pool_.destroy(device_.logi_device());
    }

    void RenderActorSkinned::udpate_ubuf(
        uint32_t index,
        const U_GbufActorSkinned& data,
        VulkanMemoryAllocator mem_alloc
    ) {
        auto& ubuf = uniform_buf_.at(index);
        ubuf.set_data(&data, sizeof(U_GbufActorSkinned), mem_alloc);
    }

    VkDescriptorSet RenderActorSkinned::get_desc_set(size_t index) {
        return desc_sets_.at(index);
    }

}  // namespace mirinae
