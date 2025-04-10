#include "mirinae/cosmos.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/ren_model.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lightweight/input_proc.hpp"
#include "mirinae/lightweight/task.hpp"


namespace {

    class ValueInterpolator {

    public:
        void set(sung::TAngle<double> tgt) {
            tgt_ = tgt;
            cur_ = tgt;
        }

        void set_target(sung::TAngle<double> tgt) { tgt_ = tgt; }

        sung::TAngle<double> get(double dt) {
            const auto diff = tgt_.calc_short_diff_from(cur_);
            cur_ = cur_ + diff * (dt * 20);
            return cur_;
        }

    private:
        sung::TAngle<double> cur_;
        sung::TAngle<double> tgt_;
    };


    class ThirdPersonController : public mirinae::ICamController {

    public:
        using Tform = mirinae::TransformQuat<double>;

        void pre_sync(
            mirinae::Scene& scene, mirinae::InputActionMapper& action_map
        ) {
            namespace cpnt = mirinae::cpnt;
            using Angle = mirinae::cpnt::Transform::Angle;
            using ActionType = mirinae::InputActionMapper::ActionType;

            auto& reg = *scene.reg_;
            const auto dt = scene.clock().dt();

            auto modl = reg.try_get<cpnt::MdlActorSkinned>(target_);
            if (!modl) {
                camera_ = entt::null;
                return;
            }
            auto& anim_state = modl->anim_state_;

            auto cam_tform = reg.try_get<cpnt::Transform>(camera_);
            if (!cam_tform) {
                camera_ = entt::null;
                return;
            }

            auto tgt_tform = reg.try_get<cpnt::Transform>(target_);
            if (!tgt_tform) {
                target_ = entt::null;
                return;
            }

            const auto look_rot =
                (action_map.get_value_key_look() * key_look_speed_) +
                (action_map.get_value_mouse_look() * mouse_look_speed_);

            // Look horizontally
            {
                auto r = Angle::from_rad(look_rot.x);
                if (0 != r.rad())
                    cam_tform->rotate(r * dt, glm::vec3{ 0, 1, 0 });
            }

            // Look vertically
            {
                auto r = Angle::from_rad(look_rot.y);
                if (0 != r.rad()) {
                    const auto right = glm::mat3_cast(cam_tform->rot_) *
                                       glm::vec3{ 1, 0, 0 };
                    cam_tform->rotate(r * dt, right);
                }
            }

            // Zoom
            {
                offset_dist_ *= std::pow(1.04, -action_map.get_mwheel_zoom());
                offset_dist_ = std::max(offset_dist_, 0.001);
            }

            // Move with respect to camera direction
            {
                constexpr auto ANGLE_OFFSET = Angle::from_deg(90);

                const glm::dvec2 move_dir{
                    action_map.get_value_move_right(),
                    action_map.get_value_move_backward(),
                };
                const auto move_dir_len = glm::length(move_dir);
                const auto sprint = action_map.get_value(ActionType::sprint) >
                                    0.1;
                const auto walk = action_map.get_value(
                                      ActionType::translate_down
                                  ) > 0.1;

                if (move_dir_len > 0.01) {
                    const auto cam_front = cam_tform->make_forward_dir();
                    const auto cam_front_angle = Angle::from_rad(
                        std::atan2(cam_front.z, cam_front.x)
                    );

                    auto move_speed = move_speed_;
                    if (sprint) {
                        move_speed *= 3;
                    } else if (walk) {
                        move_speed = 0.7;
                    }

                    const auto move_vec_rot = mirinae::rotate_vec(
                        move_dir, cam_front_angle + ANGLE_OFFSET
                    );
                    const auto move_vec_scale = move_vec_rot *
                                                (dt * move_speed);

                    tgt_heading_.set_target(
                        Angle::from_rad(
                            std::atan2(move_vec_rot.y, move_vec_rot.x)
                        )
                    );

                    tgt_tform->pos_.x += move_vec_scale.x;
                    tgt_tform->pos_.z += move_vec_scale.y;
                    tgt_tform->reset_rotation();
                    tgt_tform->rotate(
                        player_model_heading_ - tgt_heading_.get(dt),
                        glm::vec3{ 0, 1, 0 }
                    );

                    if (sprint) {
                        if (anim_state.get_cur_anim_name() != anim_sprint_)
                            anim_state.select_anim_name(
                                anim_sprint_, scene.clock()
                            );
                    } else if (walk) {
                        if (anim_state.get_cur_anim_name() != anim_walk_)
                            anim_state.select_anim_name(
                                anim_walk_, scene.clock()
                            );
                    } else {
                        if (anim_state.get_cur_anim_name() != anim_run_)
                            anim_state.select_anim_name(
                                anim_run_, scene.clock()
                            );
                    }
                } else {
                    if (anim_state.get_cur_anim_name() != anim_idle_)
                        anim_state.select_anim_name(anim_idle_, scene.clock());
                }
            }

            // Move vertically
            {
                double vertical = 0;
                if (action_map.get_value(ActionType::translate_up))
                    vertical += 1;
                if (action_map.get_value(ActionType::translate_down))
                    vertical -= 1;

                tgt_tform->pos_.y += vertical * dt * 15;
            }
        }

        void post_sync(
            mirinae::Scene& scene, const mirinae::InputActionMapper& action_map
        ) {
            namespace cpnt = mirinae::cpnt;
            using Angle = mirinae::cpnt::Transform::Angle;
            auto& reg = *scene.reg_;
            const auto dt = scene.clock().dt();

            auto cam_tform = reg.try_get<cpnt::Transform>(camera_);
            if (!cam_tform) {
                camera_ = entt::null;
                return;
            }

            auto tgt_tform = reg.try_get<cpnt::Transform>(target_);
            if (!tgt_tform) {
                target_ = entt::null;
                return;
            }

            const auto cam_forward = cam_tform->make_forward_dir();
            const auto tgt_pos = tgt_tform->pos_;
            cam_tform->pos_ = tgt_pos - cam_forward * offset_dist_;
            cam_tform->pos_ += tgt_tform->make_up_dir() * offset_height_;
            cam_tform->pos_ += cam_tform->make_right_dir() * offset_hor_ *
                               offset_dist_;
        }

        void set_target(entt::entity target) override {
            if (target == entt::null)
                return;

            target_ = target;
        }

        void set_camera(entt::entity camera) override {
            if (camera == entt::null)
                return;

            camera_ = camera;
        }

    private:
        entt::entity target_ = entt::null;
        entt::entity camera_ = entt::null;
        ::ValueInterpolator tgt_heading_;

    public:
        double move_speed_ = 3;        // World space
        double offset_dist_ = 2;       // World space
        double offset_height_ = 0.75;  // World space
        double offset_hor_ = 0.2;      // World space
        double key_look_speed_ = 1;
        double mouse_look_speed_ = 0.1;
    };

}  // namespace


namespace {

    class TaskGlobalInit : public mirinae::StageTask {

    public:
        TaskGlobalInit(mirinae::CosmosSimulator& cosmos)
            : StageTask("global init"), cosmos_(cosmos) {
            fence_.succeed(this);
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            cosmos_.tick_clock();
            cosmos_.scene().do_frame();
        }

        enki::ITaskSet* get_fence() override { return &fence_; }

    private:
        mirinae::CosmosSimulator& cosmos_;
        mirinae::FenceTask fence_;
    };


    class TaskControlPreSync : public mirinae::StageTask {

    public:
        TaskControlPreSync(
            std::shared_ptr<ThirdPersonController> ctrl,
            mirinae::CosmosSimulator& cosmos,
            mirinae::InputActionMapper& action_map
        )
            : StageTask("control pre sync")
            , ctrl_(ctrl)
            , cosmos_(cosmos)
            , action_map_(action_map) {
            fence_.succeed(this);
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            ctrl_->pre_sync(cosmos_.scene(), action_map_);
        }

        enki::ITaskSet* get_fence() override { return &fence_; }

    private:
        std::shared_ptr<ThirdPersonController> ctrl_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::InputActionMapper& action_map_;
        mirinae::FenceTask fence_;
    };


    class TaskControlPostSync : public mirinae::StageTask {

    public:
        TaskControlPostSync(
            std::shared_ptr<ThirdPersonController> ctrl,
            mirinae::CosmosSimulator& cosmos,
            mirinae::InputActionMapper& action_map
        )
            : StageTask("control post sync")
            , ctrl_(ctrl)
            , cosmos_(cosmos)
            , action_map_(action_map) {
            fence_.succeed(this);
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            ctrl_->post_sync(cosmos_.scene(), action_map_);
        }

        enki::ITaskSet* get_fence() override { return &fence_; }

    private:
        std::shared_ptr<ThirdPersonController> ctrl_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::InputActionMapper& action_map_;
        mirinae::FenceTask fence_;
    };

}  // namespace


namespace mirinae {

    CosmosSimulator::CosmosSimulator(ScriptEngine& script)
        : scene_(clock_, script)
        , cam_ctrl_(std::make_shared<::ThirdPersonController>()) {}

    void CosmosSimulator::register_tasks(
        TaskGraph& tasks, mirinae::InputActionMapper& action_map
    ) {
        auto c = std::dynamic_pointer_cast<::ThirdPersonController>(cam_ctrl_);

        {
            auto& stage = tasks.stages_.emplace_back();
            stage.task_ = std::make_unique<TaskGlobalInit>(*this);
        }

        scene_.register_tasks(tasks);

        {
            auto& stage = tasks.stages_.emplace_back();
            stage.task_ = std::make_unique<TaskControlPreSync>(
                c, *this, action_map
            );
        }

        phys_world_.register_tasks(tasks, *scene_.reg_);

        {
            auto& stage = tasks.stages_.emplace_back();
            stage.task_ = std::make_unique<TaskControlPostSync>(
                c, *this, action_map
            );
        }
    }

    void CosmosSimulator::tick_clock() { clock_.tick(); }

}  // namespace mirinae
