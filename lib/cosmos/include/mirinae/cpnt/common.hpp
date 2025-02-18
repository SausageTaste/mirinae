#pragma once

#include <memory>


namespace mirinae {

    template <typename T>
    struct RenUnitHolder {
        template <typename U>
        U* ren_unit() {
            return dynamic_cast<U*>(ren_unit_.get());
        }

        std::unique_ptr<T> ren_unit_;
    };

}  // namespace mirinae
