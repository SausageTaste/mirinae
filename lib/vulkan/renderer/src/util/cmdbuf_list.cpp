#include "util/cmdbuf_list.hpp"


// CmdBufList
namespace mirinae {

    CmdBufList::CmdBufList() { frame_data_.resize(MAX_FRAMES_IN_FLIGHT); }

    void CmdBufList::clear(FrameIndex f_idx) {
        frame_data_.at(f_idx.get()).cmdbufs_.clear();
    }

    void CmdBufList::add(VkCommandBuffer cmdbuf, FrameIndex f_idx) {
        frame_data_.at(f_idx.get()).cmdbufs_.push_back(cmdbuf);
    }

    const VkCommandBuffer* CmdBufList::data(FrameIndex f_idx) const {
        return frame_data_.at(f_idx.get()).cmdbufs_.data();
    }

    size_t CmdBufList::size(FrameIndex f_idx) const {
        return frame_data_.at(f_idx.get()).cmdbufs_.size();
    }

    std::vector<VkCommandBuffer>& CmdBufList::vector(FrameIndex f_idx) {
        return frame_data_.at(f_idx.get()).cmdbufs_;
    }

}  // namespace mirinae
