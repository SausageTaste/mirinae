#pragma once

#include <vector>
#include <memory>

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
        virtual uint32_t value_type_size() const = 0;

    };


    template <typename T>
    class TImage2D : public IImage2D {

    public:
        bool init(const T* data, uint32_t width, uint32_t height, uint32_t channels) {
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
            return static_cast<uint32_t>(data_.size() * sizeof(T));
        }

        uint32_t width() const override {
            return width_;
        }
        uint32_t height() const override {
            return height_;
        }
        uint32_t channels() const override {
            return channels_;
        }
        uint32_t value_type_size() const override {
            return sizeof(T);
        }

        const T* get_texel_at_clamp(int x, int y) const {
            x = glm::clamp<int>(x, 0, width_ - 1);
            y = glm::clamp<int>(y, 0, height_ - 1);
            const auto index = (x + width_ * y) * channels_;
            return data_.data() + index;
        }

    private:
        std::vector<T> data_;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        uint32_t channels_ = 0;

    };


    std::unique_ptr<IImage2D> parse_image(const uint8_t* data, size_t data_size, bool force_rgba);

}
