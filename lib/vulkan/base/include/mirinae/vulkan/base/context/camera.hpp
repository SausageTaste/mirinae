#pragma once

#include <array>

#include "mirinae/cpnt/camera.hpp"
#include "mirinae/vulkan/base/context/base.hpp"


namespace mirinae {

    class ViewFrustum {

    public:
        void update(const glm::dmat4& proj, const glm::dmat4& view);

    public:
        std::array<glm::vec3, 8> vtx_;
        std::array<glm::vec3, 6> axes_;
        glm::dmat4 view_inv_;
    };


    class CamGeometry {

    public:
        void update(
            const cpnt::StandardCamera& cam,
            const TransformQuat<double>* tform,
            const uint32_t width,
            const uint32_t height
        );

        auto& proj() const { return proj_mat_; }
        auto& proj_inv() const { return proj_inv_; }
        auto& view() const { return view_mat_; }
        auto& view_inv() const { return view_inv_; }
        auto pv() const { return proj_mat_ * view_mat_; }

        // World space position of the camera
        auto& view_pos() const { return view_pos_; }
        auto& fov() const { return fov_; }
        auto& view_frustum() const { return view_frustum_; }

    private:
        ViewFrustum view_frustum_;
        glm::dmat4 proj_mat_;
        glm::dmat4 view_mat_;
        glm::dmat4 proj_inv_;
        glm::dmat4 view_inv_;
        glm::dvec3 view_pos_;
        sung::TAngle<double> fov_;
    };


    class CamCache : public CamGeometry {

    public:
        void update(
            const cpnt::StandardCamera& cam,
            const TransformQuat<double>* tform,
            const uint32_t width,
            const uint32_t height
        );

    public:
        float exposure_ = 1;
        float gamma_ = 1;
    };


    struct RpCtxtCamera : public RpCtxtBase {
        CamCache main_cam_;
        ViewFrustum view_frustum_;
    };

}  // namespace mirinae
