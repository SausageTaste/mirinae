#pragma once

#include <spdlog/fmt/fmt.h>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


namespace fmt {

    template <typename T>
    struct formatter<glm::tvec2<T>> : formatter<T> {
        format_context::iterator format(
            const glm::tvec2<T>& v, format_context& ctx
        ) {
            fmt::format_to(ctx.out(), "(");
            formatter<T>::format(v.x, ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(v.y, ctx);
            fmt::format_to(ctx.out(), ")");

            return ctx.out();
        }
    };


    template <typename T>
    struct formatter<glm::tvec3<T>> : formatter<T> {
        format_context::iterator format(
            const glm::tvec3<T>& v, format_context& ctx
        ) {
            fmt::format_to(ctx.out(), "(");
            formatter<T>::format(v.x, ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(v.y, ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(v.z, ctx);
            fmt::format_to(ctx.out(), ")");

            return ctx.out();
        }
    };


    template <typename T>
    struct formatter<glm::tvec4<T>> : formatter<T> {
        format_context::iterator format(
            const glm::tvec4<T>& v, format_context& ctx
        ) {
            fmt::format_to(ctx.out(), "(");
            formatter<T>::format(v.x, ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(v.y, ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(v.z, ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(v.w, ctx);
            fmt::format_to(ctx.out(), ")");

            return ctx.out();
        }
    };


    template <typename T>
    struct formatter<glm::tmat3x3<T>> : formatter<T> {
        format_context::iterator format(
            const glm::tmat3x3<T>& x, format_context& ctx
        ) {
            fmt::format_to(ctx.out(), "[[");
            formatter<T>::format(x[0][0], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[1][0], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[2][0], ctx);
            fmt::format_to(ctx.out(), "]\n [");

            formatter<T>::format(x[0][1], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[1][1], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[2][1], ctx);
            fmt::format_to(ctx.out(), "]\n [");

            formatter<T>::format(x[0][2], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[1][2], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[2][2], ctx);
            fmt::format_to(ctx.out(), "]]");

            return ctx.out();
        }
    };


    template <typename T>
    struct formatter<glm::tmat4x4<T>> : formatter<T> {
        format_context::iterator format(
            const glm::tmat4x4<T>& x, format_context& ctx
        ) {
            fmt::format_to(ctx.out(), "[[");
            formatter<T>::format(x[0][0], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[1][0], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[2][0], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[3][0], ctx);
            fmt::format_to(ctx.out(), "]\n [");

            formatter<T>::format(x[0][1], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[1][1], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[2][1], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[3][1], ctx);
            fmt::format_to(ctx.out(), "]\n [");

            formatter<T>::format(x[0][2], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[1][2], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[2][2], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[3][2], ctx);
            fmt::format_to(ctx.out(), "]\n [");

            formatter<T>::format(x[0][3], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[1][3], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[2][3], ctx);
            fmt::format_to(ctx.out(), ", ");
            formatter<T>::format(x[3][3], ctx);
            fmt::format_to(ctx.out(), "]]");

            return ctx.out();
        }
    };

}  // namespace fmt
