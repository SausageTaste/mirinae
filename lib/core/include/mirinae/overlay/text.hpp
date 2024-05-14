#pragma once

#include <stb_truetype.h>

#include "mirinae/overlay/iwidget.hpp"


namespace mirinae {

    class StaticVector {

    public:
        uint32_t& operator[](size_t index) { return data_[index]; }
        const uint32_t& operator[](size_t index) const { return data_[index]; }

        bool push_back(uint32_t value) {
            if (fill_size_ >= CAPACITY)
                return false;

            data_[fill_size_++] = value;
            return true;
        }
        bool pop_back() {
            if (fill_size_ == 0)
                return false;

            --fill_size_;
            return true;
        }
        void clear() { fill_size_ = 0; }

        size_t size() const { return fill_size_; }
        size_t capacity() const { return CAPACITY; }
        size_t remaining() const { return CAPACITY - fill_size_; }

        bool is_full() const { return fill_size_ >= CAPACITY; }
        bool is_empty() const { return fill_size_ == 0; }

        const uint32_t* begin() const { return data_; }
        const uint32_t* end() const { return data_ + fill_size_; }

    private:
        constexpr static size_t CAPACITY = 1024;
        uint32_t data_[CAPACITY];
        size_t fill_size_ = 0;
    };


    class TextBlocks {

    public:
        class Block {

        public:
            bool append(uint32_t value);
            bool pop_back();

            bool is_full() const;
            bool is_empty() const;

            const uint32_t* begin() const;
            const uint32_t* end() const;

        private:
            StaticVector data_;
            size_t new_line_count_ = 0;
        };

        void append(char c);
        void append(uint32_t c);
        void append(const std::string_view str);

        void pop_back();
        void clear();

        std::string make_str() const;

        std::vector<Block>::const_iterator begin() const;
        std::vector<Block>::const_iterator end() const;

    private:
        Block& last_valid_block();

        std::vector<Block> blocks_;
    };


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
        std::shared_ptr<TextBlocks> texts_;
        glm::dvec2 last_mouse_pos_;
        double line_spacing_ = 1.2;
        bool word_wrap_ = true;
        bool owning_mouse_ = false;
    };

}  // namespace mirinae
