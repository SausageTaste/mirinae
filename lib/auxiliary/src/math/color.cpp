#include "mirinae/math/color.hpp"

#include <algorithm>

#include <imgui.h>


namespace {

    template <typename T>
    constexpr T COLOR_EPSILON = static_cast<T>(0.0000001);


    float abs_max3(float a, float b, float c) {
        return std::max(std::max(std::abs(a), std::abs(b)), std::abs(c));
    }

    float abs_max3(const glm::vec3& v) { return ::abs_max3(v.r, v.g, v.b); }

    template <typename T>
    glm::tvec4<T> split_color(glm::tvec3<T> color) {
        const auto max = ::abs_max3(color);
        if (max < COLOR_EPSILON<T>)
            return glm::tvec4<T>{ 0 };
        else
            return glm::tvec4<T>{ color / max, max };
    }

}  // namespace


namespace mirinae {

    glm::vec4 split_color(const glm::vec3& color) {
        return ::split_color<float>(color);
    }

    glm::vec3 combine_color(const glm::vec4& color) {
        return glm::vec3{ color.r, color.g, color.b } * color.a;
    }

}  // namespace mirinae


// ColorIntensity
namespace mirinae {

    ColorIntensity::Vec3 ColorIntensity::scaled_color() const {
        return color_ * intensity_;
    }

    void ColorIntensity::set_scaled_color(const Vec3& color) {
        color_ = color;
        intensity_ = 1;
        this->normalize_color();
    }

    void ColorIntensity::set_scaled_color(T r, T g, T b) {
        color_.x = r;
        color_.y = g;
        color_.z = b;
        intensity_ = 1;
        this->normalize_color();
    }

    void ColorIntensity::set_scaled_color(T rgb) {
        color_.x = rgb;
        color_.y = rgb;
        color_.z = rgb;
        intensity_ = 1;
        this->normalize_color();
    }

    void ColorIntensity::normalize_color() {
        const auto max = ::abs_max3(color_);
        if (max < COLOR_EPSILON<T>) {
            color_ = glm::tvec3<T>{ 0 };
            intensity_ = 0;
            return;
        }

        color_ /= max;
        intensity_ *= max;
    }

}  // namespace mirinae
