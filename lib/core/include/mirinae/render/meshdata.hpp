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
        glm::vec3 tangent_;
        glm::vec2 texcoord_;
    };


    struct VertexSkinned {
        glm::ivec4 joint_indices_;
        glm::vec4 joint_weights_;
        glm::vec3 pos_;
        glm::vec3 normal_;
        glm::vec3 tangent_;
        glm::vec2 uv_;
    };


    class VerticesStaticPair {

    public:
        void append_vertex(const VertexStatic& vertex);

        std::vector<VertexStatic> vertices_;
        std::vector<VertIndexType_t> indices_;
    };


    class VerticesSkinnedPair {

    public:
        void append_vertex(const VertexSkinned& vertex);

        std::vector<VertexSkinned> vertices_;
        std::vector<VertIndexType_t> indices_;
    };


    std::optional<VerticesStaticPair> parse_dmd_static(
        const uint8_t* const file_content, const size_t content_size
    );

}  // namespace mirinae
