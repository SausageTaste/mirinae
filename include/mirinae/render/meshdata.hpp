#pragma once

#include <array>

#include <glm/glm.hpp>


namespace mirinae {

    class VertexStatic {

    public:
        VertexStatic() = default;

        VertexStatic(float x_pos, float y_pos, float z_pos, float x_normal, float y_normal, float z_normal, float x_uv, float y_uv) {
            this->pos() = glm::vec3{ x_pos, y_pos, z_pos };
            this->normal() = glm::vec3{ x_normal, y_normal, z_normal };
            this->uv() = glm::vec2{ x_uv, y_uv };
        }

        constexpr static size_t data_size() {
            constexpr auto a = VALUE_COUNT * sizeof(float);
            static_assert(a == sizeof(VertexStatic));
            return a;
        }

        glm::vec3& pos() {
            static_assert(sizeof(glm::vec3) == sizeof(float) * 3);
            return *reinterpret_cast<glm::vec3*>(values.data());
        }
        glm::vec3& normal() {
            static_assert(sizeof(glm::vec3) == sizeof(float) * 3);
            return *reinterpret_cast<glm::vec3*>(values.data() + 3);
        }
        glm::vec2& uv() {
            static_assert(sizeof(glm::vec2) == sizeof(float) * 2);
            return *reinterpret_cast<glm::vec2*>(values.data() + 6);
        }

    private:
        // Pos(3), normal(3), uv(2)
        constexpr static int VALUE_COUNT = 3 + 3 + 2;
        std::array<float, VALUE_COUNT> values;

    };

}
