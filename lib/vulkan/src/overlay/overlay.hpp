#pragma once

#include "mirinae/lua/fwd.hpp"

#include "overlay/iwidget.hpp"
#include "overlay/text.hpp"


namespace mirinae {

    class IDevConsole : public IWidget {

    public:
        virtual void replace_output_buf(std::shared_ptr<ITextData>& texts) = 0;
    };

    std::unique_ptr<IDevConsole> create_dev_console(
        mirinae::TextRenderData& text_render_data,
        mirinae::DesclayoutManager& desclayout,
        mirinae::ITextureManager& tex_man,
        mirinae::ScriptEngine& script,
        mirinae::VulkanDevice& device
    );


    class OverlayManager : public IInputProcessor {

    public:
        OverlayManager(
            uint32_t win_width,
            uint32_t win_height,
            mirinae::DesclayoutManager& desclayout,
            mirinae::ITextureManager& tex_man,
            mirinae::VulkanDevice& device
        );
        ~OverlayManager();

        void tick(WidgetRenderUniData& ren_data);
        void record_render(WidgetRenderUniData& ren_data);

        void on_fbuf_resize(uint32_t width, uint32_t height);
        bool on_key_event(const mirinae::key::Event& e) override;
        bool on_text_event(char32_t c) override;
        bool on_mouse_event(const mouse::Event& e) override;

        const WindowDimInfo& win_dim() const;
        TextRenderData& text_render_data();

        WidgetManager& widgets();
        WidgetManager const& widgets() const;

        void create_image_view(VkImageView img_view, int x, int y);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
