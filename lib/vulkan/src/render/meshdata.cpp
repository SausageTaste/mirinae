#include "mirinae/render/meshdata.hpp"

#include <spdlog/spdlog.h>

#include <daltools/dmd/parser.h>


// VerticesStaticPair
namespace mirinae {

    void VerticesStaticPair::append_vertex(const VertexStatic& vertex) {
        const auto cur_index = vertices_.size();
        if ((std::numeric_limits<VertIndexType_t>::max)() < cur_index)
            spdlog::warn("Index overflow: {}", cur_index);

        indices_.push_back(static_cast<uint16_t>(vertices_.size()));
        vertices_.push_back(vertex);
    }

    void VerticesSkinnedPair::append_vertex(const VertexSkinned& vertex) {
        const auto cur_index = vertices_.size();
        if ((std::numeric_limits<VertIndexType_t>::max)() < cur_index)
            spdlog::warn("Index overflow: {}", cur_index);

        indices_.push_back(static_cast<uint16_t>(vertices_.size()));
        vertices_.push_back(vertex);
    }

}  // namespace mirinae


namespace mirinae {

    std::optional<VerticesStaticPair> parse_dmd_static(
        const uint8_t* const file_content, const size_t content_size
    ) {
        dal::parser::Model parsed_model;
        const auto parse_result = dal::parser::parse_dmd(
            parsed_model, file_content, content_size
        );
        if (dal::parser::ModelParseResult::success != parse_result) {
            spdlog::error(
                "Failed to parse dmd file: {}", static_cast<int>(parse_result)
            );
            return std::nullopt;
        }

        if (!parsed_model.units_straight_joint_.empty())
            spdlog::warn(
                "Parsing static DMD model but skinned straight mesh found"
            );
        if (!parsed_model.units_indexed_joint_.empty())
            spdlog::warn(
                "Parsing static DMD model but skinned indexed mesh found"
            );
        if (!parsed_model.units_straight_.empty())
            spdlog::warn(
                "Parsing static DMD model that has straight mesh, which will "
                "cause performance issue"
            );

        VerticesStaticPair output;

        for (const auto& unit : parsed_model.units_straight_) {
            const auto vert_count = unit.mesh_.vertices_.size() / 3;
            for (size_t i = 0; i < vert_count; ++i) {
                VertexStatic vert;
                vert.pos_.x = unit.mesh_.vertices_[i * 3 + 0];
                vert.pos_.y = unit.mesh_.vertices_[i * 3 + 1];
                vert.pos_.z = unit.mesh_.vertices_[i * 3 + 2];
                vert.normal_.x = unit.mesh_.normals_[i * 3 + 0];
                vert.normal_.y = unit.mesh_.normals_[i * 3 + 1];
                vert.normal_.z = unit.mesh_.normals_[i * 3 + 2];
                vert.texcoord_.x = unit.mesh_.uv_coordinates_[i * 2 + 0];
                vert.texcoord_.y = unit.mesh_.uv_coordinates_[i * 2 + 1];
                output.append_vertex(vert);
            }
        }

        for (const auto& unit : parsed_model.units_indexed_) {
            for (const auto index : unit.mesh_.indices_) {
                if ((std::numeric_limits<VertIndexType_t>::max)() < index)
                    spdlog::warn("Index overflow: {}", index);

                output.indices_.push_back(index);
            }

            for (const auto& vert : unit.mesh_.vertices_) {
                output.vertices_.emplace_back(
                    vert.pos_, vert.uv_, vert.normal_
                );
            }
        }

        return output;
    }

}  // namespace mirinae
