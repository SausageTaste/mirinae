#pragma once

#include "mirinae/lightweight/task.hpp"
#include "mirinae/vulkan/base/renderpass/common.hpp"
#include "util/flags.hpp"


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


    class RenderPassesTask : public DependingTask {

    public:
        void init(
            CmdBufList& cmdbuf_list,
            FlagShip& flag_ship,
            RpCtxt& rp_ctxt,
            RpResources& rp_res,
            std::function<bool()> resize_func,
            std::vector<std::unique_ptr<IRpBase>>& passes,
            VulkanDevice& device
        );

        void prepare();

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override;

        CmdBufList* cmdbuf_list_ = nullptr;
        FlagShip* flag_ship_ = nullptr;
        RpCtxt* rp_ctxt_ = nullptr;
        RpResources* rp_res_ = nullptr;
        std::function<bool()> resize_func_;
        VulkanDevice* device_ = nullptr;

        std::vector<std::unique_ptr<IRpTask>> passes_;
    };

}  // namespace mirinae
