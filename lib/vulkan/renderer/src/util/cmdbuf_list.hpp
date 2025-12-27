#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "mirinae/vulkan/base/context/base.hpp"


namespace mirinae {

    class CmdBufList {

    public:
        CmdBufList();

        void clear(FrameIndex f_idx);
        void add(VkCommandBuffer cmdbuf, FrameIndex f_idx);

        const VkCommandBuffer* data(FrameIndex f_idx) const;
        size_t size(FrameIndex f_idx) const;
        std::vector<VkCommandBuffer>& vector(FrameIndex f_idx);

    private:
        struct FrameData {
            std::vector<VkCommandBuffer> cmdbufs_;
        };

        std::vector<FrameData> frame_data_;
    };


}  // namespace mirinae
