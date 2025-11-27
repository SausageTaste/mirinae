#include "mirinae/vulkan/base/context/camera.hpp"


namespace {

    glm::vec3 make_normal(
        const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2
    ) {
        return glm::normalize(glm::cross(p1 - p0, p2 - p0));
    }

}  // namespace


// ViewFrustum
namespace mirinae {

    void ViewFrustum::update(const glm::dmat4& proj, const glm::dmat4& view) {
        constexpr float VALUE = 1;

        const static std::vector<glm::vec3> v{
            glm::vec3(VALUE, VALUE, VALUE),    // 0
            glm::vec3(-VALUE, VALUE, VALUE),   // 1
            glm::vec3(VALUE, -VALUE, VALUE),   // 2
            glm::vec3(VALUE, VALUE, 0),        // 3
            glm::vec3(-VALUE, -VALUE, VALUE),  // 4
            glm::vec3(-VALUE, VALUE, 0),       // 5
            glm::vec3(VALUE, -VALUE, 0),       // 6
            glm::vec3(-VALUE, -VALUE, 0),      // 7
        };

        view_inv_ = glm::inverse(view);

        const auto proj_inv = glm::mat4(glm::inverse(proj));
        for (size_t i = 0; i < v.size(); ++i) {
            auto ndc = proj_inv * glm::vec4(v[i], 1);
            auto ndc3 = glm::vec3(ndc) / ndc.w;
            vtx_[i] = ndc3;
        }

        // Since 'Separating Axis Theorem' is used, the direction of normal
        // doesn't really matter
        axes_[0] = ::make_normal(vtx_[0], vtx_[2], vtx_[3]);  // Fixed +x
        axes_[1] = ::make_normal(vtx_[1], vtx_[4], vtx_[5]);  // Fixed -x
        axes_[2] = ::make_normal(vtx_[0], vtx_[1], vtx_[3]);  // Fixed +y
        axes_[3] = ::make_normal(vtx_[2], vtx_[4], vtx_[6]);  // Fixed -y
        axes_[4] = ::make_normal(vtx_[0], vtx_[1], vtx_[2]);  // Fixed +z
        axes_[5] = ::make_normal(vtx_[3], vtx_[5], vtx_[6]);  // Fixed -z

        return;
    }

}  // namespace mirinae


// CamGeometry
namespace mirinae {

    void CamGeometry::update(
        const cpnt::StandardCamera& cam,
        const TransformQuat<double>* tform,
        const uint32_t width,
        const uint32_t height
    ) {
        if (tform) {
            view_mat_ = tform->make_view_mat();
            view_inv_ = glm::inverse(view_mat_);
            view_pos_ = tform->pos_;
        } else {
            view_mat_ = glm::dmat4(1);
            view_inv_ = glm::dmat4(1);
            view_pos_ = glm::dvec3(0);
        }

        proj_mat_ = cam.proj_.make_proj_mat(width, height);
        proj_inv_ = glm::inverse(proj_mat_);
        fov_ = cam.proj_.fov_;
        view_frustum_.update(proj_mat_, view_mat_);
    }

}  // namespace mirinae


// CamCache
namespace mirinae {

    void CamCache::update(
        const cpnt::StandardCamera& cam,
        const TransformQuat<double>* tform,
        const uint32_t width,
        const uint32_t height
    ) {
        CamGeometry::update(cam, tform, width, height);

        exposure_ = cam.exposure_;
        gamma_ = cam.gamma_;
    }

}  // namespace mirinae
