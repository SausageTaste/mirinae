#include "mirinae/actor/transform.hpp"


// NoclipController
namespace mirinae::syst {

    bool NoclipController::on_key_event(const key::Event& e) {
        key_states_.notify(e);

        if (e.action_type == key::ActionType::down) {
            if (e.key == key::KeyCode::lbracket)
                move_speed_ *= 0.5;
            else if (e.key == key::KeyCode::rbracket)
                move_speed_ *= 2;
        }

        return true;
    }

    bool NoclipController::on_mouse_event(const mirinae::mouse::Event& e) {
        using mirinae::mouse::ActionType;

        if (e.action_ == ActionType::down) {
            owning_mouse_ = true;
            last_mouse_pos_ = { e.xpos_, e.ypos_ };
            last_applied_mouse_pos_ = last_mouse_pos_;
        } else if (e.action_ == ActionType::up) {
            owning_mouse_ = false;
            last_mouse_pos_ = { 0, 0 };
            last_applied_mouse_pos_ = last_mouse_pos_;
        } else if (e.action_ == ActionType::move && owning_mouse_) {
            last_mouse_pos_ = { e.xpos_, e.ypos_ };
        }

        return owning_mouse_;
    }

    void NoclipController::apply(cpnt::Transform& transform, float delta_time) {
        {
            glm::vec3 move_dir{ 0, 0, 0 };
            if (key_states_.is_pressed(key::KeyCode::w))
                move_dir.z -= 1;
            if (key_states_.is_pressed(key::KeyCode::s))
                move_dir.z += 1;
            if (key_states_.is_pressed(key::KeyCode::a))
                move_dir.x -= 1;
            if (key_states_.is_pressed(key::KeyCode::d))
                move_dir.x += 1;

            if (glm::length(move_dir) > 0) {
                move_dir = glm::mat3_cast(transform.rot_) * move_dir;
                transform.pos_ += move_dir * float(delta_time * move_speed_);
            }
        }

        {
            float vertical = 0;
            if (key_states_.is_pressed(key::KeyCode::lctrl))
                vertical -= 1;
            if (key_states_.is_pressed(key::KeyCode::space))
                vertical += 1;

            if (vertical != 0)
                transform.pos_.y += vertical * delta_time * move_speed_;
        }

        {
            float rot = 0;
            if (key_states_.is_pressed(key::KeyCode::left))
                rot += 1;
            if (key_states_.is_pressed(key::KeyCode::right))
                rot -= 1;

            if (0 != rot)
                transform.rotate(
                    cpnt::Transform::Angle::from_rad(rot * delta_time * 2.f),
                    glm::vec3{ 0, 1, 0 }
                );
        }

        {
            float rot = 0;
            if (key_states_.is_pressed(key::KeyCode::up))
                rot += 1;
            if (key_states_.is_pressed(key::KeyCode::down))
                rot -= 1;

            if (0 != rot) {
                const auto right = glm::mat3_cast(transform.rot_) *
                                   glm::vec3{ 1, 0, 0 };
                transform.rotate(
                    cpnt::Transform::Angle::from_rad(rot * delta_time * 2.f),
                    right
                );
            }
        }

        {
            const auto rot = last_applied_mouse_pos_.x - last_mouse_pos_.x;
            if (0 != rot)
                transform.rotate(
                    cpnt::Transform::Angle::from_rad(rot * delta_time * 0.3),
                    glm::vec3{ 0, 1, 0 }
                );
        }

        {
            const auto rot = last_applied_mouse_pos_.y - last_mouse_pos_.y;
            if (0 != rot) {
                const auto right = glm::mat3_cast(transform.rot_) *
                                   glm::vec3{ 1, 0, 0 };
                transform.rotate(
                    cpnt::Transform::Angle::from_rad(rot * delta_time * 0.3),
                    right
                );
            }
        }

        last_applied_mouse_pos_ = last_mouse_pos_;
    }

}  // namespace mirinae::syst
