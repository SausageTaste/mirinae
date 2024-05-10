#pragma once

#include <sung/general/aabb.hpp>

#include "mirinae/render/renderee.hpp"


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


    class IWidget {

    public:
        virtual ~IWidget() = default;

        virtual void record_render(const WidgetRenderUniData& uniform_data) {}
        virtual void update_content(const WindowDimInfo& wd) {}

        virtual void hide(bool hidden) {}
        virtual bool hidden() const { return false; }

        virtual bool on_activate() { return false; }
        virtual bool on_deactivate() { return false; }

        virtual bool on_key_event(const key::Event& e) { return false; }
        virtual bool on_text_event(uint32_t c) { return false; }
        virtual bool on_mouse_event(const mouse::Event& e) { return false; }
    };


    class IRectWidget : public IWidget {

    public:
        void hide(bool hidden) override { hidden_ = hidden; }
        bool hidden() const override { return hidden_; }

        sung::AABB2<double> aabb() const {
            return sung::AABB2<double>(
                pos_.x, pos_.x + size_.x, pos_.y, pos_.y + size_.y
            );
        }
        bool is_inside_cl(double x, double y) const {
            return this->aabb().is_inside_cl(x, y);
        }

        glm::dvec2 pos_{ 0, 0 };
        glm::dvec2 size_{ 0, 0 };
        bool hidden_ = false;
    };


    class OverlayManager {

    public:
        OverlayManager(
            uint32_t win_width,
            uint32_t win_height,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        );
        ~OverlayManager();

        void record_render(
            size_t frame_index,
            VkCommandBuffer cmd_buf,
            VkPipelineLayout pipe_layout
        );
        void on_fbuf_resize(uint32_t width, uint32_t height);
        bool on_key_event(const mirinae::key::Event& e);
        bool on_text_event(uint32_t c);
        bool on_mouse_event(const mouse::Event& e);

        std::vector<std::unique_ptr<IWidget>>::iterator begin();
        std::vector<std::unique_ptr<IWidget>>::iterator end();

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
