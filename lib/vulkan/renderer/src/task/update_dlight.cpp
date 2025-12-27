#include "task/update_dlight.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/camera.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"


// UpdateDlight
namespace mirinae {

    void UpdateDlight::init(CosmosSimulator& cosmos, Swapchain& swhain) {
        cosmos_ = &cosmos;
        swhain_ = &swhain;
    }

    void UpdateDlight::prepare() {
        this->set_size(cosmos_->reg().view<cpnt::DLight>().size());
    }

    void UpdateDlight::ExecuteRange(
        enki::TaskSetPartition range, uint32_t tid
    ) {
        namespace cpnt = cpnt;

        auto& reg = cosmos_->reg();

        const auto e_cam = cosmos_->scene().main_camera_;
        auto cam = reg.try_get<cpnt::StandardCamera>(e_cam);
        auto cam_view = reg.try_get<cpnt::Transform>(e_cam);

        if (cam == nullptr || cam_view == nullptr) {
            SPDLOG_WARN("Not a camera: {}", static_cast<uint32_t>(e_cam));
            return;
        }

        const auto view_inv = glm::inverse(cam_view->make_view_mat());

        auto view = reg.view<cpnt::DLight>();
        auto begin = view.begin() + range.start;
        auto end = view.begin() + range.end;
        for (auto it = begin; it != end; ++it) {
            const auto e = *it;
            auto& light = reg.get<cpnt::DLight>(e);

            auto tfrom = reg.try_get<cpnt::Transform>(e);
            if (!tfrom) {
                SPDLOG_WARN(
                    "DLight without transform: {}", static_cast<uint32_t>(e_cam)
                );
                continue;
            }

            tfrom->pos_ = cam_view->pos_;
            light.cascades_.update(
                swhain_->calc_ratio(),
                light.max_shadow_distance_,
                view_inv,
                cam->proj_,
                light,
                *tfrom
            );
        }
    }

}  // namespace mirinae
