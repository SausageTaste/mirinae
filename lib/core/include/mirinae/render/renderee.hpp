#pragma once

#include <vector>

#include "mirinae/actor/transform.hpp"

#include "uniform.hpp"
#include "vkcomposition.hpp"


namespace mirinae {

    class ITexture {

    public:
        virtual ~ITexture() = default;
        virtual VkImageView image_view() = 0;

    };


    class TextureManager {

    public:
        TextureManager(VulkanDevice& device);
        ~TextureManager();

        std::shared_ptr<ITexture> request(const std::string& res_id);

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> pimpl_;

    };


    class RenderUnit {

    public:
        void init(
            uint32_t max_flight_count,
            const VerticesStaticPair& vertices,
            VkImageView image_view,
            VkSampler texture_sampler,
            CommandPool& cmd_pool,
            DescLayoutBundle& desclayouts,
            VulkanDevice& vulkan_device
        );
        void destroy(VulkanMemoryAllocator mem_alloc, VkDevice logi_device);

        VkDescriptorSet get_desc_set(size_t index);
        void record_bind_vert_buf(VkCommandBuffer cmdbuf);
        uint32_t vertex_count() const;

    private:
        DescriptorPool desc_pool_;
        VertexIndexPair vert_index_pair_;
        std::vector<VkDescriptorSet> desc_sets_;

    };


    class RenderModel {

    public:
        RenderModel(VulkanDevice& vulkan_device) : device_(vulkan_device) {}
        ~RenderModel();

    public:
        std::vector<RenderUnit> render_units_;
        VulkanDevice& device_;

    };


    class ModelManager {

    public:
        ModelManager(VulkanDevice& device);
        ~ModelManager();

        std::shared_ptr<RenderModel> request_static(const std::string& res_id, DescLayoutBundle& desclayouts, TextureManager& tex_man);

    private:
        class Pimpl;
        std::unique_ptr<Pimpl> pimpl_;

    };


    class RenderActor {

    public:
        RenderActor(VulkanDevice& vulkan_device) : device_(vulkan_device) {}
        ~RenderActor() { this->destroy(); }

        void init(
            uint32_t max_flight_count,
            DescLayoutBundle& desclayouts
        );
        void destroy();

        void udpate_ubuf(uint32_t index, const glm::mat4& view_mat, const glm::mat4& proj_mat, VulkanMemoryAllocator mem_alloc);
        VkDescriptorSet get_desc_set(size_t index);

        TransformQuat<double> transform_;

    private:
        DescriptorPool desc_pool_;
        U_Unorthodox ubuf_data_;
        std::vector<Buffer> uniform_buf_;
        std::vector<VkDescriptorSet> desc_sets_;
        VulkanDevice& device_;

    };

}
