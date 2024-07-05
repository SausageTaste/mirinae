#pragma once


namespace mirinae {

    template <typename T, typename Tag>
    class StrongType {

    public:
        StrongType() = default;

        StrongType(const StrongType& rhs) : value_(rhs.value_) {}

        explicit StrongType(const T& value) : value_(value) {}

        StrongType& operator=(const StrongType& rhs) {
            value_ = rhs.value_;
            return *this;
        }

        StrongType& operator=(const T& value) {
            value_ = value;
            return *this;
        }

        // Implicit conversion operator
        operator T() const { return value_; }

        T get() const { return value_; }
        void set(const T& value) { value_ = value; }

    private:
        T value_;
    };

}  // namespace mirinae
