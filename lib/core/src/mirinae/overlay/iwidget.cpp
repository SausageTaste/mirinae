#include "mirinae/overlay/iwidget.hpp"


namespace {

    template <typename T>
    glm::tvec2<T> convert_screen_pos(T x, T y, T width, T height) {
        return glm::tvec2<T>(x / width * 2 - 1, y / height * 2 - 1);
    }

    template <typename T>
    glm::tvec2<T> convert_screen_offset(T x, T y, T width, T height) {
        return glm::tvec2<T>(x / width * 2, y / height * 2);
    }

}


// WindowDimInfo
namespace mirinae {

    WindowDimInfo::WindowDimInfo(double width, double height, double ui_scale)
        : width_(width), height_(height), ui_scale_(ui_scale) {}

    double WindowDimInfo::width() const { return width_; }

    double WindowDimInfo::height() const { return height_; }

    double WindowDimInfo::ui_scale() const { return ui_scale_; }

    glm::dvec2 WindowDimInfo::pos_2_ndc(double x, double y) const {
        return ::convert_screen_pos(x, y, width_, height_);
    }

    glm::dvec2 WindowDimInfo::len_2_ndc(double w, double h) const {
        return ::convert_screen_offset(w, h, width_, height_);
    }

}  // namespace mirinae
