#pragma once

#include <array>

#include "mirinae/vulkan/base/render/vkmajorplayers.hpp"


namespace mirinae {

    class FrameSync {

    public:
        void init(VkDevice logi_device);
        void destroy(VkDevice logi_device);

        Semaphore& get_cur_img_ava_semaph();
        Semaphore& get_cur_render_fin_semaph();
        Fence& get_cur_in_flight_fence();

        FrameIndex get_frame_index() const;
        void increase_frame_index();

    private:
        constexpr static size_t S = MAX_FRAMES_IN_FLIGHT;

        std::array<Semaphore, S> img_available_semaphores_;
        std::array<Semaphore, S> render_finished_semaphores_;
        std::array<Fence, S> in_flight_fences_;
        FrameIndex cur_frame_{ 0 };
    };

}  // namespace mirinae
