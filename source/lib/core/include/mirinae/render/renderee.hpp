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
            DescriptorSetLayout& layout,
            VulkanDevice& vulkan_device
        );
        void destroy(VulkanMemoryAllocator mem_alloc, VkDevice logi_device);

        void udpate_ubuf(uint32_t index, const glm::mat4& view_mat, const glm::mat4& proj_mat, VulkanMemoryAllocator mem_alloc);
        VkDescriptorSet get_desc_set(size_t index);
        void record_bind_vert_buf(VkCommandBuffer cmdbuf);
        uint32_t vertex_count() const;

        TransformQuat transform_;

    private:
        U_Unorthodox ubuf_data_;
        DescriptorPool desc_pool_;
        VertexIndexPair vert_index_pair_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<Buffer> uniform_buf_;

    };

}
