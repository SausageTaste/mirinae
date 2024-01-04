#include "mirinae/render/renderee.hpp"

#include <spdlog/spdlog.h>

#include <daltools/model_parser.h>


namespace {

    uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice phys_device) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(phys_device, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
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

        if (vkCreateImage(logi_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(logi_device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = ::find_memory_type(memRequirements.memoryTypeBits, properties, phys_device);

        if (vkAllocateMemory(logi_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
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
        barrier.srcAccessMask = 0; // TODO
        barrier.dstAccessMask = 0; // TODO
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mip_levels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags src_stage;
        VkPipelineStageFlags dst_stage;
        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else {
            spdlog::error("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            cmd_buf,
            src_stage,
            dst_stage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
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

            vkCmdPipelineBarrier(cmd_buf,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(cmd_buf,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR
            );

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd_buf,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mip_levels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd_buf,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier
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
        void init(VkImage image, uint32_t mip_levels, VkFormat format, VkImageAspectFlags aspect_flags, VkDevice logi_device) {
            this->destroy(logi_device);
            this->handle_ = mirinae::create_image_view(image, mip_levels, format, aspect_flags, logi_device);
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

}


// TextureManager
namespace mirinae {

    class TextureData : public ITexture {

    public:
        TextureData(VulkanDevice& device) : device_(device) {}

        ~TextureData() {
            this->destroy();
        }

        void init(
            const std::string& id,
            const IImage2D& image,
            CommandPool& cmd_pool
        ) {
            id_ = id;

            Buffer staging_buffer;
            staging_buffer.init_staging(image.data_size(), device_.mem_alloc());
            staging_buffer.set_data(image.data(), image.data_size(), device_.mem_alloc());

            texture_.init_rgba8_srgb(image.width(), image.height(), device_.mem_alloc());
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

            texture_view_.init(texture_.image(), texture_.mip_levels(), texture_.format(), VK_IMAGE_ASPECT_COLOR_BIT, device_.logi_device());
        }

        void init_depth(uint32_t width, uint32_t height) {
            id_ = "<depth>";

            const auto depth_format = device_.find_supported_format(
                { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            );

            texture_.init_depth(width, height, depth_format, device_.mem_alloc());
            texture_view_.init(texture_.image(), 1, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, device_.logi_device());
        }

        void destroy() {
            texture_view_.destroy(device_.logi_device());
            texture_.destroy(device_.mem_alloc());
        }

        VkFormat format() override {
            return texture_.format();
        }

        VkImageView image_view() override {
            return texture_view_.get();
        }

        auto& id() const { return id_; }

    private:
        VulkanDevice& device_;
        Image texture_;
        ::ImageView texture_view_;
        std::string id_;

    };


    class TextureManager::Pimpl {

    public:
        Pimpl(VulkanDevice& device)
            : device_(device)
        {
            cmd_pool_.init(device_.graphics_queue_family_index().value(), device_.logi_device());
        }

        ~Pimpl() {
            this->destroy_all();
            cmd_pool_.destroy(device_.logi_device());
        }

        std::shared_ptr<TextureData> request(const std::string& res_id) {
            if (auto index = this->find_index(res_id))
                return textures_.at(index.value());

            const auto img_data = device_.filesys().read_file_to_vector(res_id.c_str());
            if (!img_data.has_value()) {
                spdlog::error("Failed to read image file: {}", res_id);
                return nullptr;
            }

            const auto image = mirinae::parse_image(img_data->data(), img_data->size());
            auto& output = textures_.emplace_back(new TextureData{ device_ });
            output->init(res_id, *image, cmd_pool_);
            return output;
        }

        std::unique_ptr<ITexture> create_depth(uint32_t width, uint32_t height) {
            auto output = std::make_unique<TextureData>(device_);
            output->init_depth(width, height);
            return output;
        }

    private:
        std::optional<size_t> find_index(const std::string& id) {
            for (size_t i = 0; i < textures_.size(); ++i) {
                if (textures_.at(i)->id() == id)
                    return i;
            }
            return std::nullopt;
        }

        void destroy_all() {
            for (auto& tex : textures_) {
                if (tex.use_count() > 1)
                    spdlog::warn("Want to destroy texture '{}' is still in use", tex->id());
                tex->destroy();
            }
            textures_.clear();
        }

        VulkanDevice& device_;
        CommandPool cmd_pool_;
        std::vector<std::shared_ptr<TextureData>> textures_;

    };


    TextureManager::TextureManager(VulkanDevice& device)
        : pimpl_(std::make_unique<Pimpl>(device))
    {

    }

    TextureManager::~TextureManager() {

    }

    std::shared_ptr<ITexture> TextureManager::request(const std::string& res_id) {
        return pimpl_->request(res_id);
    }

    std::unique_ptr<ITexture> TextureManager::create_depth(uint32_t width, uint32_t height) {
        return pimpl_->create_depth(width, height);
    }

}


// RenderUnit
namespace mirinae {

    void RenderUnit::init(
        uint32_t max_flight_count,
        const VerticesStaticPair& vertices,
        VkImageView image_view,
        VkSampler texture_sampler,
        CommandPool& cmd_pool,
        DescLayoutBundle& desclayouts,
        VulkanDevice& vulkan_device
    ) {
        desc_pool_.init(max_flight_count, vulkan_device.logi_device());
        desc_sets_ = desc_pool_.alloc(max_flight_count, desclayouts.model_.get(), vulkan_device.logi_device());

        for (size_t i = 0; i < max_flight_count; i++) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = image_view;
            imageInfo.sampler = texture_sampler;

            std::vector<VkWriteDescriptorSet> write_info{};
            {
                auto& descriptorWrite = write_info.emplace_back();
                descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrite.dstSet = desc_sets_.at(i);
                descriptorWrite.dstBinding = static_cast<uint32_t>(write_info.size() - 1);
                descriptorWrite.dstArrayElement = 0;
                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pImageInfo = &imageInfo;
            }

            vkUpdateDescriptorSets(vulkan_device.logi_device(), static_cast<uint32_t>(write_info.size()), write_info.data(), 0, nullptr);
        }

        vert_index_pair_.init(
            vertices,
            cmd_pool,
            vulkan_device.mem_alloc(),
            vulkan_device.graphics_queue(),
            vulkan_device.logi_device()
        );
    }

    void RenderUnit::destroy(VulkanMemoryAllocator mem_alloc, VkDevice logi_device) {
        vert_index_pair_.destroy(mem_alloc);
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

}


// RenderModel
namespace mirinae {

    RenderModel::~RenderModel() {
        for (auto& unit : render_units_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
    }

}


// ModelManager
namespace mirinae {

    class ModelManager::Pimpl {

    public:
        Pimpl(VulkanDevice& device)
            : device_(device)
        {
            texture_sampler_.init(
                device_.is_anisotropic_filtering_supported(),
                device_.max_sampler_anisotropy(),
                device_.logi_device()
            );
            cmd_pool_.init(device_.graphics_queue_family_index().value(), device_.logi_device());
        }

        ~Pimpl() {
            cmd_pool_.destroy(device_.logi_device());
            texture_sampler_.destroy(device_.logi_device());
        }

        std::shared_ptr<RenderModel> request_static(const mirinae::respath_t& res_id, DescLayoutBundle& desclayouts, TextureManager& tex_man) {
            auto found = models_.find(res_id);
            if (models_.end() != found)
                return found->second;

            const auto content = device_.filesys().read_file_to_vector(res_id.c_str());
            if (!content.has_value()) {
                spdlog::error("Failed to read dmd file: {}", res_id);
                return nullptr;
            }

            dal::parser::Model parsed_model;
            const auto parse_result = dal::parser::parse_dmd(parsed_model, content->data(), content->size());
            if (dal::parser::ModelParseResult::success != parse_result) {
                spdlog::error("Failed to parse dmd file: {}", static_cast<int>(parse_result));
                return nullptr;
            }

            std::shared_ptr<RenderModel> output = std::make_shared<RenderModel>(device_);

            for (const auto& src_unit : parsed_model.units_indexed_) {
                VerticesStaticPair dst_vertices;
                dst_vertices.indices_.assign(src_unit.mesh_.indices_.begin(), src_unit.mesh_.indices_.end());

                for (auto& src_vertex : src_unit.mesh_.vertices_) {
                    auto& dst_vertex = dst_vertices.vertices_.emplace_back();
                    dst_vertex.pos_ = src_vertex.pos_;
                    dst_vertex.normal_ = src_vertex.normal_;
                    dst_vertex.texcoord_ = src_vertex.uv_;
                }

                const auto new_texture_path = replace_file_name_ext(res_id, src_unit.material_.albedo_map_);
                auto texture = tex_man.request(new_texture_path);
                if (!texture)
                    texture = tex_man.request("asset/textures/missing_texture.png");

                auto& dst_unit = output->render_units_.emplace_back();
                dst_unit.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    dst_vertices,
                    texture->image_view(),
                    texture_sampler_.get(),
                    cmd_pool_,
                    desclayouts,
                    device_
                );
            }

            for (const auto& src_unit : parsed_model.units_indexed_joint_) {
                VerticesStaticPair dst_vertices;
                dst_vertices.indices_.assign(src_unit.mesh_.indices_.begin(), src_unit.mesh_.indices_.end());

                for (auto& src_vertex : src_unit.mesh_.vertices_) {
                    auto& dst_vertex = dst_vertices.vertices_.emplace_back();
                    dst_vertex.pos_ = src_vertex.pos_;
                    dst_vertex.normal_ = src_vertex.normal_;
                    dst_vertex.texcoord_ = src_vertex.uv_;
                }

                const auto new_texture_path = replace_file_name_ext(res_id, src_unit.material_.albedo_map_);
                auto texture = tex_man.request(new_texture_path);
                if (!texture)
                    texture = tex_man.request("asset/textures/missing_texture.png");

                auto& dst_unit = output->render_units_.emplace_back();
                dst_unit.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    dst_vertices,
                    texture->image_view(),
                    texture_sampler_.get(),
                    cmd_pool_,
                    desclayouts,
                    device_
                );
            }

            models_[res_id] = output;
            return output;
        }

    private:
        VulkanDevice& device_;
        Sampler texture_sampler_;
        CommandPool cmd_pool_;

        std::map<std::string, std::shared_ptr<RenderModel>> models_;

    };


    ModelManager::ModelManager(VulkanDevice& device)
        : pimpl_(std::make_unique<Pimpl>(device))
    {

    }

    ModelManager::~ModelManager() {

    }

    std::shared_ptr<RenderModel> ModelManager::request_static(const mirinae::respath_t& res_id, DescLayoutBundle& desclayouts, TextureManager& tex_man) {
        return pimpl_->request_static(res_id, desclayouts, tex_man);
    }

}


// RenderActor
namespace mirinae {

    void RenderActor::init(
        uint32_t max_flight_count,
        DescLayoutBundle& desclayouts
    ) {
        desc_pool_.init(max_flight_count, device_.logi_device());
        desc_sets_ = desc_pool_.alloc(max_flight_count, desclayouts.actor_.get(), device_.logi_device());

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& ubuf = uniform_buf_.emplace_back();
            ubuf.init_ubuf(sizeof(U_Unorthodox), device_.mem_alloc());
        }

        for (size_t i = 0; i < max_flight_count; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniform_buf_.at(i).buffer();
            bufferInfo.offset = 0;
            bufferInfo.range = uniform_buf_.at(i).size();

            std::vector<VkWriteDescriptorSet> write_info{};
            {
                auto& descriptorWrite = write_info.emplace_back();
                descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrite.dstSet = desc_sets_.at(i);
                descriptorWrite.dstBinding = static_cast<uint32_t>(write_info.size() - 1);
                descriptorWrite.dstArrayElement = 0;
                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pBufferInfo = &bufferInfo;
            }

            vkUpdateDescriptorSets(device_.logi_device(), static_cast<uint32_t>(write_info.size()), write_info.data(), 0, nullptr);
        }
    }

    void RenderActor::destroy() {
        for (auto& ubuf : uniform_buf_)
            ubuf.destroy(device_.mem_alloc());
        uniform_buf_.clear();

        desc_pool_.destroy(device_.logi_device());
    }

    void RenderActor::udpate_ubuf(uint32_t index, const glm::mat4& view_mat, const glm::mat4& proj_mat, VulkanMemoryAllocator mem_alloc) {
        auto& ubuf = uniform_buf_.at(index);
        ubuf_data_.model = transform_.make_model_mat();
        ubuf_data_.view = view_mat;
        ubuf_data_.proj = proj_mat;
        ubuf.set_data(&ubuf_data_, sizeof(U_Unorthodox), mem_alloc);
    }

    VkDescriptorSet RenderActor::get_desc_set(size_t index) {
        return desc_sets_.at(index);
    }

}
