#pragma once

#include <memory>


namespace mirinae {

    template <typename T>
    struct RenUnitHolder {
        template <typename U>
        U* ren_unit() {
            return dynamic_cast<U*>(ren_unit_.get());
        }

        template <typename U>
        const U* ren_unit() const {
            return dynamic_cast<const U*>(ren_unit_.get());
        }

        std::unique_ptr<T> ren_unit_;
    };

}  // namespace mirinae
