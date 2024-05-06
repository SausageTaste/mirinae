#include "mirinae/actor/transform.hpp"

#include <spdlog/spdlog.h>


// NoclipController
namespace mirinae::syst {

    bool NoclipController::on_key_event(const key::Event& e) {
        if (e.action_type == key::ActionType::down) {
            if (e.key == key::KeyCode::lbracket)
                move_speed_ *= 0.5;
            else if (e.key == key::KeyCode::rbracket)
                move_speed_ *= 2;
        }

        return true;
    }

    bool NoclipController::on_mouse_event(
        const mouse::Event& e, IOsIoFunctions& osio
    ) {
        using mirinae::mouse::ActionType;

        if (e.action_ == ActionType::move && owning_mouse_) {
            last_mouse_pos_ = { e.xpos_, e.ypos_ };
            return true;
        }

        if (e.button_ == mouse::ButtonCode::right) {
            if (e.action_ == ActionType::down) {
                owning_mouse_ = true;
                last_mouse_pos_ = { e.xpos_, e.ypos_ };
                last_applied_mouse_pos_ = last_mouse_pos_;
                osio.set_hidden_mouse_mode(true);
                return true;
            } else if (e.action_ == ActionType::up) {
                owning_mouse_ = false;
                last_mouse_pos_ = { 0, 0 };
                last_applied_mouse_pos_ = last_mouse_pos_;
                osio.set_hidden_mouse_mode(false);
                return true;
            }
        }

        return owning_mouse_;
    }

    void NoclipController::apply(
        cpnt::Transform& transform,
        const double delta_time,
        const key::EventAnalyzer& key_states
    ) {
        {
            glm::dvec3 move_dir{ 0, 0, 0 };
            if (key_states.is_pressed(key::KeyCode::w))
                move_dir.z -= 1;
            if (key_states.is_pressed(key::KeyCode::s))
                move_dir.z += 1;
            if (key_states.is_pressed(key::KeyCode::a))
                move_dir.x -= 1;
            if (key_states.is_pressed(key::KeyCode::d))
                move_dir.x += 1;

            if (glm::length(move_dir) > 0) {
                move_dir = glm::mat3_cast(transform.rot_) * move_dir;
                transform.pos_ += move_dir * (delta_time * move_speed_);
            }
        }

        {
            double vertical = 0;
            if (key_states.is_pressed(key::KeyCode::lctrl))
                vertical -= 1;
            if (key_states.is_pressed(key::KeyCode::space))
                vertical += 1;

            if (vertical != 0)
                transform.pos_.y += vertical * delta_time * move_speed_;
        }

        {
            auto rot = cpnt::Transform::Angle::from_zero();
            if (key_states.is_pressed(key::KeyCode::left))
                rot = rot.add_rad(1);
            if (key_states.is_pressed(key::KeyCode::right))
                rot = rot.add_rad(-1);

            if (0 != rot.rad())
                transform.rotate(rot * (delta_time * 2), glm::vec3{ 0, 1, 0 });
        }

        {
            auto rot = cpnt::Transform::Angle::from_zero();
            if (key_states.is_pressed(key::KeyCode::up))
                rot = rot.add_rad(1);
            if (key_states.is_pressed(key::KeyCode::down))
                rot = rot.add_rad(-1);

            if (0 != rot.rad()) {
                const auto right = glm::mat3_cast(transform.rot_) *
                                   glm::vec3{ 1, 0, 0 };
                transform.rotate(rot * (delta_time * 2), right);
            }
        }

        {
            const auto rot = last_applied_mouse_pos_.x - last_mouse_pos_.x;
            if (0 != rot)
                transform.rotate(
                    cpnt::Transform::Angle::from_rad(rot * delta_time * 0.1),
                    glm::vec3{ 0, 1, 0 }
                );
        }

        {
            const auto rot = last_applied_mouse_pos_.y - last_mouse_pos_.y;
            if (0 != rot) {
                const auto right = glm::mat3_cast(transform.rot_) *
                                   glm::vec3{ 1, 0, 0 };
                transform.rotate(
                    cpnt::Transform::Angle::from_rad(rot * delta_time * 0.1),
                    right
                );
            }
        }

        last_applied_mouse_pos_ = last_mouse_pos_;
    }

}  // namespace mirinae::syst
