#include "mirinae/render/renderee.hpp"


// RenderUnit
namespace mirinae {

    void RenderUnit::init(
        uint32_t max_flight_count,
        const mirinae::VerticesStaticPair& vertices,
        VkImageView image_view,
        VkSampler texture_sampler,
        mirinae::CommandPool& cmd_pool,
        mirinae::DescriptorSetLayout& layout,
        VulkanDevice& vulkan_device
    ) {
        desc_pool_.init(max_flight_count, vulkan_device.logi_device());
        desc_sets_ = desc_pool_.alloc(max_flight_count, layout, vulkan_device.logi_device());

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& ubuf = uniform_buf_.emplace_back();
            ubuf.init_ubuf(sizeof(mirinae::U_Unorthodox), vulkan_device.mem_alloc());
        }

        for (size_t i = 0; i < max_flight_count; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniform_buf_.at(i).buffer();
            bufferInfo.offset = 0;
            bufferInfo.range = uniform_buf_.at(i).size();

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
                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pBufferInfo = &bufferInfo;
            }
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

    void RenderUnit::destroy(mirinae::VulkanMemoryAllocator mem_alloc, VkDevice logi_device) {
        for (auto& ubuf : uniform_buf_)
            ubuf.destroy(mem_alloc);
        uniform_buf_.clear();

        vert_index_pair_.destroy(mem_alloc);
        desc_pool_.destroy(logi_device);
    }

    void RenderUnit::udpate_ubuf(uint32_t index, const glm::mat4& view_mat, const glm::mat4& proj_mat, mirinae::VulkanMemoryAllocator mem_alloc) {
        auto& ubuf = uniform_buf_.at(index);
        ubuf_data_.model = transform_.make_model_mat();
        ubuf_data_.view = view_mat;
        ubuf_data_.proj = proj_mat;
        ubuf.set_data(&ubuf_data_, sizeof(mirinae::U_Unorthodox), mem_alloc);
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
