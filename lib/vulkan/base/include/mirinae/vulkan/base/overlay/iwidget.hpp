#pragma once

#include <sung/basic/aabb.hpp>

#include "mirinae/lightweight/input_proc.hpp"
#include "mirinae/vulkan/base/render/renderee.hpp"


namespace mirinae {

    class WindowDimInfo {

    public:
        WindowDimInfo() = default;
        WindowDimInfo(double width, double height, double ui_scale);

        double width() const;
        double height() const;
        double ui_scale() const;

        glm::dvec2 pos_2_ndc(double x, double y) const;
        glm::dvec2 pos_2_ndc(const glm::dvec2& v) const {
            return pos_2_ndc(v.x, v.y);
        }

        glm::dvec2 len_2_ndc(double w, double h) const;
        glm::dvec2 len_2_ndc(const glm::dvec2& v) const {
            return len_2_ndc(v.x, v.y);
        }

    private:
        double width_ = 16;
        double height_ = 16;
        double ui_scale_ = 1;
    };


    struct WidgetRenderUniData {
        double width() const { return win_dim_.width(); }
        double height() const { return win_dim_.height(); }
        glm::dvec2 pos_2_ndc(double x, double y) const {
            return win_dim_.pos_2_ndc(x, y);
        }
        glm::dvec2 len_2_ndc(double w, double h) const {
            return win_dim_.len_2_ndc(w, h);
        }

        WindowDimInfo win_dim_;
        size_t frame_index_;
        VkCommandBuffer cmd_buf_;
        VkPipelineLayout pipe_layout_;
    };


    class IWidget : public IInputProcessor {

    public:
        virtual void tick(const WidgetRenderUniData& ren_data) {}
        virtual void record_render(const WidgetRenderUniData& ren_data) {}
        virtual void update_content(const WindowDimInfo& wd) {}
        virtual void request_update() {};

        virtual void hide(bool hidden) {}
        virtual bool hidden() const { return false; }

        virtual void set_focus(bool focused) {}
        virtual bool focused() const { return false; }
    };


    class IRectWidget : public IWidget {

    public:
        void tick(const WidgetRenderUniData& ren_data) override {
            if (need_update_)
                this->update_content(ren_data.win_dim_);
        }

        void request_update() override { need_update_ = true; };

        void hide(bool hidden) override { hidden_ = hidden; }
        bool hidden() const override { return hidden_; }

        sung::Aabb2D<double> aabb() const {
            return sung::Aabb2D<double>(
                pos_.x, pos_.x + size_.x, pos_.y, pos_.y + size_.y
            );
        }
        bool is_inside_cl(double x, double y) const {
            return this->aabb().is_inside_cl(x, y);
        }

        glm::dvec2 pos_{ 0, 0 };
        glm::dvec2 size_{ 0, 0 };
        bool hidden_ = false;
        bool need_update_ = true;
    };


    class WidgetManager : public IWidget {

    public:
        void tick(const WidgetRenderUniData& ren_data) override;
        void record_render(const WidgetRenderUniData& uniform_data) override;
        void update_content(const WindowDimInfo& wd) override;
        void request_update() override;

        void hide(bool hidden) override;
        bool hidden() const override;
        void set_focus(bool focused) override;
        bool focused() const override;

        bool on_key_event(const key::Event& e) override;
        bool on_text_event(char32_t c) override;
        bool on_mouse_event(const mouse::Event& e) override;

        void add_widget(std::unique_ptr<IWidget>&& widget);
        void remove_widget(IWidget* widget);
        void clear_widgets();

        template <typename TWidget, typename... TArgs>
        TWidget* emplace_back(TArgs&&... args) {
            auto w = std::make_unique<TWidget>(std::forward<TArgs>(args)...);
            w->request_update();
            auto ptr = w.get();
            widgets_.emplace_back(std::move(w));
            return ptr;
        }

        template <typename TWidget>
        TWidget* find_by_type() {
            for (auto& widget : widgets_) {
                if (auto casted = dynamic_cast<TWidget*>(widget.get()))
                    return casted;
            }

            return nullptr;
        }

        std::vector<std::unique_ptr<IWidget>>::iterator begin();
        std::vector<std::unique_ptr<IWidget>>::iterator end();

    private:
        std::vector<std::unique_ptr<IWidget>> widgets_;
        IWidget* focused_widget_ = nullptr;
        bool hidden_ = false;
        bool focused_ = false;
    };

}  // namespace mirinae
