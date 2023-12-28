#include "mirinae/actor/transform.hpp"


// NoclipController
namespace mirinae::syst {

    bool NoclipController::on_key_event(const key::Event& e) {
        key_states_.notify(e);
        return true;
    }

    void NoclipController::apply(cpnt::Transform& transform, float delta_time) const {
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
                transform.pos_ += move_dir * delta_time * 10.0f;
            }
        }

        {
            float vertical = 0;
            if (key_states_.is_pressed(key::KeyCode::lctrl))
                vertical -= 1;
            if (key_states_.is_pressed(key::KeyCode::space))
                vertical += 1;

            if (vertical != 0)
                transform.pos_.y += vertical * delta_time * 10.0f;
        }

        {
            float rot = 0;
            if (key_states_.is_pressed(key::KeyCode::left))
                rot += 1;
            if (key_states_.is_pressed(key::KeyCode::right))
                rot -= 1;

            if (0 != rot)
                transform.rotate(cpnt::Transform::Angle::from_rad(rot * delta_time * 2.f), glm::vec3{ 0, 1, 0 });
        }

        {
            float rot = 0;
            if (key_states_.is_pressed(key::KeyCode::up))
                rot += 1;
            if (key_states_.is_pressed(key::KeyCode::down))
                rot -= 1;

            if (0 != rot) {
                const auto right = glm::mat3_cast(transform.rot_) * glm::vec3{ 1, 0, 0 };
                transform.rotate(cpnt::Transform::Angle::from_rad(rot * delta_time * 2.f), right);
            }
        }
    }

}
