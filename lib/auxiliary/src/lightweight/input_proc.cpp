#include "mirinae/lightweight/input_proc.hpp"

#include <SDL3/SDL_scancode.h>


// InputActionMapper
namespace mirinae {

    InputActionMapper::InputActionMapper() {
        key_map_.emplace(SDL_SCANCODE_W, ActionType::move_forward);
        key_map_.emplace(SDL_SCANCODE_S, ActionType::move_backward);
        key_map_.emplace(SDL_SCANCODE_A, ActionType::move_left);
        key_map_.emplace(SDL_SCANCODE_D, ActionType::move_right);

        key_map_.emplace(SDL_SCANCODE_UP, ActionType::look_up);
        key_map_.emplace(SDL_SCANCODE_DOWN, ActionType::look_down);
        key_map_.emplace(SDL_SCANCODE_LEFT, ActionType::look_left);
        key_map_.emplace(SDL_SCANCODE_RIGHT, ActionType::look_right);

        key_map_.emplace(SDL_SCANCODE_SPACE, ActionType::translate_up);
        key_map_.emplace(SDL_SCANCODE_LALT, ActionType::translate_down);
        key_map_.emplace(SDL_SCANCODE_LSHIFT, ActionType::sprint);
    }

    bool InputActionMapper::on_key_event(const key::Event& e) {
        const auto it = key_map_.find(e.scancode_);
        if (it == key_map_.end())
            return false;

        const auto action = it->second;

        switch (e.action_type) {
            case key::ActionType::down:
                action_values_[action] = 1;
                break;
            case key::ActionType::up:
                action_values_[action] = 0;
                break;
        }

        return true;
    }

    bool InputActionMapper::on_text_event(char32_t c) { return false; }

    bool InputActionMapper::on_mouse_event(const mouse::Event& e) {
        using mirinae::mouse::ActionType;

        if (e.action_ == ActionType::mwheel_up) {
            ++mwheel_up_;
            return true;
        } else if (e.action_ == ActionType::mwheel_down) {
            ++mwheel_down_;
            return true;
        }

        if (e.action_ == ActionType::move) {
            if (move_pointer_ == &mouse_state_) {
                mouse_state_.last_pos_ = { e.xpos_, e.ypos_ };
                return true;
            } else if (look_pointer_ == &mouse_state_) {
                mouse_state_.last_pos_ = { e.xpos_, e.ypos_ };
                return true;
            }
            return false;
        }

        if (e.button_ == mirinae::mouse::ButtonCode::right) {
            if (e.action_ == ActionType::down) {
                if (e.xpos_ < 400) {
                    if (move_pointer_)
                        return true;
                    move_pointer_ = &mouse_state_;
                } else {
                    if (look_pointer_)
                        return true;
                    look_pointer_ = &mouse_state_;
                    if (osio_)
                        osio_->set_hidden_mouse_mode(true);
                }

                mouse_state_.start_pos_ = { e.xpos_, e.ypos_ };
                mouse_state_.last_pos_ = mouse_state_.start_pos_;
                mouse_state_.consumed_pos_ = mouse_state_.start_pos_;
                return true;
            } else if (e.action_ == ActionType::up) {
                if (move_pointer_ == &mouse_state_) {
                    move_pointer_ = nullptr;
                } else if (look_pointer_ == &mouse_state_) {
                    look_pointer_ = nullptr;
                }

                mouse_state_.start_pos_ = { 0, 0 };
                mouse_state_.last_pos_ = mouse_state_.start_pos_;
                mouse_state_.consumed_pos_ = mouse_state_.start_pos_;
                if (osio_)
                    osio_->set_hidden_mouse_mode(false);
                return true;
            }
        }

        if (move_pointer_ == &mouse_state_)
            return true;
        else if (look_pointer_ == &mouse_state_)
            return true;

        return false;
    }

    bool InputActionMapper::on_mouse_rel_event(const mouse::EventRel& e) {
        using mirinae::mouse::ActionType;

        if (move_pointer_ == &mouse_state_) {
            mouse_state_.last_pos_.x += e.xrel_;
            mouse_state_.last_pos_.y += e.yrel_;
            return true;
        } else if (look_pointer_ == &mouse_state_) {
            mouse_state_.last_pos_.x += e.xrel_;
            mouse_state_.last_pos_.y += e.yrel_;
            return true;
        }

        return false;
    }

    bool InputActionMapper::on_touch_event(const touch::Event& e) {
        return false;
    }

    double InputActionMapper::get_value(ActionType action) const {
        const auto it = action_values_.find(action);
        if (it == action_values_.end())
            return 0;

        return it->second;
    }

    double InputActionMapper::get_value_move_backward() const {
        return this->get_value(ActionType::move_backward) -
               this->get_value(ActionType::move_forward);
    }

    double InputActionMapper::get_value_move_right() const {
        return this->get_value(ActionType::move_right) -
               this->get_value(ActionType::move_left);
    }

    glm::dvec2 InputActionMapper::get_value_key_look() const {
        glm::dvec2 out{ 0 };
        out.x = this->get_value(ActionType::look_left) -
                this->get_value(ActionType::look_right);
        out.y = this->get_value(ActionType::look_up) -
                this->get_value(ActionType::look_down);
        return out;
    }

    glm::dvec2 InputActionMapper::get_value_mouse_look() const {
        glm::dvec2 out{ 0 };

        if (look_pointer_) {
            out.x = look_pointer_->consumed_pos_.x - look_pointer_->last_pos_.x;
            out.y = look_pointer_->consumed_pos_.y - look_pointer_->last_pos_.y;
            look_pointer_->consumed_pos_ = look_pointer_->last_pos_;
        }

        return out;
    }

}  // namespace mirinae
