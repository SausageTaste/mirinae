#pragma once

#include <stb_truetype.h>

#include "mirinae/overlay/iwidget.hpp"
#include "mirinae/util/text_data.hpp"


namespace mirinae {

    class FontLibrary {

    public:
        FontLibrary(mirinae::IFilesys& filesys);

        const unsigned char* data() const { return font_.data; }

    private:
        stbtt_fontinfo font_;
        std::vector<uint8_t> file_data_;
    };


    class TextRenderData {

    public:
        TextRenderData(VulkanDevice& device);

        void init_ascii(
            VkSampler sampler,
            FontLibrary& fonts,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        );

        bool is_ready() const;

        VkDescriptorSet get_desc_set(size_t frame_index);
        stbtt_bakedchar& get_char_info(size_t index);

        uint32_t text_height() const;
        uint32_t atlas_width() const;
        uint32_t atlas_height() const;

    private:
        constexpr static int START_CHAR = 32;
        constexpr static int END_CHAR = 127;
        constexpr static int TEXT_HEIGHT = 22;

        std::array<stbtt_bakedchar, END_CHAR - START_CHAR> char_baked_;
        std::unique_ptr<mirinae::ITexture> texture_;
        mirinae::TImage2D<unsigned char> bitmap_;
        mirinae::OverlayRenderUnit render_unit_;
    };


    class TextBox : public IRectWidget {

    public:
        TextBox(TextRenderData& text_render_data);

        void record_render(const WidgetRenderUniData& udata) override;
        bool on_mouse_event(const mouse::Event& e) override;

        std::string make_str() const;

        void add_text(const char c);
        void add_text(const uint32_t c);
        void add_text(const std::string_view str);

        void remove_one_char();
        void clear_text();

        glm::dvec2 scroll_{ 0, 0 };
        bool enable_scroll_ = true;

    private:
        TextRenderData& text_render_data_;
        std::shared_ptr<ITextData> texts_;
        glm::dvec2 last_mouse_pos_;
        double line_spacing_ = 1.2;
        bool word_wrap_ = true;
        bool owning_mouse_ = false;
    };

}  // namespace mirinae