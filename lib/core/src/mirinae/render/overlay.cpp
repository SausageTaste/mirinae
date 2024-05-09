#include "mirinae/render/overlay.hpp"

#include <sstream>
#include <string_view>

#define STB_TRUETYPE_IMPLEMENTATION
#include <spdlog/spdlog.h>
#include <stb_truetype.h>
#include <sung/general/aabb.hpp>


namespace {

    template <typename T>
    glm::tvec2<T> convert_screen_pos(T x, T y, T width, T height) {
        return glm::tvec2<T>(x / width * 2 - 1, y / height * 2 - 1);
    }

    template <typename T>
    glm::tvec2<T> convert_screen_offset(T x, T y, T width, T height) {
        return glm::tvec2<T>(x / width * 2, y / height * 2);
    }


    template <typename T>
    glm::tvec2<T> get_aabb_min(const sung::AABB2<T>& aabb) {
        return glm::tvec2<T>(aabb.x_min(), aabb.y_min());
    }

    template <typename T>
    glm::tvec2<T> get_aabb_dim(const sung::AABB2<T>& aabb) {
        return glm::tvec2<T>(aabb.width(), aabb.height());
    };

}  // namespace


namespace {

    class ImageView : public mirinae::IWidget {

    public:
        ImageView(
            VkSampler sampler,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        ) {
            auto& overlay = render_units_.emplace_back(device);
            overlay.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                tex_man.request("asset/textures/lorem_ipsum.png", true)
                    ->image_view(),
                tex_man.request("asset/textures/white.png", true)->image_view(),
                sampler,
                desclayout,
                tex_man
            );
        }

        void record_render(const mirinae::WidgetRenderUniData& udata) override {
            for (auto& overlay : render_units_) {
                auto desc_main = overlay.get_desc_set(udata.frame_index_);
                vkCmdBindDescriptorSets(
                    udata.cmd_buf_,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    udata.pipe_layout_,
                    0,
                    1,
                    &desc_main,
                    0,
                    nullptr
                );

                vkCmdPushConstants(
                    udata.cmd_buf_,
                    udata.pipe_layout_,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    sizeof(overlay.push_const_),
                    &overlay.push_const_
                );

                vkCmdDraw(udata.cmd_buf_, 6, 1, 0, 0);
            }
        }

        void on_parent_resize(double width, double height) override {
            for (auto& overlay : render_units_) {
                overlay.ubuf_data_.set(
                    (width - 10 - 100) / width * 2 - 1,
                    (height - 10 - 100) / height * 2 - 1,
                    100 / width,
                    100 / height
                );

                overlay.push_const_.pos_offset = {
                    (width - 10 - 100) / width * 2 - 1,
                    (height - 10 - 100) / height * 2 - 1
                };
                overlay.push_const_.pos_scale = { 100 / width, 100 / height };
                overlay.push_const_.uv_offset = { 0, 0 };
                overlay.push_const_.uv_scale = { 1, 1 };

                for (size_t i = 0; i < overlay.ubuf_count(); ++i)
                    overlay.udpate_ubuf(i);
            }
        }

    private:
        std::vector<mirinae::OverlayRenderUnit> render_units_;
    };

}  // namespace


namespace {

    class FontLibrary {

    public:
        FontLibrary(mirinae::IFilesys& filesys) {
            const auto font_path = "asset/font/SeoulNamsanM.ttf";
            if (!filesys.read_file_to_vector(font_path, file_data_))
                throw std::runtime_error("failed to load font file");

            stbtt_InitFont(
                &font_,
                reinterpret_cast<const unsigned char*>(file_data_.data()),
                0
            );
        }

        const unsigned char* data() const { return font_.data; }

    private:
        stbtt_fontinfo font_;
        std::vector<uint8_t> file_data_;
    };


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
        void clear() { fill_size_ = 0; }

        size_t size() const { return fill_size_; }
        size_t capacity() const { return CAPACITY; }
        size_t remaining() const { return CAPACITY - fill_size_; }

        bool is_full() const { return fill_size_ >= CAPACITY; }
        bool is_empty() const { return fill_size_ == 0; }

        auto begin() const { return data_; }
        auto end() const { return data_ + fill_size_; }

    private:
        constexpr static size_t CAPACITY = 1024;
        uint32_t data_[CAPACITY];
        size_t fill_size_ = 0;
    };


    class TextBlocks {

    public:
        void append(const std::string_view str) {
            size_t start_index = 0;

            for (auto c : str) {
                this->last_valid_block().append(static_cast<uint32_t>(c));
            }
        }

        auto begin() const { return blocks_.begin(); }
        auto end() const { return blocks_.end(); }

    private:
        class Block {

        public:
            bool append(uint32_t value) {
                if ('\n' == value)
                    ++new_line_count_;

                return data_.push_back(value);
            }

            bool is_full() const { return data_.is_full(); }

            auto begin() const { return data_.begin(); }
            auto end() const { return data_.end(); }

        private:
            StaticVector data_;
            size_t new_line_count_ = 0;
        };

        Block& last_valid_block() {
            if (blocks_.empty())
                blocks_.emplace_back();
            if (blocks_.back().is_full())
                blocks_.emplace_back();

            return blocks_.back();
        }

        std::vector<Block> blocks_;
    };


    class TextRenderData {

    public:
        TextRenderData(mirinae::VulkanDevice& device) : render_unit_(device) {}

        void init_ascii(
            VkSampler sampler,
            FontLibrary& fonts,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        ) {
            constexpr int w = 256;
            constexpr int h = 128;
            std::array<uint8_t, w * h> temp_bitmap;

            stbtt_BakeFontBitmap(
                fonts.data(),
                0,
                TEXT_HEIGHT,
                temp_bitmap.data(),
                w,
                h,
                START_CHAR,
                END_CHAR - START_CHAR,
                char_baked_.data()
            );
            bitmap_.init(temp_bitmap.data(), w, h, 1);
            texture_ = tex_man.create_image("glyphs_ascii", bitmap_, false);

            render_unit_.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                tex_man.request("asset/textures/white.png", false)
                    ->image_view(),
                texture_->image_view(),
                sampler,
                desclayout,
                tex_man
            );
        }

        bool is_ready() const { return texture_ != nullptr; }

        VkDescriptorSet get_desc_set(size_t frame_index) {
            return render_unit_.get_desc_set(frame_index);
        }

        auto& get_char_info(size_t index) {
            return char_baked_.at(index - START_CHAR);
        }

        auto text_height() const { return TEXT_HEIGHT; }
        auto atlas_width() const { return texture_->width(); }
        auto atlas_height() const { return texture_->height(); }

    private:
        constexpr static int START_CHAR = 32;
        constexpr static int END_CHAR = 127;
        constexpr static int TEXT_HEIGHT = 22;

        std::array<stbtt_bakedchar, END_CHAR - START_CHAR> char_baked_;
        std::unique_ptr<mirinae::ITexture> texture_;
        mirinae::TImage2D<unsigned char> bitmap_;
        mirinae::OverlayRenderUnit render_unit_;
    };


    class TextBox : public mirinae::IWidget {

    public:
        TextBox(::TextRenderData& text_render_data)
            : text_render_data_(text_render_data) {}

        void add_text(const std::string_view str) { texts_.append(str); }

        void record_render(const mirinae::WidgetRenderUniData& udata) override {
            auto desc_main = text_render_data_.get_desc_set(udata.frame_index_);
            vkCmdBindDescriptorSets(
                udata.cmd_buf_,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                udata.pipe_layout_,
                0,
                1,
                &desc_main,
                0,
                nullptr
            );

            const sung::AABB2<double> widget_box(
                pos_.x, pos_.x + size_.x, pos_.y, pos_.y + size_.y
            );
            auto x_offset = pos_.x;
            auto y_offset = pos_.y + text_render_data_.text_height();

            for (auto& block : texts_) {
                for (auto c : block) {
                    if ('\n' == c) {
                        x_offset = pos_.x;
                        y_offset += text_render_data_.text_height() *
                                    line_spacing_;
                        continue;
                    }

                    const auto& char_info = text_render_data_.get_char_info(c);
                    const sung::AABB2<double> char_info_box(
                        char_info.x0, char_info.x1, char_info.y0, char_info.y1
                    );
                    const auto char_info_min = ::get_aabb_min(char_info_box);
                    const auto char_info_dim = ::get_aabb_dim(char_info_box);
                    const glm::dvec2 texture_dim(
                        text_render_data_.atlas_width(),
                        text_render_data_.atlas_height()
                    );

                    const sung::AABB2<double> glyph_box(
                        x_offset + char_info.xoff + scroll_.x,
                        x_offset + char_info.xoff + scroll_.x + char_info_dim.x,
                        y_offset + char_info.yoff + scroll_.y,
                        y_offset + char_info.yoff + scroll_.y + char_info_dim.y
                    );

                    mirinae::U_OverlayPushConst push_const;
                    sung::AABB2<double> clipped_glyph_box;
                    if (widget_box.make_intersection(
                            glyph_box, clipped_glyph_box
                        )) {
                        const auto clipped_glyph_area = clipped_glyph_box.area(
                        );
                        if (clipped_glyph_area <= 0.0) {
                            x_offset += char_info.xadvance;
                            continue;
                        } else if (clipped_glyph_area < glyph_box.area()) {
                            push_const.pos_offset = ::convert_screen_pos(
                                clipped_glyph_box.x_min(),
                                clipped_glyph_box.y_min(),
                                udata.width(),
                                udata.height()
                            );
                            push_const.pos_scale = ::convert_screen_offset(
                                clipped_glyph_box.width(),
                                clipped_glyph_box.height(),
                                udata.width(),
                                udata.height()
                            );
                            push_const.uv_scale =
                                char_info_dim *
                                ::get_aabb_dim(clipped_glyph_box) /
                                (::get_aabb_dim(glyph_box) * texture_dim);
                            const auto texture_space_offset =
                                (::get_aabb_min(clipped_glyph_box) -
                                 ::get_aabb_min(glyph_box)) *
                                char_info_dim / ::get_aabb_dim(glyph_box);
                            push_const.uv_offset = (char_info_min +
                                                    texture_space_offset) /
                                                   texture_dim;

                        } else {
                            push_const.pos_offset = ::convert_screen_pos(
                                glyph_box.x_min(),
                                glyph_box.y_min(),
                                udata.width(),
                                udata.height()
                            );
                            push_const.pos_scale = ::convert_screen_offset(
                                glyph_box.width(),
                                glyph_box.height(),
                                udata.width(),
                                udata.height()
                            );
                            push_const.uv_offset = char_info_min / texture_dim;
                            push_const.uv_scale = char_info_dim / texture_dim;
                        }
                    } else {
                        x_offset += char_info.xadvance;
                        continue;
                    }

                    push_const.color = { 1, 1, 1, 1 };

                    vkCmdPushConstants(
                        udata.cmd_buf_,
                        udata.pipe_layout_,
                        VK_SHADER_STAGE_VERTEX_BIT |
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                        0,
                        sizeof(push_const),
                        &push_const
                    );

                    vkCmdDraw(udata.cmd_buf_, 6, 1, 0, 0);
                    x_offset += char_info.xadvance;
                }
            }
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            if (e.action_ == mirinae::mouse::ActionType::down) {
                const sung::AABB2<double> bounding(
                    pos_.x, pos_.x + size_.x, pos_.y, pos_.y + size_.y
                );
                if (bounding.is_inside_cl(e.xpos_, e.ypos_)) {
                    owning_mouse_ = true;
                    last_mouse_pos_ = { e.xpos_, e.ypos_ };
                }
            } else if (e.action_ == mirinae::mouse::ActionType::up) {
                owning_mouse_ = false;
            } else if (e.action_ == mirinae::mouse::ActionType::move &&
                       owning_mouse_) {
                scroll_.x += e.xpos_ - last_mouse_pos_.x;
                scroll_.y += e.ypos_ - last_mouse_pos_.y;
                last_mouse_pos_ = { e.xpos_, e.ypos_ };
            }

            return owning_mouse_;
        }

        glm::dvec2 pos_{ 10, 10 };
        glm::dvec2 size_{ 512, 512 };
        glm::dvec2 scroll_{ 0, 0 };

    private:
        ::TextRenderData& text_render_data_;
        TextBlocks texts_;
        glm::dvec2 last_mouse_pos_;
        double line_spacing_ = 1.2;
        bool word_wrap_ = true;
        bool owning_mouse_ = false;
    };


    class DevConsole : public mirinae::IWidget {

    public:
        DevConsole(
            VkSampler sampler,
            ::TextRenderData& text_render_data,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        )
            : bg_img_(sampler, desclayout, tex_man, device)
            , text_box_(text_render_data) {
            text_box_.add_text("Hello, World!\n");
        }

        void record_render(const mirinae::WidgetRenderUniData& udata) override {
            if (hidden_)
                return;

            text_box_.record_render(udata);
            bg_img_.record_render(udata);
        }

        void hide(bool hidden) override { hidden_ = hidden; }

        bool hidden() const override { return hidden_; }

        void on_parent_resize(double width, double height) override {
            bg_img_.on_parent_resize(width, height);
            text_box_.on_parent_resize(width, height);
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            if (bg_img_.on_key_event(e))
                return true;
            if (text_box_.on_key_event(e))
                return true;

            return false;
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            if (bg_img_.on_mouse_event(e))
                return true;
            if (text_box_.on_mouse_event(e))
                return true;

            return false;
        }

    private:
        ImageView bg_img_;
        TextBox text_box_;
        bool hidden_ = false;
    };

}  // namespace


namespace mirinae {

    class OverlayManager::Impl {

    public:
        Impl(
            uint32_t win_width,
            uint32_t win_height,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        )
            : device_(device)
            , tex_man_(tex_man)
            , desclayout_(desclayout)
            , sampler_(device)
            , font_lib_(device.filesys())
            , text_render_data_(device)
            , wid_width_(win_width)
            , wid_height_(win_height) {
            SamplerBuilder sampler_builder;
            sampler_.reset(sampler_builder.build(device_));
        }

        DevConsole& get_or_create_dev_console() {
            if (auto w = get_widget_type<DevConsole>())
                return *w;

            auto w = std::make_unique<DevConsole>(
                sampler_.get(),
                this->ascii_ren_data(),
                desclayout_,
                tex_man_,
                device_
            );

            w->on_parent_resize(wid_width_, wid_height_);
            w->hide(true);
            widgets_.emplace_back(std::move(w));
            return *this->get_widget_type<DevConsole>();
        }

        template <typename TWidget>
        TWidget* get_widget_type() {
            for (auto& widget : widgets_) {
                if (auto casted = dynamic_cast<TWidget*>(widget.get()))
                    return casted;
            }

            return nullptr;
        }

        TextRenderData& ascii_ren_data() {
            if (!text_render_data_.is_ready()) {
                text_render_data_.init_ascii(
                    sampler_.get(), font_lib_, desclayout_, tex_man_, device_
                );
            }

            return text_render_data_;
        }

        mirinae::VulkanDevice& device_;
        mirinae::TextureManager& tex_man_;
        mirinae::DesclayoutManager& desclayout_;

        mirinae::Sampler sampler_;
        ::FontLibrary font_lib_;
        double wid_width_ = 0;
        double wid_height_ = 0;

        std::vector<std::unique_ptr<IWidget>> widgets_;

    private:
        ::TextRenderData text_render_data_;
    };


    OverlayManager::OverlayManager(
        uint32_t win_width,
        uint32_t win_height,
        mirinae::DesclayoutManager& desclayout,
        mirinae::TextureManager& tex_man,
        mirinae::VulkanDevice& device
    )
        : pimpl_(std::make_unique<Impl>(
              win_width, win_height, desclayout, tex_man, device
          )) {}

    OverlayManager::~OverlayManager() = default;

    void OverlayManager::record_render(
        size_t frame_index,
        VkCommandBuffer cmd_buf,
        VkPipelineLayout pipe_layout
    ) {
        WidgetRenderUniData udata;
        udata.screen_size_ = { pimpl_->wid_width_, pimpl_->wid_height_ };
        udata.frame_index_ = frame_index;
        udata.cmd_buf_ = cmd_buf;
        udata.pipe_layout_ = pipe_layout;

        for (auto& widget : pimpl_->widgets_) widget->record_render(udata);
    }

    void OverlayManager::on_fbuf_resize(uint32_t width, uint32_t height) {
        pimpl_->wid_width_ = width;
        pimpl_->wid_height_ = height;

        for (auto& widget : pimpl_->widgets_)
            widget->on_parent_resize(pimpl_->wid_width_, pimpl_->wid_height_);
    }

    bool OverlayManager::on_key_event(const mirinae::key::Event& e) {
        if (e.action_type == key::ActionType::up) {
            if (e.key == key::KeyCode::backquote) {
                auto& w = pimpl_->get_or_create_dev_console();
                w.hide(!w.hidden());
                return true;
            }
        }

        for (auto& widget : pimpl_->widgets_) {
            if (widget->on_key_event(e))
                return true;
        }

        return false;
    }

    bool OverlayManager::on_mouse_event(const mouse::Event& e) {
        for (auto& widget : pimpl_->widgets_) {
            if (widget->on_mouse_event(e))
                return true;
        }

        return false;
    }

    std::vector<std::unique_ptr<IWidget>>::iterator OverlayManager::begin() {
        return pimpl_->widgets_.begin();
    }

    std::vector<std::unique_ptr<IWidget>>::iterator OverlayManager::end() {
        return pimpl_->widgets_.end();
    }

}  // namespace mirinae
