#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


namespace mirinae {

    class IDebugRen {

    public:
        virtual ~IDebugRen() = default;

        virtual void tri(
            const glm::vec3& p0,
            const glm::vec3& p1,
            const glm::vec3& p2,
            const glm::vec4& color
        ) = 0;
    };

}  // namespace mirinae
