#include "mirinae/cpnt/light.hpp"

#include <algorithm>

#include <imgui.h>
#include <sung/basic/aabb.hpp>


namespace {

    float max3(float a, float b, float c) {
        return std::max(std::max(a, b), c);
    }

    float max3(const glm::vec3& v) { return ::max3(v.r, v.g, v.b); }

    void render_color_intensity(mirinae::ColorIntensity& ci) {
        ImGui::PushID(&ci);
        ImGui::ColorEdit3("Color", &ci.color()[0]);
        ImGui::SliderFloat(
            "Intensity",
            &ci.intensity(),
            0.0,
            1000.0,
            nullptr,
            ImGuiSliderFlags_Logarithmic
        );
        ImGui::PopID();
    }

}  // namespace


// ColorIntensity
namespace mirinae {

    ColorIntensity::Vec3 ColorIntensity::scaled_color() const {
        return color_ * intensity_;
    }

    void ColorIntensity::set_scaled_color(const Vec3& color) {
        color_ = color;
        intensity_ = 1;
        this->normalize_color();
    }

    void ColorIntensity::set_scaled_color(T r, T g, T b) {
        color_.x = r;
        color_.y = g;
        color_.z = b;
        intensity_ = 1;
        this->normalize_color();
    }

    void ColorIntensity::normalize_color() {
        constexpr T EPSILON = static_cast<T>(0.0001);

        if (color_.x <= EPSILON && color_.y <= EPSILON && color_.z <= EPSILON) {
            color_ = glm::tvec3<T>{ 0 };
            intensity_ = 0;
            return;
        }

        const auto max = ::max3(color_);
        color_ /= max;
        intensity_ *= max;
        return;
    }

}  // namespace mirinae


// DirectionalLight
namespace mirinae {

    glm::dvec3 DirectionalLight::calc_to_light_dir(
        const glm::dmat4 view_mat, const Tform& tform
    ) const {
        const auto v = view_mat * tform.make_model_mat() *
                       glm::dvec4(0, 0, 1, 0);
        return glm::normalize(glm::dvec3(v));
    };

    /*
    glm::dmat4 DLight::make_proj_mat() const {
        auto p = glm::orthoRH_ZO<double>(-10, 10, -10, 10, -50, 50);
        return p;
    }

    glm::dmat4 DLight::make_view_mat() const {
        return transform_.make_view_mat();
    }

    glm::dmat4 DLight::make_light_mat() const {
        return make_proj_mat() * make_view_mat();
    }*/

    glm::dmat4 DirectionalLight::make_light_mat(
        const std::array<glm::dvec3, 8>& p, const Tform& tform
    ) const {
        const auto view_mat = tform.make_view_mat();

        sung::AABB3<double> aabb;
        aabb.set(p[0].x, p[0].y, p[0].z);

        for (auto& v : p) {
            const auto v4 = view_mat * glm::dvec4(v, 1);
            aabb.expand_to_span(v4.x, v4.y, v4.z);
        }

        // Why the hell???
        auto proj_mat = glm::orthoRH_ZO<double>(
            aabb.x_min(),
            aabb.x_max(),
            -aabb.y_max(),
            -aabb.y_min(),
            -2 * aabb.z_max() + aabb.z_min(),
            -aabb.z_min()
        );
        proj_mat[1][1] *= -1;

        return proj_mat * view_mat;
    }

    void DirectionalLight::set_light_dir(glm::dvec3 dir, Tform& tform) {
        dir = glm::normalize(dir);
        const auto axis = glm::cross(glm::dvec3{ 0, 0, -1 }, dir);
        const auto cos_angle = glm::dot(glm::dvec3{ 0, 0, -1 }, dir);
        const auto angle = sung::acos_safe(cos_angle);
        tform.reset_rotation();
        tform.rotate(Tform::Angle::from_rad(angle), axis);
    }

}  // namespace mirinae


// CascadeInfo
namespace mirinae {

    void CascadeInfo::update(
        const double ratio,
        const glm::dmat4& view_inv,
        const PerspectiveCamera<double>& pers,
        const DirectionalLight& dlight,
        const DirectionalLight::Tform& tform
    ) {
        const auto dist = this->make_plane_distances(pers.near_, pers.far_);

        for (size_t i = 0; i < dist.size() - 1; ++i) {
            auto& c = cascades_.at(i);

            c.near_ = dist[i];
            c.far_ = dist[i + 1];

            this->make_frustum_vertices(
                ratio, c.near_, pers.fov_, view_inv, c.frustum_verts_.data()
            );

            this->make_frustum_vertices(
                ratio, c.far_, pers.fov_, view_inv, c.frustum_verts_.data() + 4
            );

            c.light_mat_ = dlight.make_light_mat(c.frustum_verts_, tform);

            far_depths_[i] = this->calc_clip_depth(
                -c.far_, pers.near_, pers.far_
            );
        }

        return;
    }

    void CascadeInfo::make_frustum_vertices(
        const double screen_ratio,
        const double plane_dist,
        const Angle fov,
        const glm::dmat4& view_inv,
        glm::dvec3* const out
    ) {
        const auto tan_half_angle_vertical = std::tan(fov.rad() * 0.5);
        const auto tan_half_angle_horizontal = tan_half_angle_vertical *
                                               screen_ratio;

        const auto half_width = plane_dist * tan_half_angle_horizontal;
        const auto half_height = plane_dist * tan_half_angle_vertical;

        out[0] = glm::dvec3{ -half_width, -half_height, -plane_dist };
        out[1] = glm::dvec3{ half_width, -half_height, -plane_dist };
        out[2] = glm::dvec3{ -half_width, half_height, -plane_dist };
        out[3] = glm::dvec3{ half_width, half_height, -plane_dist };

        for (size_t i = 0; i < 4; ++i)
            out[i] = view_inv * glm::dvec4{ out[i], 1 };
    }

    std::array<double, 5> CascadeInfo::make_plane_distances(
        const double p_near, const double p_far
    ) {
        std::array<double, 5> out;
        const auto dist = p_far - p_near;

        out[0] = p_near;
        out[1] = p_near + dist * 0.05;
        out[2] = p_near + dist * 0.2;
        out[3] = p_near + dist * 0.5;
        out[4] = p_far;

        return out;
    }

    double CascadeInfo::calc_clip_depth(double z, double n, double f) {
        return (f * (z + n)) / (z * (f - n));
    }

}  // namespace mirinae


// DLight
namespace mirinae::cpnt {

    void DLight::render_imgui() { ::render_color_intensity(color_); }

}  // namespace mirinae::cpnt


// SLight
namespace mirinae::cpnt {

    void SLight::render_imgui() {
        ::render_color_intensity(color_);

        float inner_angle = inner_angle_.rad();
        ImGui::SliderAngle("Inner angle", &inner_angle, 0, 180);
        inner_angle_.set_rad(inner_angle);

        float outer_angle = outer_angle_.rad();
        ImGui::SliderAngle("Outer angle", &outer_angle, 0, 180);
        outer_angle_.set_rad(outer_angle);
    }

    glm::dvec3 SLight::calc_view_space_pos(
        const glm::dmat4 view_mat, const Tform& tform
    ) const {
        const auto v = view_mat * glm::dvec4(tform.pos_, 1);
        return glm::dvec3(v);
    }

    glm::dvec3 SLight::calc_to_light_dir(
        const glm::dmat4 view_mat, const Tform& tform
    ) const {
        const auto v = view_mat * tform.make_model_mat() *
                       glm::dvec4(0, 0, 1, 0);
        return glm::normalize(glm::dvec3(v));
    };

    glm::dmat4 SLight::make_proj_mat() const {
        return mirinae::make_perspective<double>(
            outer_angle_ * 2, 1.0, 0.1, max_distance_
        );
    }

    glm::dmat4 SLight::make_view_mat(const Tform& tform) const {
        return tform.make_view_mat();
    }

    glm::dmat4 SLight::make_light_mat(const Tform& tform) const {
        return make_proj_mat() * make_view_mat(tform);
    }

}  // namespace mirinae::cpnt


namespace mirinae::cpnt {

    void VPLight::render_imgui() { ::render_color_intensity(color_); }

    void AtmosphereSimple::render_imgui() {
        ImGui::ColorEdit3("Fog color", &fog_color_[0]);
        ImGui::SliderFloat(
            "Fog density",
            &fog_density_,
            0.0,
            0.01,
            "%.6f",
            ImGuiSliderFlags_Logarithmic
        );
    }

}  // namespace mirinae::cpnt
