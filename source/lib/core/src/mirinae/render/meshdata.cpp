#include "mirinae/render/meshdata.hpp"

#include <spdlog/spdlog.h>

#include <daltools/model_parser.h>


// VerticesStaticPair
namespace mirinae {

    void VerticesStaticPair::append_vertex(const VertexStatic& vertex) {
        const auto cur_index = vertices_.size();
        if ((std::numeric_limits<VertIndexType_t>::max)() < cur_index)
            spdlog::warn("Index overflow: {}", cur_index);

        indices_.push_back(static_cast<uint16_t>(vertices_.size()));
        vertices_.push_back(vertex);
    }

}


namespace mirinae {

    std::optional<VerticesStaticPair> parse_dmd_static(const uint8_t* const file_content, const size_t content_size) {
        dal::parser::Model parsed_model;
        constexpr auto a = sizeof(dal::parser::Model);
        constexpr auto b = offsetof(dal::parser::Model, m_units_straight);
        constexpr auto c = offsetof(dal::parser::Model, m_units_straight_joint);
        constexpr auto d = offsetof(dal::parser::Model, m_units_indexed);
        constexpr auto e = offsetof(dal::parser::Model, m_units_indexed_joint);
        constexpr auto f = offsetof(dal::parser::Model, m_animations);
        constexpr auto g = offsetof(dal::parser::Model, m_skeleton);
        constexpr auto h = offsetof(dal::parser::Model, m_aabb);
        constexpr auto i = offsetof(dal::parser::Model, m_signature_hex);
        constexpr auto j = sizeof(dal::parser::AABB3);
        constexpr auto k = sizeof(std::string);

        const auto parse_result = dal::parser::parse_dmd(parsed_model, file_content, content_size);
        if (dal::parser::ModelParseResult::success != parse_result) {
            spdlog::error("Failed to parse dmd file: {}", static_cast<int>(parse_result));
            return std::nullopt;
        }

        if (!parsed_model.m_units_straight_joint.empty())
            spdlog::warn("Parsing static DMD model but skinned straight mesh found");
        if (!parsed_model.m_units_indexed_joint.empty())
            spdlog::warn("Parsing static DMD model but skinned indexed mesh found");
        if (!parsed_model.m_units_straight.empty())
            spdlog::warn("Parsing static DMD model that has straight mesh, which will cause performance issue");

        VerticesStaticPair output;

        for (const auto& unit : parsed_model.m_units_straight) {
            const auto vert_count = unit.m_mesh.m_vertices.size() / 3;
            for (size_t i = 0; i < vert_count; ++i) {
                VertexStatic vert;
                vert.pos_.x = unit.m_mesh.m_vertices[i * 3 + 0];
                vert.pos_.y = unit.m_mesh.m_vertices[i * 3 + 1];
                vert.pos_.z = unit.m_mesh.m_vertices[i * 3 + 2];
                vert.normal_.x = unit.m_mesh.m_normals[i * 3 + 0];
                vert.normal_.y = unit.m_mesh.m_normals[i * 3 + 1];
                vert.normal_.z = unit.m_mesh.m_normals[i * 3 + 2];
                vert.texcoord_.x = unit.m_mesh.m_texcoords[i * 2 + 0];
                vert.texcoord_.y = unit.m_mesh.m_texcoords[i * 2 + 1];
                output.append_vertex(vert);
            }
        }

        for (const auto& unit : parsed_model.m_units_indexed) {
            for (const auto index : unit.m_mesh.m_indices) {
                if ((std::numeric_limits<VertIndexType_t>::max)() < index)
                    spdlog::warn("Index overflow: {}", index);

                output.indices_.push_back(index);
            }

            for (const auto& vert : unit.m_mesh.m_vertices) {
                output.vertices_.emplace_back(vert.m_position, vert.m_uv_coords, vert.m_normal);
            }
        }

        return output;
    }

}
