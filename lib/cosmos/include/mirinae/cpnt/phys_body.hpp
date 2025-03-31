#pragma once

#include <glm/vec3.hpp>

#include "mirinae/cpnt/common.hpp"


namespace mirinae {

    struct ICharacterPhysBody {
        virtual ~ICharacterPhysBody() = default;
    };

}  // namespace mirinae


namespace mirinae::cpnt {

    class CharacterPhys : public RenUnitHolder<ICharacterPhysBody> {

    public:
        double height_ = 1;
        double radius_ = 1;
    };

}  // namespace mirinae::cpnt
