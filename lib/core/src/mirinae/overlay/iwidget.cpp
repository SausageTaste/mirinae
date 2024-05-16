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

}  // namespace


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


//
namespace mirinae {

    void WidgetManager::tick() {
        for (auto& widget : widgets_) {
            widget->tick();
        }
    }

    void WidgetManager::record_render(const WidgetRenderUniData& uniform_data) {
        for (auto& widget : widgets_) {
            if (!widget->hidden()) {
                widget->record_render(uniform_data);
            }
        }
    }

    void WidgetManager::update_content(const WindowDimInfo& wd) {
        for (auto& widget : widgets_) {
            widget->update_content(wd);
        }
    }

    void WidgetManager::request_update() {
        for (auto& widget : widgets_) {
            widget->request_update();
        }
    }

    bool WidgetManager::on_key_event(const key::Event& e) {
        if (nullptr != focused_widget_) {
            if (focused_widget_->on_key_event(e))
                return true;
        }

        for (auto& widget : widgets_) {
            if (widget.get() == focused_widget_)
                continue;
            if (widget->on_key_event(e))
                return true;
        }

        return false;
    }

    bool WidgetManager::on_text_event(char32_t e) {
        if (nullptr != focused_widget_) {
            if (focused_widget_->on_text_event(e))
                return true;
        }

        for (auto& widget : widgets_) {
            if (widget.get() == focused_widget_)
                continue;
            if (widget->on_text_event(e))
                return true;
        }

        return false;
    }

    bool WidgetManager::on_mouse_event(const mouse::Event& e) {
        if (nullptr != focused_widget_) {
            if (focused_widget_->on_mouse_event(e))
                return true;
        }

        for (auto& widget : widgets_) {
            if (widget.get() == focused_widget_)
                continue;
            if (widget->on_mouse_event(e))
                return true;
        }

        return false;
    }

    void WidgetManager::hide(bool hidden) { hidden_ = hidden; }

    bool WidgetManager::hidden() const { return hidden_; }

    void WidgetManager::set_focus(bool focused) {
        if (focused_ == focused)
            return;
        focused_ = focused;

        for (auto& widget : widgets_) {
            if (widget.get() == focused_widget_)
                widget->set_focus(focused_);
            else
                widget->set_focus(false);
        }
    }

    bool WidgetManager::focused() const { return focused_; }

    void WidgetManager::add_widget(std::unique_ptr<IWidget>&& widget) {
        widgets_.push_back(std::move(widget));
    }

    void WidgetManager::remove_widget(IWidget* widget) {
        widgets_.erase(
            std::remove_if(
                widgets_.begin(),
                widgets_.end(),
                [widget](const std::unique_ptr<IWidget>& w) -> bool {
                    return w.get() == widget;
                }
            ),
            widgets_.end()
        );
    }

    void WidgetManager::clear_widgets() { widgets_.clear(); }

    std::vector<std::unique_ptr<IWidget>>::iterator WidgetManager::begin() {
        return widgets_.begin();
    }

    std::vector<std::unique_ptr<IWidget>>::iterator WidgetManager::end() {
        return widgets_.end();
    }

}  // namespace mirinae
