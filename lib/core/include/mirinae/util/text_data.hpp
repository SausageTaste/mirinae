#pragma once

#include <memory>
#include <string>
#include <string_view>


namespace mirinae {

    class ITextStream {

    public:
        virtual ~ITextStream() = default;

        virtual bool append(char) = 0;
        virtual bool append(char32_t) = 0;
        virtual bool append(std::string_view) = 0;
    };


    class ITextData : public ITextStream {

    public:
        virtual std::string make_str() const = 0;
        virtual std::u32string make_str32() const = 0;

        virtual bool pop_back() = 0;
        virtual bool clear() = 0;
    };


    std::unique_ptr<ITextData> create_text_blocks();

}  // namespace mirinae
