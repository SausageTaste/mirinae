#pragma once

#include "vkmajorplayers.hpp"


namespace mirinae {

    class VertexIndexPair {

    public:
        void init(
            const std::vector<mirinae::VertexStatic>& vertices,
            const std::vector<uint16_t>& indices,
            CommandPool& cmdpool,
            PhysDevice& phys_device,
            LogiDevice& logi_device
        );
        void destroy(LogiDevice& logi_device);

        void record_bind(VkCommandBuffer cmdbuf);

        uint32_t vertex_count() const;

    private:
        void init_vertices(const std::vector<mirinae::VertexStatic>& vertices, CommandPool& cmdpool, PhysDevice& phys_device, LogiDevice& logi_device);
        void init_indices(const std::vector<uint16_t>& indices, CommandPool& cmdpool, PhysDevice& phys_device, LogiDevice& logi_device);

        mirinae::Buffer vertex_buf_;
        mirinae::Buffer index_buf_;
        size_t vertex_count_ = 0;

    };

}