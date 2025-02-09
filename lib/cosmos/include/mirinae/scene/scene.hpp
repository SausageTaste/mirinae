#pragma once

#include <filesystem>

#include <entt/entt.hpp>
#include <sung/basic/geometry3d.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/lightweight/script.hpp"
#include "mirinae/lightweight/skin_anim.hpp"
#include "mirinae/math/mamath.hpp"


namespace mirinae::cpnt {

    using Transform = TransformQuat<double>;


    template <typename T>
    class TColorIntensity {

    public:
        glm::tvec3<T>& color() { return color_; }
        T& intensity() { return intensity_; }

        glm::tvec3<T> scaled_color() const { return color_ * intensity_; }

        void set_scaled_color(const glm::tvec3<T>& color) {
            color_ = color;
            intensity_ = 1;
            this->normalize_color();
        }

        void set_scaled_color(T r, T g, T b) {
            color_.x = r;
            color_.y = g;
            color_.z = b;
            intensity_ = 1;
            this->normalize_color();
        }

        void normalize_color() {
            constexpr T EPSILON = static_cast<T>(0.0001);

            if (color_.x <= EPSILON && color_.y <= EPSILON &&
                color_.z <= EPSILON) {
                color_ = glm::tvec3<T>{ 0 };
                intensity_ = 0;
                return;
            }

            const auto max = std::max({ color_.r, color_.g, color_.b });
            color_ /= max;
            intensity_ *= max;
            return;
        }

    private:
        glm::tvec3<T> color_{ 0 };
        T intensity_ = 0;
    };

    using ColorIntensity = TColorIntensity<float>;


    struct DLight {
        /**
         * @param view_mat View matrix of camera
         * @return glm::vec3
         */
        glm::dvec3 calc_to_light_dir(const glm::dmat4 view_mat) const;

        // glm::dmat4 make_proj_mat() const;
        // glm::dmat4 make_view_mat() const;
        // glm::dmat4 make_light_mat() const;
        glm::dmat4 make_light_mat(const std::array<glm::dvec3, 8>& p) const;

        // Set the direction of the light ray traveling out of the light source
        void set_light_dir(glm::dvec3 dir);
        void set_light_dir(double x, double y, double z) {
            this->set_light_dir(glm::dvec3{ x, y, z });
        }

        TransformQuat<double> transform_;
        ColorIntensity color_;
    };


    struct SLight {
        glm::dvec3 calc_view_space_pos(const glm::dmat4 view_mat) const;
        glm::dvec3 calc_to_light_dir(const glm::dmat4 view_mat) const;

        glm::dmat4 make_proj_mat() const;
        glm::dmat4 make_view_mat() const;
        glm::dmat4 make_light_mat() const;

        TransformQuat<double> transform_;
        ColorIntensity color_;
        sung::TAngle<double> inner_angle_;
        sung::TAngle<double> outer_angle_;
        double max_distance_ = 100;
    };


    struct VPLight {
        ColorIntensity color_;
        glm::dvec3 pos_;
    };


    struct StaticModelActor {
        std::filesystem::path model_path_;
    };


    struct SkinnedModelActor {
        std::filesystem::path model_path_;
        SkinAnimState anim_state_;
    };


    struct StandardCamera {
        TransformQuat<double> view_;
        PerspectiveCamera<double> proj_;
        float exposure_ = 1;
        float gamma_ = 1;
    };


    struct Ocean {
        constexpr static uint32_t CASCADE_COUNT = 3;

        static double max_wavelen(double L) {
            return std::ceil(
                SUNG_PI * std::sqrt(2.0 * 256.0 * 256.0 / (L * L))
            );
        }

        struct Cascade {
            float amplitude() const { return active_ ? amplitude_ : 0; }

            glm::vec2 texco_offset_{ 0, 0 };
            glm::vec2 texco_scale_{ 1, 0 };
            float amplitude_ = 500000;
            float jacobian_scale_ = 1;
            float cutoff_high_ = 0;
            float cutoff_low_ = 0;
            float lod_scale_ = 100;
            float L_ = 100;
            bool active_ = true;
        };

        TransformQuat<double> transform_;
        std::array<Cascade, CASCADE_COUNT> cascades_;
        glm::dvec2 wind_dir_{ 1, 1 };
        double time_ = 0;
        double repeat_time_ = 100;
        float wind_speed_ = 10;
        float fetch_ = 100;
        float depth_ = 1;
        float swell_ = 0.5;
        float spread_blend_ = 0.5;
        float tile_size_ = 20;
        float foam_scale_ = 1;
        float foam_bias_ = 2;
        float lod_scale_ = 1;
        int tile_count_x_ = 10;
        int tile_count_y_ = 10;
    };


    struct AtmosphereSimple {
        glm::vec3 fog_color_{ 0.5, 0.5, 0.5 };
        float fog_density_ = 0.0001f;
    };

}  // namespace mirinae::cpnt


namespace mirinae {

    class Scene {

    public:
        Scene(const clock_t& global_clock, ScriptEngine& script);

        auto& clock() const { return clock_; }

        void do_frame();

        // Ray in world space
        void pick_entt(const sung::LineSegment3& ray);

    public:
        constexpr static uint64_t MAGIC_NUM = 46461236464165;

        entt::registry reg_;
        std::vector<entt::entity> entt_without_model_;
        entt::entity main_camera_ = entt::null;
        const uint64_t magic_num_ = MAGIC_NUM;

    private:
        ScriptEngine& script_;
        clock_t clock_;
    };

}  // namespace mirinae
