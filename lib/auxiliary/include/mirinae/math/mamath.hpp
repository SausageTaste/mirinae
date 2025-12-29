#pragma once

#include <dal/auxiliary/perspective.hpp>
#include <dal/auxiliary/transform.hpp>
#include <sung/basic/angle.hpp>


namespace mirinae {

    template <typename T>
    glm::tvec2<T> rotate_vec(const glm::tvec2<T>& vec, sung::TAngle<T> angle) {
        const auto cos_a = std::cos(angle.rad());
        const auto sin_a = std::sin(angle.rad());
        return glm::tvec2<T>{ vec.x * cos_a - vec.y * sin_a,
                              vec.x * sin_a + vec.y * cos_a };
    }

    template <typename T>
    glm::tvec2<T> calc_negated_ndc_pos(
        const glm::tvec3<T>& p, const glm::tmat4x4<T>& pvm
    ) {
        auto p_clip = pvm * glm::tvec4<T>{ p, 1 };
        const auto w_rcp = 1 / std::abs(p_clip.w);
        return glm::tvec2<T>(p_clip.x * w_rcp, p_clip.y * w_rcp);
    }

    template <typename T>
    glm::tvec2<T> calc_screen_pos(
        T x_ndc, T y_ndc, T fbuf_width, T fbuf_height
    ) {
        constexpr T ONE = 1.0;
        constexpr T HALF = 0.5;

        return glm::tvec2<T>(
            (HALF + x_ndc * HALF) * fbuf_width,
            (HALF - y_ndc * HALF) * fbuf_height
        );
    }


    template <typename T>
    using TransformQuat = dal::TransformQuat<T>;

    template <typename T>
    using PerspectiveCamera = dal::PerspectiveCamera<T>;

}  // namespace mirinae
