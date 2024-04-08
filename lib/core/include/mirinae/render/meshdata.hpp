#pragma once

#include <optional>
#include <vector>

#include "mirinae/util/include_glm.hpp"


namespace mirinae {

    using VertIndexType_t = uint16_t;


    struct VertexStatic {
        VertexStatic() = default;
        VertexStatic(
            const glm::vec3& pos,
            const glm::vec2& texcoord,
            const glm::vec3& normal
        )
            : pos_(pos), normal_(normal), texcoord_(texcoord) {}

        glm::vec3 pos_;
        glm::vec3 normal_;
        glm::vec2 texcoord_;
    };


    class VerticesStaticPair {

    public:
        void append_vertex(const VertexStatic& vertex);

        std::vector<VertexStatic> vertices_;
        std::vector<VertIndexType_t> indices_;
    };


    std::optional<VerticesStaticPair> parse_dmd_static(
        const uint8_t* const file_content, const size_t content_size
    );

}  // namespace mirinae
