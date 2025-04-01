#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


namespace mirinae {

    class DebugMesh {

    public:
        struct Vertex {
            glm::vec3 pos_;  // World pos
        };

        std::vector<Vertex> vtx_;
        std::vector<uint32_t> idx_;
    };


    class IDebugRen {

    public:
        virtual ~IDebugRen() = default;

        virtual void tri(
            const glm::vec3& p0,
            const glm::vec3& p1,
            const glm::vec3& p2,
            const glm::vec4& color
        ) = 0;

        // You must keep the mesh object around!
        // The renderer won't make a copy of it.
        virtual void mesh(const DebugMesh& mesh) = 0;
    };

}  // namespace mirinae
