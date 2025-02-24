#pragma once

#include <array>
#include <filesystem>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <sung/basic/angle.hpp>

#include "mirinae/math/mamath.hpp"


namespace mirinae {

    class ColorIntensity {

    public:
        using T = float;
        using Vec3 = glm::tvec3<T>;

        Vec3& color() { return color_; }
        T& intensity() { return intensity_; }
        Vec3 scaled_color() const;

        void set_scaled_color(const Vec3& color);
        void set_scaled_color(T r, T g, T b);

        void normalize_color();

    private:
        Vec3 color_{ 0 };
        T intensity_ = 0;
    };


    class DirectionalLight {

    public:
        using Tform = TransformQuat<double>;

    public:
        glm::dvec3 calc_to_light_dir(const glm::dmat4 view, const Tform& tform)
            const;

        // glm::dmat4 make_proj_mat() const;
        // glm::dmat4 make_view_mat() const;
        // glm::dmat4 make_light_mat() const;
        glm::dmat4 make_light_mat(
            const std::array<glm::dvec3, 8>& p, const Tform& tform
        ) const;

        // Set the direction of the light ray traveling out of the light source
        void set_light_dir(glm::dvec3 dir, Tform& tform);

        void set_light_dir(double x, double y, double z, Tform& tform) {
            return this->set_light_dir(glm::dvec3{ x, y, z }, tform);
        }

    public:
        ColorIntensity color_;
    };


    class CascadeInfo {

    public:
        using Angle = sung::TAngle<double>;

        struct Cascade {
            std::array<glm::dvec3, 8> frustum_verts_;
            glm::dmat4 light_mat_;
            double near_;
            double far_;
        };

        void update(
            const double ratio,
            const glm::dmat4& view_inv,
            const PerspectiveCamera<double>& pers,
            const DirectionalLight& dlight,
            const DirectionalLight::Tform& tform
        );

        std::array<Cascade, 4> cascades_;
        std::array<double, 4> far_depths_;

    private:
        static void make_frustum_vertices(
            const double screen_ratio,
            const double plane_dist,
            const Angle fov,
            const glm::dmat4& view_inv,
            glm::dvec3* const out
        );

        static std::array<double, 5> make_plane_distances(
            const double p_near, const double p_far
        );

        double calc_clip_depth(double z, double n, double f);
    };

}  // namespace mirinae


namespace mirinae::cpnt {

    class DLight : public DirectionalLight {

    public:
        void render_imgui();

        CascadeInfo cascades_;
    };


    class SLight {

    public:
        using Tform = TransformQuat<double>;

    public:
        void render_imgui();

        glm::dvec3 calc_view_space_pos(
            const glm::dmat4 view_mat, const Tform& tform
        ) const;

        glm::dvec3 calc_to_light_dir(
            const glm::dmat4 view_mat, const Tform& tform
        ) const;

        glm::dmat4 make_proj_mat() const;
        glm::dmat4 make_view_mat(const Tform& tform) const;
        glm::dmat4 make_light_mat(const Tform& tform) const;

    public:
        ColorIntensity color_;
        sung::TAngle<double> inner_angle_;
        sung::TAngle<double> outer_angle_;
        double max_distance_ = 100;
    };


    class VPLight {

    public:
        void render_imgui();

    public:
        ColorIntensity color_;
    };


    struct AtmosphereSimple {

    public:
        AtmosphereSimple();
        void render_imgui();

    public:
        std::filesystem::path sky_tex_path_;
        glm::vec3 fog_color_;
        float fog_density_;
    };

}  // namespace mirinae::cpnt
