#pragma once

#include "mirinae/overlay/iwidget.hpp"
#include "mirinae/overlay/text.hpp"
#include "mirinae/util/script.hpp"


namespace mirinae {

    class IDevConsole : public IWidget {

    public:
        virtual void replace_output_buf(std::shared_ptr<ITextData>& texts) = 0;
    };

    std::unique_ptr<IDevConsole> create_dev_console(
        VkSampler sampler,
        mirinae::TextRenderData& text_render_data,
        mirinae::DesclayoutManager& desclayout,
        mirinae::TextureManager& tex_man,
        mirinae::ScriptEngine& script,
        mirinae::VulkanDevice& device
    );


    class OverlayManager : public IInputProcessor {

    public:
        OverlayManager(
            uint32_t win_width,
            uint32_t win_height,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        );
        ~OverlayManager();

        void tick(WidgetRenderUniData& ren_data);
        void record_render(WidgetRenderUniData& ren_data);

        void on_fbuf_resize(uint32_t width, uint32_t height);
        bool on_key_event(const mirinae::key::Event& e) override;
        bool on_text_event(char32_t c) override;
        bool on_mouse_event(const mouse::Event& e) override;

        VkSampler sampler() const;
        const WindowDimInfo& win_dim() const;
        TextRenderData& text_render_data();

        WidgetManager& widgets();
        WidgetManager const& widgets() const;

        void create_image_view(VkImageView img_view);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
