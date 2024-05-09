#pragma once

#include "mirinae/render/renderee.hpp"


namespace mirinae {

    struct WidgetRenderUniData {
        double width() const { return screen_size_.x; }
        double height() const { return screen_size_.y; }

        glm::dvec2 screen_size_;
        size_t frame_index_;
        VkCommandBuffer cmd_buf_;
        VkPipelineLayout pipe_layout_;
    };


    class IWidget {

    public:
        virtual ~IWidget() = default;
        virtual void record_render(const WidgetRenderUniData& uniform_data) {}

        virtual void hide(bool hidden) {}
        virtual bool hidden() const { return false; }

        virtual void on_parent_resize(double width, double height) {}
        virtual bool on_key_event(const key::Event& e) { return false; }
        virtual bool on_mouse_event(const mouse::Event& e) { return false; }
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
        bool on_mouse_event(const mouse::Event& e);

        std::vector<std::unique_ptr<IWidget>>::iterator begin();
        std::vector<std::unique_ptr<IWidget>>::iterator end();

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
