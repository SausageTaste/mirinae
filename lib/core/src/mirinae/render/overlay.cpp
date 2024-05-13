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

    class ImageView : public mirinae::IRectWidget {

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
                tex_man.request("asset/textures/black.png", true)->image_view(),
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

        void update_content(const mirinae::WindowDimInfo& wd) override {
            for (auto& overlay : render_units_) {
                overlay.push_const_.pos_offset = wd.pos_2_ndc(pos_);
                overlay.push_const_.pos_scale = wd.len_2_ndc(size_);
                overlay.push_const_.uv_offset = { 0, 0 };
                overlay.push_const_.uv_scale = { 1, 1 };
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
        void append(char c) {
            this->last_valid_block().append(static_cast<uint32_t>(c));
        }
        void append(uint32_t c) { this->last_valid_block().append(c); }
        void append(const std::string_view str) {
            for (auto c : str) {
                this->last_valid_block().append(static_cast<uint32_t>(c));
            }
        }

        void pop_back() {
            const auto block_count = blocks_.size();
            for (size_t i = 0; i < block_count; ++i) {
                auto& block = blocks_[block_count - i - 1];

                if (block.pop_back())
                    return;
                else
                    blocks_.pop_back();
            }
        }

        void clear() { blocks_.clear(); }

        std::string make_str() const {
            std::ostringstream oss;

            for (const auto& block : blocks_) {
                for (auto c : block) {
                    oss << static_cast<char>(c);
                }
            }

            return oss.str();
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

            bool pop_back() {
                if (data_.is_empty())
                    return false;

                if ('\n' == data_.end()[-1])
                    --new_line_count_;

                return data_.pop_back();
            }

            bool is_full() const { return data_.is_full(); }
            bool is_empty() const { return data_.is_empty(); }

            const uint32_t* begin() const { return data_.begin(); }
            const uint32_t* end() const { return data_.end(); }

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
            const auto i = index - START_CHAR;
            if (i >= char_baked_.size())
                return char_baked_['?' - START_CHAR];
            else
                return char_baked_.at(i);
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


    class TextBox : public mirinae::IRectWidget {

    public:
        TextBox(::TextRenderData& text_render_data)
            : text_render_data_(text_render_data) {}

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
                            push_const.pos_offset = udata.pos_2_ndc(
                                clipped_glyph_box.x_min(),
                                clipped_glyph_box.y_min()
                            );
                            push_const.pos_scale = udata.len_2_ndc(
                                clipped_glyph_box.width(),
                                clipped_glyph_box.height()
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
                            push_const.pos_offset = udata.pos_2_ndc(
                                glyph_box.x_min(), glyph_box.y_min()
                            );
                            push_const.pos_scale = udata.len_2_ndc(
                                glyph_box.width(), glyph_box.height()
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
            using mirinae::mouse::ActionType;

            if (e.action_ == ActionType::down) {
                if (this->is_inside_cl(e.xpos_, e.ypos_)) {
                    owning_mouse_ = true;
                    last_mouse_pos_ = { e.xpos_, e.ypos_ };
                }
            } else if (e.action_ == ActionType::up) {
                owning_mouse_ = false;
            } else if (e.action_ == ActionType::move && owning_mouse_) {
                if (enable_scroll_) {
                    scroll_.x += e.xpos_ - last_mouse_pos_.x;
                    scroll_.y += e.ypos_ - last_mouse_pos_.y;
                }
                last_mouse_pos_ = { e.xpos_, e.ypos_ };
            }

            return owning_mouse_;
        }

        std::string make_str() const { return texts_.make_str(); }

        void add_text(const char c) { texts_.append(c); }
        void add_text(const uint32_t c) { texts_.append(c); }
        void add_text(const std::string_view str) { texts_.append(str); }

        void remove_one_char() { texts_.pop_back(); }
        void clear_text() { texts_.clear(); }

        glm::dvec2 scroll_{ 0, 0 };
        bool enable_scroll_ = true;

    private:
        ::TextRenderData& text_render_data_;
        TextBlocks texts_;
        glm::dvec2 last_mouse_pos_;
        double line_spacing_ = 1.2;
        bool word_wrap_ = true;
        bool owning_mouse_ = false;
    };


    class LineEdit : public mirinae::IRectWidget {

    public:
        LineEdit(
            VkSampler sampler,
            ::TextRenderData& text_render_data,
            mirinae::DesclayoutManager& desclayout,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        )
            : bg_img_(sampler, desclayout, tex_man, device)
            , text_box_(text_render_data) {
            text_box_.scroll_ = { 0, 0 };
            text_box_.enable_scroll_ = false;
        }

        void record_render(const mirinae::WidgetRenderUniData& udata) override {
            if (hidden_)
                return;

            bg_img_.record_render(udata);
            text_box_.record_render(udata);
        }

        void update_content(const mirinae::WindowDimInfo& wd) override {
            bg_img_.pos_ = pos_;
            bg_img_.size_ = size_;
            bg_img_.update_content(wd);

            text_box_.pos_ = pos_;
            text_box_.size_ = size_;
            text_box_.update_content(wd);
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            if (e.key == mirinae::key::KeyCode::backspace) {
                if (e.action_type == mirinae::key::ActionType::down) {
                    text_box_.remove_one_char();
                }
            }

            return true;
        }

        bool on_text_event(uint32_t c) override {
            if (c == '`')
                return false;

            text_box_.add_text(c);
            return true;
        }

        void add_text(const std::string_view str) { text_box_.add_text(str); }

        std::string flush_str() {
            const auto str = text_box_.make_str();
            text_box_.clear_text();
            return str;
        }

    private:
        ImageView bg_img_;
        TextBox text_box_;
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
            : bg_img_text_box_(sampler, desclayout, tex_man, device)
            , text_box_(text_render_data)
            , line_edit_(
                  sampler, text_render_data, desclayout, tex_man, device
              ) {
            text_box_.pos_ = { 10, 10 };
            text_box_.size_ = { 500, 400 };
            text_box_.add_text("Hello, World!\n");

            bg_img_text_box_.pos_ = { 10, 10 };
            bg_img_text_box_.size_ = { 500, 400 };

            constexpr float LINE_EDIT_VER_MARGIN = 4;

            line_edit_.pos_ = { 10, 415 };
            line_edit_.size_ = { 500, text_render_data.text_height() };
        }

        void record_render(const mirinae::WidgetRenderUniData& udata) override {
            line_edit_.record_render(udata);
            bg_img_text_box_.record_render(udata);
            text_box_.record_render(udata);
        }

        void update_content(const mirinae::WindowDimInfo& wd) override {
            bg_img_text_box_.update_content(wd);
            text_box_.update_content(wd);
            line_edit_.update_content(wd);
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            if (e.key == mirinae::key::KeyCode::enter) {
                if (e.action_type == mirinae::key::ActionType::down) {
                    const auto line = line_edit_.flush_str();
                    if (!line.empty()) {
                        text_box_.add_text(line);
                        text_box_.add_text("\n");
                        spdlog::info("Console command: '{}'", line);
                    }
                }
            }

            if (line_edit_.on_key_event(e))
                return true;
            if (bg_img_text_box_.on_key_event(e))
                return true;
            if (text_box_.on_key_event(e))
                return true;

            return false;
        }

        bool on_text_event(uint32_t c) override {
            if (line_edit_.on_text_event(c))
                return true;
            if (bg_img_text_box_.on_text_event(c))
                return true;
            if (text_box_.on_text_event(c))
                return true;

            return false;
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            if (line_edit_.on_mouse_event(e))
                return true;
            if (bg_img_text_box_.on_mouse_event(e))
                return true;
            if (text_box_.on_mouse_event(e))
                return true;

            return false;
        }

        void hide(bool hidden) override { hidden_ = hidden; }

        bool hidden() const override { return hidden_; }

    private:
        ImageView bg_img_text_box_;
        TextBox text_box_;
        LineEdit line_edit_;
        bool hidden_ = false;
    };

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
            , win_dim_(win_width, win_height, 1) {
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

            w->update_content(win_dim_);
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

        mirinae::WindowDimInfo win_dim_;
        mirinae::Sampler sampler_;
        ::FontLibrary font_lib_;

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
        udata.win_dim_ = pimpl_->win_dim_;
        udata.frame_index_ = frame_index;
        udata.cmd_buf_ = cmd_buf;
        udata.pipe_layout_ = pipe_layout;

        for (auto& widget : pimpl_->widgets_) {
            if (widget->hidden())
                continue;
            widget->record_render(udata);
        }
    }

    void OverlayManager::on_fbuf_resize(uint32_t width, uint32_t height) {
        pimpl_->win_dim_ = WindowDimInfo{ static_cast<double>(width),
                                          static_cast<double>(height),
                                          1.0 };

        for (auto& widget : pimpl_->widgets_) {
            if (widget->hidden())
                continue;
            widget->update_content(pimpl_->win_dim_);
        }
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
            if (widget->hidden())
                continue;
            if (widget->on_key_event(e))
                return true;
        }

        return false;
    }

    bool OverlayManager::on_text_event(uint32_t c) {
        for (auto& widget : pimpl_->widgets_) {
            if (widget->hidden())
                continue;
            if (widget->on_text_event(c))
                return true;
        }

        return false;
    }

    bool OverlayManager::on_mouse_event(const mouse::Event& e) {
        for (auto& widget : pimpl_->widgets_) {
            if (widget->hidden())
                continue;
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
