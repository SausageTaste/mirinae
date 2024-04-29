#include "mirinae/render/vkcomposition.hpp"


namespace {

    void set_buffer_data(
        mirinae::Buffer& dst,
        const void* src,
        size_t src_size,
        mirinae::CommandPool& cmdpool,
        mirinae::VulkanMemoryAllocator allocator,
        VkQueue graphics_q,
        VkDevice logi_device
    ) {
        mirinae::Buffer staging_buffer;
        staging_buffer.init_staging(src_size, allocator);
        staging_buffer.set_data(src, src_size, allocator);

        auto cmdbuf = cmdpool.alloc(logi_device);
        dst.record_copy_cmd(staging_buffer, cmdbuf, logi_device);

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmdbuf;
        vkQueueSubmit(graphics_q, 1, &submit_info, VK_NULL_HANDLE);

        vkQueueWaitIdle(graphics_q);
        cmdpool.free(cmdbuf, logi_device);
        staging_buffer.destroy(allocator);
    }

}  // namespace


// VertexIndexPair
namespace mirinae {

    void VertexIndexPair::init(
        const VerticesStaticPair& vertices,
        CommandPool& cmdpool,
        VulkanMemoryAllocator allocator,
        VkQueue graphics_q,
        VkDevice logi_device
    ) {
        // Vertices data size
        const auto v_s = sizeof(VertexStatic) * vertices.vertices_.size();
        vertex_buf_.init_vertices(v_s, allocator);
        ::set_buffer_data(
            vertex_buf_,
            vertices.vertices_.data(),
            v_s,
            cmdpool,
            allocator,
            graphics_q,
            logi_device
        );

        // Indices data size
        const auto i_s = sizeof(uint16_t) * vertices.indices_.size();
        index_buf_.init_indices(i_s, allocator);
        ::set_buffer_data(
            index_buf_,
            vertices.indices_.data(),
            i_s,
            cmdpool,
            allocator,
            graphics_q,
            logi_device
        );

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
        vkCmdBindIndexBuffer(
            cmdbuf, index_buf_.buffer(), 0, VK_INDEX_TYPE_UINT16
        );
    }

    uint32_t VertexIndexPair::vertex_count() const {
        return static_cast<uint32_t>(vertex_count_);
    }

}  // namespace mirinae
