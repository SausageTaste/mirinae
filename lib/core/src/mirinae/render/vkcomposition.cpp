#include "mirinae/render/vkcomposition.hpp"


// VertexIndexPair
namespace mirinae {

    void VertexIndexPair::init(
        const VerticesStaticPair& vertices,
        CommandPool& cmdpool,
        VulkanMemoryAllocator allocator,
        VkQueue graphics_q,
        VkDevice logi_device
    ) {
        this->init_vertices(vertices.vertices_, cmdpool, allocator, graphics_q, logi_device);
        this->init_indices(vertices.indices_, cmdpool, allocator, graphics_q, logi_device);
        vertex_count_ = vertices.indices_.size();
    }

    void VertexIndexPair::destroy(VulkanMemoryAllocator allocator) {
        vertex_buf_.destroy(allocator);
        index_buf_.destroy(allocator);
        vertex_count_ = 0;
    }

    void VertexIndexPair::record_bind(VkCommandBuffer cmdbuf) {
        VkBuffer vertex_buffers[] = { vertex_buf_.buffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(cmdbuf, index_buf_.buffer(), 0, VK_INDEX_TYPE_UINT16);
    }

    uint32_t VertexIndexPair::vertex_count() const {
        return static_cast<uint32_t>(vertex_count_);
    }

    void VertexIndexPair::init_vertices(
        const std::vector<mirinae::VertexStatic>& vertices,
        CommandPool& cmdpool,
        VulkanMemoryAllocator allocator,
        VkQueue graphics_q,
        VkDevice logi_device
    ) {
        const auto data_size = sizeof(mirinae::VertexStatic) * vertices.size();

        mirinae::Buffer staging_buffer;
        staging_buffer.init_staging(data_size, allocator);
        staging_buffer.set_data(vertices.data(), data_size, allocator);

        vertex_buf_.init_vertices(data_size, allocator);

        auto cmdbuf = cmdpool.alloc(logi_device);
        vertex_buf_.record_copy_cmd(staging_buffer, cmdbuf, logi_device);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;
        vkQueueSubmit(graphics_q, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_q);
        cmdpool.free(cmdbuf, logi_device);
        staging_buffer.destroy(allocator);
    }

    void VertexIndexPair::init_indices(
        const std::vector<uint16_t>& indices,
        CommandPool& cmdpool,
        VulkanMemoryAllocator allocator,
        VkQueue graphics_q,
        VkDevice logi_device
    ) {
        const auto data_size = sizeof(uint16_t) * indices.size();

        mirinae::Buffer staging_buffer;
        staging_buffer.init_staging(data_size, allocator);
        staging_buffer.set_data(indices.data(), data_size, allocator);

        index_buf_.init_indices(data_size, allocator);

        auto cmdbuf = cmdpool.alloc(logi_device);
        index_buf_.record_copy_cmd(staging_buffer, cmdbuf, logi_device);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;
        vkQueueSubmit(graphics_q, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_q);
        cmdpool.free(cmdbuf, logi_device);
        staging_buffer.destroy(allocator);
    }

}
