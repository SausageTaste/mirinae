#include "mirinae/lightweight/input_proc.hpp"


// InputActionMapper
namespace mirinae {

    InputActionMapper::InputActionMapper() {
        key_map_.emplace(key::KeyCode::w, ActionType::move_forward);
        key_map_.emplace(key::KeyCode::s, ActionType::move_backward);
        key_map_.emplace(key::KeyCode::a, ActionType::move_left);
        key_map_.emplace(key::KeyCode::d, ActionType::move_right);

        key_map_.emplace(key::KeyCode::up, ActionType::look_up);
        key_map_.emplace(key::KeyCode::down, ActionType::look_down);
        key_map_.emplace(key::KeyCode::left, ActionType::look_left);
        key_map_.emplace(key::KeyCode::right, ActionType::look_right);

        key_map_.emplace(key::KeyCode::space, ActionType::translate_up);
        key_map_.emplace(key::KeyCode::lshfit, ActionType::translate_down);
    }

    bool InputActionMapper::on_key_event(const key::Event& e) {
        const auto it = key_map_.find(e.key);
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

    double InputActionMapper::get_value_look_up() const {
        return this->get_value(ActionType::look_up) -
               this->get_value(ActionType::look_down);
    }

    double InputActionMapper::get_value_look_left() const {
        return this->get_value(ActionType::look_left) -
               this->get_value(ActionType::look_right);
    }

}  // namespace mirinae
