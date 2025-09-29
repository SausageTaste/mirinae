#pragma once

#include <cstdint>

#include "mirinae/lightweight/lightweights.hpp"


namespace mirinae {

    constexpr static int MAX_FRAMES_IN_FLIGHT = 2;

    using FrameIndex = StrongType<int, struct FrameIndexTag>;

    // It stands for Swapchain Image Index
    using ShainImageIndex = StrongType<uint32_t, struct SwapchainImgIdxTag>;


    struct RpCtxtBase {
        FrameIndex f_index_;
        ShainImageIndex i_index_;
        double dt_;
    };

}  // namespace mirinae
