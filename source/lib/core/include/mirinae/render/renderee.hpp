#pragma once

#include <vector>

#include "mirinae/actor/transform.hpp"

#include "mem_alloc.hpp"
#include "uniform.hpp"
#include "vkcomposition.hpp"
#include "vkdevice.hpp"


namespace mirinae {

    class RenderUnit {

    public:
        void init(
            uint32_t max_flight_count,
            const mirinae::VerticesStaticPair& vertices,
            VkImageView image_view,
            VkSampler texture_sampler,
            mirinae::CommandPool& cmd_pool,
            mirinae::DescriptorSetLayout& layout,
            VulkanDevice& vulkan_device
        );
        void destroy(mirinae::VulkanMemoryAllocator mem_alloc, VkDevice logi_device);

        void udpate_ubuf(uint32_t index, const glm::mat4& view_mat, const glm::mat4& proj_mat, mirinae::VulkanMemoryAllocator mem_alloc);
        VkDescriptorSet get_desc_set(size_t index);
        void record_bind_vert_buf(VkCommandBuffer cmdbuf);
        uint32_t vertex_count() const;

        mirinae::TransformQuat transform_;

    private:
        mirinae::U_Unorthodox ubuf_data_;
        mirinae::DescriptorPool desc_pool_;
        mirinae::VertexIndexPair vert_index_pair_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<mirinae::Buffer> uniform_buf_;

    };

}
