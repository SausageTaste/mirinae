#include "mirinae/math/glm_fmt.hpp"

#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>


namespace {

    TEST(CustomFormat, Vec3) {
        std::vector<glm::dvec4> data{
            { 1.0 / 3.0, 2.0 / 3.0, 1.0 / 5.0, 85.0 },
            { 128, -256, -512, 1024 },
        };


        for (auto& x : data) {
            fmt::print("{:>8.3f} = dvec4\n", x);
            fmt::print("{:>8} = uvec4\n", glm::uvec4(x));
            fmt::print("{:>8.3f} = vec3\n", glm::vec3(x));
            fmt::print("{:>8.3f} = dvec2\n", glm::dvec2(x));
            fmt::print("{:>8} = ivec2\n", glm::ivec2(x));
        }
    }

    TEST(CustomFormat, Mat4) {
        std::vector<glm::dmat4> matrices{
            glm::translate(glm::dmat4{ 1 }, glm::dvec3{ 1.0 / 3.0, 0, 256 }),
            glm::perspective(glm::radians(45.0), 4.0 / 3.0, 0.1, 100.0),
        };

        for (auto& m : matrices) {
            fmt::print("{:>8.3f} = dmat4\n", m);
            fmt::print("{:>8.3f} = mat4\n", glm::mat4(m));
            fmt::print("{:>8} = imat4\n", glm::tmat4x4<int>(m));
            fmt::print("{:>8.3f} = dmat3\n", glm::dmat3(m));
            fmt::print("{:>8.3f} = mat3\n", glm::mat3(m));
        }
    }

}  // namespace


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
