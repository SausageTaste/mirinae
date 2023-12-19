#pragma once

#include <vector>

#include <spdlog/spdlog.h>

#include "mirinae/util/include_glm.hpp"


namespace mirinae {

    using VertIndexType_t = uint16_t;


    struct VertexStatic {
        VertexStatic(const glm::vec3& pos, const glm::vec2& texcoord, const glm::vec3& normal)
            : pos_(pos)
            , normal_(normal)
            , texcoord_(texcoord)
        {

        }

        glm::vec3 pos_;
        glm::vec3 normal_;
        glm::vec2 texcoord_;
    };


    class VerticesStaticPair {

    public:
        void append_vertex(const VertexStatic& vertex) {
            const auto cur_index = vertices_.size();
            if ((std::numeric_limits<VertIndexType_t>::max)() < cur_index) {
                throw std::runtime_error("too many vertices");
            }

            indices_.push_back(static_cast<uint16_t>(vertices_.size()));
            vertices_.push_back(vertex);
        }

        std::vector<VertexStatic> vertices_;
        std::vector<VertIndexType_t> indices_;

    };

}
