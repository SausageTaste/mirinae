#pragma once

#include <fmt/format.h>
#include <glm/vec3.hpp>


namespace fmt {

    template <>
    struct formatter<glm::vec3> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) {
            return ctx.begin();
        }

        template <typename FormatContext>
        auto format(const glm::vec3& v, FormatContext& ctx) {
            return fmt::format_to(
                ctx.out(), "({:.2f}, {:.2f}, {:.2f})", v.x, v.y, v.z
            );
        }
    };


    template <>
    struct formatter<glm::dvec3> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) {
            return ctx.begin();
        }

        template <typename FormatContext>
        auto format(const glm::dvec3& v, FormatContext& ctx) {
            return fmt::format_to(
                ctx.out(), "({:.2f}, {:.2f}, {:.2f})", v.x, v.y, v.z
            );
        }
    };

}  // namespace fmt
