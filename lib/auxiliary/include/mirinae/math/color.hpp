#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


namespace mirinae {

    glm::vec4 split_color(const glm::vec3& color);
    glm::vec3 combine_color(const glm::vec4& color);


    class ColorIntensity {

    public:
        using T = float;
        using Vec3 = glm::tvec3<T>;

        Vec3& color() { return color_; }
        T& intensity() { return intensity_; }
        Vec3 scaled_color() const;

        void set_scaled_color(const Vec3& color);
        void set_scaled_color(T r, T g, T b);
        void set_scaled_color(T rgb);

        void normalize_color();

    private:
        Vec3 color_{ 0 };
        T intensity_ = 0;
    };

}  // namespace mirinae
