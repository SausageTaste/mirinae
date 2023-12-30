#include "mirinae/render/renderee.hpp"

#include <spdlog/spdlog.h>

#include <daltools/model_parser.h>


// TextureManager
namespace mirinae {

    class TextureData : public ITexture {

    public:
        void init(
            const std::string& id,
            const IImage2D& image,
            CommandPool& cmd_pool,
            VulkanDevice& device
        ) {
            id_ = id;

            Buffer staging_buffer;
            staging_buffer.init_staging(image.data_size(), device.mem_alloc());
            staging_buffer.set_data(image.data(), image.data_size(), device.mem_alloc());

            texture_.init_rgba8_srgb(image.width(), image.height(), device.mem_alloc());
            mirinae::copy_to_img_and_transition(
                texture_.image(),
                texture_.width(),
                texture_.height(),
                texture_.format(),
                staging_buffer.buffer(),
                cmd_pool,
                device.graphics_queue(),
                device.logi_device()
            );
            staging_buffer.destroy(device.mem_alloc());

            texture_view_.init(texture_.image(), texture_.format(), VK_IMAGE_ASPECT_COLOR_BIT, device.logi_device());
        }

        void destroy(VulkanDevice& device) {
            texture_view_.destroy(device.logi_device());
            texture_.destroy(device.mem_alloc());
        }

        VkImageView image_view() override {
            return texture_view_.get();
        }

        auto& id() const { return id_; }

    private:
        Image texture_;
        ImageView texture_view_;
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
            auto& output = textures_.emplace_back(new TextureData);
            output->init(res_id, *image, cmd_pool_, device_);
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
                tex->destroy(device_);
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
