#include "mirinae/render/overlay.hpp"

#include <sstream>
#include <string_view>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <sung/general/aabb.hpp>


namespace {

    template <typename T>
    class ScreenOffset {

    public:
        ScreenOffset(T x, T y)
            : x_(x)
            , y_(y)
        {}

        ScreenOffset operator+(const ScreenOffset& rhs) const {
            return ScreenOffset(x_ + rhs.x_, y_ + rhs.y_);
        }
        ScreenOffset operator-(const ScreenOffset& rhs) const {
            return ScreenOffset(x_ - rhs.x_, y_ - rhs.y_);
        }

        glm::tvec2<T> convert(T screen_width, T screen_height) const {
            return glm::tvec2<T>(x_ / screen_width * 2, y_ / screen_height * 2);
        }

    public:
        T x_ = 0;
        T y_ = 0;

    };


    template <typename T>
    class ScreenPos {

    public:
        ScreenPos(T x, T y)
            : x_(x)
            , y_(y)
        {}

        ScreenPos operator+(const ScreenOffset<T>& rhs) const {
            return ScreenPos(x_ + rhs.x_, y_ + rhs.y_);
        }
        ScreenOffset<T> operator-(const ScreenPos& rhs) const {
            return ScreenOffset(x_ - rhs.x_, y_ - rhs.y_);
        }

        glm::tvec2<T> convert(T screen_width, T screen_height) const {
            return glm::tvec2<T>(x_ / screen_width * 2 - 1, y_ / screen_height * 2 - 1);
        }

    public:
        T x_ = 0;
        T y_ = 0;

    };

}


namespace {

    class ImageView : public mirinae::IWidget {

    public:
        ImageView(VkSampler sampler, mirinae::DesclayoutManager& desclayout, mirinae::TextureManager& tex_man, mirinae::VulkanDevice& device) {
            auto& overlay = render_units_.emplace_back(device);
            overlay.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                tex_man.request("asset/textures/lorem_ipsum.png")->image_view(),
                tex_man.request("asset/textures/white.png")->image_view(),
                sampler,
                desclayout,
                tex_man
            );
        }

        void record_render(size_t frame_index, VkCommandBuffer cmd_buf, VkPipelineLayout pipe_layout) override {
            for (auto& overlay : render_units_) {
                auto desc_main = overlay.get_desc_set(frame_index);
                vkCmdBindDescriptorSets(
                    cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipe_layout,
                    0,
                    1, &desc_main,
                    0, nullptr
                );

                vkCmdDraw(cmd_buf, 6, 1, 0, 0);
            }
        }

        void on_parent_resize(double width, double height) override {
            for (auto& overlay : render_units_) {
                overlay.ubuf_data_.offset().x = (width - 10 - 100) / width * 2 - 1;
                overlay.ubuf_data_.offset().y = (height - 10 - 100) / height * 2 - 1;
                overlay.ubuf_data_.size().x = 100 / width;
                overlay.ubuf_data_.size().y = 100 / height;

                for (size_t i = 0; i < overlay.ubuf_count(); ++i)
                    overlay.udpate_ubuf(i);
            }
        }

    private:
        std::vector<mirinae::OverlayRenderUnit> render_units_;

    };

}


namespace {

    class FontLibrary {

    public:
        FontLibrary(mirinae::TextureManager& tex_man, mirinae::VulkanDevice& device) {
            if (!device.filesys().read_file_to_vector("asset/font/SeoulNamsanM.ttf", file_data_))
                throw std::runtime_error("failed to load font file");

            stbtt_InitFont(&font_, reinterpret_cast<const unsigned char*>(file_data_.data()), 0);

            constexpr int w = 256;
            constexpr int h = 128;
            std::array<uint8_t, w * h> temp_bitmap;
            stbtt_BakeFontBitmap(font_.data, 0, TEXT_HEIGHT, temp_bitmap.data(), w, h, START_CHAR, END_CHAR - START_CHAR, char_baked_.data());
            bitmap_.init(temp_bitmap.data(), w, h, 1);
            texture_ = tex_man.create_image("glyphs_ascii", bitmap_, false);
        }

        auto& ascii_texture() const { return *texture_; }

        const stbtt_bakedchar& get_char_info(char c) const {
            return char_baked_.at(c - START_CHAR);
        }

        double text_height() const { return TEXT_HEIGHT; }

    private:
        constexpr static int START_CHAR = 32;
        constexpr static int END_CHAR = 127;
        constexpr static int TEXT_HEIGHT = 22;

        stbtt_fontinfo font_;
        std::vector<uint8_t> file_data_;
        std::array<stbtt_bakedchar, END_CHAR - START_CHAR> char_baked_;
        mirinae::TImage2D<unsigned char> bitmap_;
        std::unique_ptr<mirinae::ITexture> texture_;

    };


    class TextWidget : public mirinae::IWidget {

    public:
        TextWidget(VkSampler sampler, FontLibrary& fonts, mirinae::DesclayoutManager& desclayout, mirinae::TextureManager& tex_man, mirinae::VulkanDevice& device)
            : glyphs_(fonts)
        {
            auto& overlay = render_units_.emplace_back(device);
            overlay.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                tex_man.request("asset/textures/white.png")->image_view(),
                fonts.ascii_texture().image_view(),
                sampler,
                desclayout,
                tex_man
            );
        }

        void record_render(size_t frame_index, VkCommandBuffer cmd_buf, VkPipelineLayout pipe_layout) override {
            for (auto& overlay : render_units_) {
                auto desc_main = overlay.get_desc_set(frame_index);
                vkCmdBindDescriptorSets(
                    cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipe_layout,
                    0,
                    1, &desc_main,
                    0, nullptr
                );

                std::string str = "Hello, World!\nThis is Sungmin Woo.";
                float x_offset = 5;
                float y_offset = 32;
                for (auto& c : str) {
                    if ('\n' == c) {
                        x_offset = 5;
                        y_offset += 32;
                        continue;
                    }

                    const auto char_info = glyphs_.get_char_info(c);

                    ScreenPos<float> pos0(x_offset + char_info.xoff, y_offset + char_info.yoff);
                    ScreenPos<float> pos1(pos0.x_ + char_info.x1 - char_info.x0, pos0.y_ + char_info.y1 - char_info.y0);
                    auto offset = pos1 - pos0;

                    mirinae::U_OverlayPushConst push_const;
                    push_const.pos_offset = pos0.convert(width_, height_);
                    push_const.pos_scale = offset.convert(width_, height_);
                    push_const.uv_offset = glm::vec2(char_info.x0, char_info.y0) / 256.0f;
                    push_const.uv_scale = glm::vec2(char_info.x1 - char_info.x0, char_info.y1 - char_info.y0) / 256.0f;
                    vkCmdPushConstants(
                        cmd_buf,
                        pipe_layout,
                        VK_SHADER_STAGE_VERTEX_BIT,
                        0,
                        sizeof(push_const),
                        &push_const
                    );

                    vkCmdDraw(cmd_buf, 6, 1, 0, 0);
                    x_offset += char_info.xadvance;
                }
            }
        }

        void on_parent_resize(double width, double height) override {
            width_ = static_cast<float>(width);
            height_ = static_cast<float>(height);

            for (auto& overlay : render_units_) {
                overlay.ubuf_data_.offset().x = (width - 10.0 - 512.0) / width * 2.0 - 1.0;
                overlay.ubuf_data_.offset().y = (height - 10.0 - 512.0) / height * 2.0 - 1.0;
                overlay.ubuf_data_.size().x = 512.0 / width;
                overlay.ubuf_data_.size().y = 512.0 / height;

                for (size_t i = 0; i < overlay.ubuf_count(); ++i)
                    overlay.udpate_ubuf(i);
            }
        }

        std::vector<mirinae::OverlayRenderUnit> render_units_;
        ::FontLibrary& glyphs_;
        float width_;
        float height_;

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
        void clear() {
            fill_size_ = 0;
        }

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
                return data_.push_back(value);
            }
            size_t append(const std::string_view str) {
                size_t added_count = 0;
                for (auto c : str) {
                    if (data_.push_back(static_cast<uint32_t>(c)))
                        ++added_count;
                    else
                        return added_count;
                }
            }

            bool is_full() const { return data_.is_full(); }

            auto begin() const { return data_.begin(); }
            auto end() const { return data_.end(); }

        private:
            StaticVector data_;

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


    class TextBox : public mirinae::IWidget {

    public:
        TextBox(VkSampler sampler, FontLibrary& fonts, mirinae::DesclayoutManager& desclayout, mirinae::TextureManager& tex_man, mirinae::VulkanDevice& device)
            : glyphs_(fonts)
            , render_unit_(device)
        {
            render_unit_.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                tex_man.request("asset/textures/white.png")->image_view(),
                fonts.ascii_texture().image_view(),
                sampler,
                desclayout,
                tex_man
            );
        }

        void add_text(const std::string_view str) {
            texts_.append(str);
        }

        void record_render(size_t frame_index, VkCommandBuffer cmd_buf, VkPipelineLayout pipe_layout) override {
            auto desc_main = render_unit_.get_desc_set(frame_index);
            vkCmdBindDescriptorSets(
                cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipe_layout,
                0,
                1, &desc_main,
                0, nullptr
            );

            const auto bound0 = pos_;
            const auto bound1 = pos_ + size_;

            auto x_offset = pos_.x_;
            auto y_offset = pos_.y_ + glyphs_.text_height();

            for (auto& block : texts_) {
                for (auto c : block) {
                    if ('\n' == c) {
                        x_offset = pos_.x_;
                        y_offset += glyphs_.text_height() * line_spacing_;
                        continue;
                    }

                    const auto& char_info = glyphs_.get_char_info(c);
                    const auto pos0 = ScreenPos<double>(x_offset + char_info.xoff, y_offset + char_info.yoff) + scroll_;
                    const auto pos1 = ScreenPos<double>(pos0.x_ + char_info.x1 - char_info.x0, pos0.y_ + char_info.y1 - char_info.y0);
                    auto dimensions = pos1 - pos0;

                    if (pos1.x_ <= bound0.x_) {
                        x_offset += char_info.xadvance;
                        continue;
                    }
                    if (pos1.y_ <= bound0.y_) {
                        x_offset += char_info.xadvance;
                        continue;
                    }
                    if (pos0.x_ >= bound1.x_)
                        continue;
                    if (pos0.y_ >= bound1.y_)
                        return;

                    glm::vec2 texture_dim(glyphs_.ascii_texture().width(), glyphs_.ascii_texture().height());

                    mirinae::U_OverlayPushConst push_const;
                    push_const.pos_offset = pos0.convert(screen_width_, screen_height_);
                    push_const.pos_scale = dimensions.convert(screen_width_, screen_height_);
                    push_const.uv_offset = glm::vec2(char_info.x0, char_info.y0) / texture_dim;
                    push_const.uv_scale = glm::vec2(char_info.x1 - char_info.x0, char_info.y1 - char_info.y0) / texture_dim;
                    vkCmdPushConstants(
                        cmd_buf,
                        pipe_layout,
                        VK_SHADER_STAGE_VERTEX_BIT,
                        0,
                        sizeof(push_const),
                        &push_const
                    );

                    vkCmdDraw(cmd_buf, 6, 1, 0, 0);
                    x_offset += char_info.xadvance;
                }
            }
        }

        void on_parent_resize(double width, double height) override {
            screen_width_ = static_cast<float>(width);
            screen_height_ = static_cast<float>(height);
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            if (e.action_ == mirinae::mouse::ActionType::down) {
                const sung::AABB2<double> bounding(pos_.x_, pos_.x_ + size_.x_, pos_.y_, pos_.y_ + size_.y_);
                if (bounding.is_contacting(e.xpos_, e.ypos_)) {
                    owning_mouse_ = true;
                    last_mouse_pos_ = { e.xpos_, e.ypos_ };
                }
            }
            else if (e.action_ == mirinae::mouse::ActionType::up) {
                owning_mouse_ = false;
            }
            else if (e.action_ == mirinae::mouse::ActionType::move && owning_mouse_) {
                scroll_.x_ += e.xpos_ - last_mouse_pos_.x;
                scroll_.y_ += e.ypos_ - last_mouse_pos_.y;
                last_mouse_pos_ = { e.xpos_, e.ypos_ };
            }

            return true;
        }

        ::ScreenPos<double> pos_{ 10, 10 };
        ::ScreenOffset<double> size_{ 512, 512 };
        ::ScreenOffset<double> scroll_{ 0, 0 };

    private:
        ::FontLibrary& glyphs_;
        mirinae::OverlayRenderUnit render_unit_;
        TextBlocks texts_;
        glm::dvec2 last_mouse_pos_;
        double screen_width_;
        double screen_height_;
        double line_spacing_ = 1.2;
        bool word_wrap_ = true;
        bool owning_mouse_ = false;

    };

}


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
            , font_lib_(tex_man, device)
            , wid_width_(win_width)
            , wid_height_(win_height)
        {
            SamplerBuilder sampler_builder;
            sampler_.reset(sampler_builder.build(device_));
        }

        mirinae::VulkanDevice& device_;
        mirinae::TextureManager& tex_man_;
        mirinae::DesclayoutManager& desclayout_;
        mirinae::Sampler sampler_;
        ::FontLibrary font_lib_;
        double wid_width_ = 0;
        double wid_height_ = 0;

        std::vector<std::unique_ptr<IWidget>> widgets_;

    };


    OverlayManager::OverlayManager(
        uint32_t win_width,
        uint32_t win_height,
        mirinae::DesclayoutManager& desclayout,
        mirinae::TextureManager& tex_man,
        mirinae::VulkanDevice& device
    )
        : pimpl_(std::make_unique<Impl>(win_width, win_height, desclayout, tex_man, device))
    {

    }

    OverlayManager::~OverlayManager() = default;

    void OverlayManager::on_fbuf_resize(uint32_t width, uint32_t height) {
        pimpl_->wid_width_ = width;
        pimpl_->wid_height_ = height;

        for (auto& widget : pimpl_->widgets_)
            widget->on_parent_resize(pimpl_->wid_width_, pimpl_->wid_height_);
    }

    bool OverlayManager::on_key_event(const mirinae::key::Event& e) {
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

    void OverlayManager::add_widget_test() {
        auto widget = std::make_unique<::TextBox>(pimpl_->sampler_.get(), pimpl_->font_lib_, pimpl_->desclayout_, pimpl_->tex_man_, pimpl_->device_);
        widget->on_parent_resize(pimpl_->wid_width_, pimpl_->wid_height_);

        for (int i = 0; i < 100; ++i) {
            widget->add_text("Hello, World! This is Sungmin Woo. Nice to meet you! I wish you a good day.\n");
        }

        pimpl_->widgets_.emplace_back(std::move(widget));
    }

    std::vector<std::unique_ptr<IWidget>>::iterator OverlayManager::begin() {
        return pimpl_->widgets_.begin();
    }

    std::vector<std::unique_ptr<IWidget>>::iterator OverlayManager::end() {
        return pimpl_->widgets_.end();
    }

}
