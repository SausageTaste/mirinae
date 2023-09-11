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
                move_dir = glm::normalize(move_dir);
                move_dir = glm::mat3_cast(transform.rot_) * move_dir;
                transform.pos_ += move_dir * delta_time * 10.0f;
            }
        }

        {
            float vertical = 0;
            if (key_states_.is_pressed(key::KeyCode::q))
                vertical -= 1;
            if (key_states_.is_pressed(key::KeyCode::e))
                vertical += 1;

            if (vertical != 0) {
                transform.pos_.y += vertical * delta_time * 10.0f;
            }
        }

        {
            glm::vec3 rot_dir{ 0, 0, 0 };
            if (key_states_.is_pressed(key::KeyCode::up))
                rot_dir.x += 1;
            if (key_states_.is_pressed(key::KeyCode::down))
                rot_dir.x -= 1;
            if (key_states_.is_pressed(key::KeyCode::left))
                rot_dir.y += 1;
            if (key_states_.is_pressed(key::KeyCode::right))
                rot_dir.y -= 1;

            if (glm::length(rot_dir) > 0) {
                rot_dir = glm::normalize(rot_dir);
                transform.rot_ = glm::rotate(transform.rot_, delta_time * 3.0f, rot_dir);
            }
        }
    }

}
