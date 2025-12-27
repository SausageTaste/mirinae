#include "util/frame_sync.hpp"


// FrameSync
namespace mirinae {

    void FrameSync::init(VkDevice logi_device) {
        this->destroy(logi_device);

        for (auto& x : img_available_semaphores_) x.init(logi_device);
        for (auto& x : render_finished_semaphores_) x.init(logi_device);
        for (auto& x : in_flight_fences_) x.init(true, logi_device);
    }

    void FrameSync::destroy(VkDevice logi_device) {
        for (auto& x : img_available_semaphores_) x.destroy(logi_device);
        for (auto& x : render_finished_semaphores_) x.destroy(logi_device);
        for (auto& x : in_flight_fences_) x.destroy(logi_device);
    }

    Semaphore& FrameSync::get_cur_img_ava_semaph() {
        return img_available_semaphores_.at(cur_frame_.get());
    }

    Semaphore& FrameSync::get_cur_render_fin_semaph() {
        return render_finished_semaphores_.at(cur_frame_.get());
    }

    Fence& FrameSync::get_cur_in_flight_fence() {
        return in_flight_fences_.at(cur_frame_.get());
    }

    FrameIndex FrameSync::get_frame_index() const { return cur_frame_; }

    void FrameSync::increase_frame_index() {
        cur_frame_ = (cur_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    }

}  // namespace mirinae


// Free functions
namespace mirinae {

    bool is_fbuf_too_small(uint32_t width, uint32_t height) {
        if (width < 5)
            return true;
        if (height < 5)
            return true;
        else
            return false;
    }

}  // namespace mirinae
