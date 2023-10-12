#pragma once

#include <vector>

#include "include_glm.hpp"


namespace mirinae {

    class IImage2D {

    public:
        virtual ~IImage2D() = default;

        virtual void destroy() = 0;
        virtual bool is_ready() const = 0;

        virtual const uint8_t* data() const = 0;
        virtual uint32_t data_size() const = 0;

        virtual uint32_t width() const = 0;
        virtual uint32_t height() const = 0;
        virtual uint32_t channels() const = 0;

    };


    template <typename T>
    class TImage2D : public IImage2D {

    public:
        bool init(const T* data, unsigned width, unsigned height, unsigned channels) {
            if (nullptr == data)
                return false;

            const auto data_size = width * height * channels;
            this->data_.assign(data, data + data_size);

            this->width_ = width;
            this->height_ = height;
            this->channels_ = channels;
            return true;
        }

        void destroy() override {
            data_.clear();
        }

        bool is_ready() const override {
            if (data_.empty())
                return false;
            if (0 == width_ || 0 == height_ || 0 == channels_)
                return false;
            return true;
        }

        const uint8_t* data() const override {
            return reinterpret_cast<const uint8_t*>(data_.data());
        }

        uint32_t data_size() const override {
            return data_.size() * sizeof(T);
        }

        unsigned width() const override {
            return width_;
        }
        unsigned height() const override {
            return height_;
        }
        unsigned channels() const override {
            return channels_;
        }

        const T* get_texel_at_clamp(int x, int y) const {
            x = glm::clamp<int>(x, 0, width_ - 1);
            y = glm::clamp<int>(y, 0, height_ - 1);
            const auto index = (x + width_ * y) * channels_;
            return data_.data() + index;
        }

    private:
        std::vector<T> data_;
        unsigned width_ = 0;
        unsigned height_ = 0;
        unsigned channels_ = 0;

    };

}
