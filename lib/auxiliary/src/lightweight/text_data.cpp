#include "mirinae/lightweight/text_data.hpp"

#include <sstream>
#include <vector>

#include <uni_algo/conv.h>


// TextBlocks
namespace {

    template <typename T>
    class StaticVector {

    public:
        T& operator[](size_t index) { return data_[index]; }
        const T& operator[](size_t index) const { return data_[index]; }

        bool push_back(T value) {
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

        const T* begin() const { return data_; }
        const T* end() const { return data_ + fill_size_; }

    private:
        constexpr static size_t CAPACITY = 1024;
        T data_[CAPACITY];
        size_t fill_size_ = 0;
    };


    class TextBlock {

    public:
        bool append(char32_t value) {
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

        const char32_t* begin() const { return data_.begin(); }

        const char32_t* end() const { return data_.end(); }

    private:
        StaticVector<char32_t> data_;
        size_t new_line_count_ = 0;
    };


    class TextBlocks : public mirinae::ITextData {

    public:
        bool append(char c) override {
            this->last_valid_block().append(static_cast<char32_t>(c));
            return true;
        }

        bool append(char32_t c) override {
            this->last_valid_block().append(c);
            return true;
        }

        bool append(const std::string_view str) override {
            const auto str32 = una::utf8to32<char, char32_t>(str);
            for (auto c : str32) {
                this->last_valid_block().append(c);
            }
            return true;
        }

        bool pop_back() override {
            const auto block_count = blocks_.size();
            for (size_t i = 0; i < block_count; ++i) {
                auto& block = blocks_[block_count - i - 1];

                if (block.pop_back())
                    return true;
                else
                    blocks_.pop_back();
            }

            return true;
        }

        bool clear() override {
            blocks_.clear();
            return true;
        }

        std::string make_str() const override {
            const auto str32 = this->make_str32();
            return una::utf32to8(str32);
        }

        std::u32string make_str32() const override {
            std::u32string output;

            for (const auto& block : blocks_) {
                for (auto c : block) {
                    output += c;
                }
            }

            return output;
        }

    private:
        TextBlock& last_valid_block() {
            if (blocks_.empty())
                blocks_.emplace_back();
            if (blocks_.back().is_full())
                blocks_.emplace_back();

            return blocks_.back();
        }

        std::vector<TextBlock> blocks_;
    };


}  // namespace


namespace mirinae {

    std::unique_ptr<ITextData> create_text_blocks() {
        return std::make_unique<::TextBlocks>();
    }

}  // namespace mirinae
