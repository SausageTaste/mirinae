#include "mirinae/render/vkcomposition.hpp"


// VertexIndexPair
namespace mirinae {

    void VertexIndexPair::init(
        const std::vector<mirinae::VertexStatic>& vertices,
        const std::vector<uint16_t>& indices,
        CommandPool& cmdpool,
        PhysDevice& phys_device,
        LogiDevice& logi_device
    ) {
        this->init_vertices(vertices, cmdpool, phys_device, logi_device);
        this->init_indices(indices, cmdpool, phys_device, logi_device);
        vertex_count_ = indices.size();
    }

    void VertexIndexPair::destroy(LogiDevice& logi_device) {
        vertex_buf_.destroy(logi_device);
        index_buf_.destroy(logi_device);
        vertex_count_ = 0;
    }

    void VertexIndexPair::record_bind(VkCommandBuffer cmdbuf) {
        VkBuffer vertex_buffers[] = { vertex_buf_.buffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmdbuf, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(cmdbuf, index_buf_.buffer(), 0, VK_INDEX_TYPE_UINT16);
    }

    uint32_t VertexIndexPair::vertex_count() const {
        return vertex_count_;
    }

    void VertexIndexPair::init_vertices(const std::vector<mirinae::VertexStatic>& vertices, CommandPool& cmdpool, PhysDevice& phys_device, LogiDevice& logi_device) {
        const auto data_size = sizeof(mirinae::VertexStatic) * vertices.size();

        mirinae::Buffer staging_buffer;
        staging_buffer.init(
            data_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            phys_device,
            logi_device
        );
        staging_buffer.set_data(vertices.data(), data_size, logi_device);

        vertex_buf_.init(data_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            phys_device,
            logi_device
        );

        auto cmdbuf = cmdpool.alloc(logi_device);
        vertex_buf_.record_copy_cmd(staging_buffer, cmdbuf, logi_device);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;
        vkQueueSubmit(logi_device.graphics_queue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(logi_device.graphics_queue());
        cmdpool.free(cmdbuf, logi_device);
        staging_buffer.destroy(logi_device);
    }

    void VertexIndexPair::init_indices(const std::vector<uint16_t>& indices, CommandPool& cmdpool, PhysDevice& phys_device, LogiDevice& logi_device) {
        const auto data_size = sizeof(uint16_t) * indices.size();

        mirinae::Buffer staging_buffer;
        staging_buffer.init(
            data_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            phys_device,
            logi_device
        );
        staging_buffer.set_data(indices.data(), data_size, logi_device);

        index_buf_.init(data_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            phys_device,
            logi_device
        );

        auto cmdbuf = cmdpool.alloc(logi_device);
        index_buf_.record_copy_cmd(staging_buffer, cmdbuf, logi_device);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdbuf;
        vkQueueSubmit(logi_device.graphics_queue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(logi_device.graphics_queue());
        cmdpool.free(cmdbuf, logi_device);
        staging_buffer.destroy(logi_device);
    }

}
