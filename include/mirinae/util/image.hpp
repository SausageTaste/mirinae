#pragma once

#include <vector>


namespace mirinae {

    class Image2D {

    public:
        Image2D() = default;

        Image2D(const uint8_t* data, unsigned width, unsigned height, unsigned channels, unsigned value_size) {
            this->init(data, width, height, channels, value_size);
        }

        void init(const uint8_t* data, unsigned width, unsigned height, unsigned channels, unsigned value_size) {
            if (nullptr == data)
                return;

            const auto data_size = width * height * channels * value_size;
            this->data_.assign(data, data + data_size);

            this->width_ = width;
            this->height_ = height;
            this->channels_ = channels;
            this->value_size_ = value_size;
        }

        void destroy() {
            data_.clear();
        }

        bool is_ready() const {
            return !data_.empty();
        }

        const uint8_t* data() const {
            return data_.data();
        }
        unsigned width() const {
            return width_;
        }
        unsigned height() const {
            return height_;
        }
        unsigned channels() const {
            return channels_;
        }

        const uint8_t* get_texel_at(unsigned x, unsigned y) const {
            const auto index = (x + width_ * y) * channels_;
            return data_.data() + index;
        }

    private:
        std::vector<uint8_t> data_;
        unsigned width_ = 0;
        unsigned height_ = 0;
        unsigned channels_ = 0;
        unsigned value_size_ = 0;

    };

}
