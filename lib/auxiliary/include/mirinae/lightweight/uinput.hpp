#pragma once

#include <array>
#include <chrono>
#include <map>
#include <optional>


namespace mirinae::key {

    class EventAnalyzer;

    using Clock_t = std::chrono::steady_clock;

    enum class ActionType { down, up };


    // TODO: Rename this to KeyActionType.
    struct Event {
        Clock_t::time_point timepoint = Clock_t::now();
        ActionType action_type = ActionType::down;
        int scancode_ = 0;
        int keycode_ = 0;
        const EventAnalyzer* states_ = nullptr;
    };


    class EventAnalyzer {

    public:
        struct KeyState {
            Clock_t::time_point timepoint = Clock_t::now();
            bool pressed = false;
        };

    public:
        void notify(const Event& e) {
            auto& state = this->get_state(e.scancode_);
            state.timepoint = e.timepoint;
            state.pressed = (e.action_type == key::ActionType::down);
        }

        bool is_pressed(int scancode) const {
            if (auto state = this->try_get_state(scancode))
                return state->pressed;
            return false;
        }

        std::optional<Clock_t::time_point> get_timepoint(int scancode) const {
            if (auto state = this->try_get_state(scancode))
                return state->timepoint;
            return std::nullopt;
        }

    private:
        KeyState& get_state(int key) {
            auto it = states_.find(key);
            if (it != states_.end()) {
                return it->second;
            }

            return states_.insert({ key, KeyState() }).first->second;
        }

        const KeyState* try_get_state(int key) const {
            auto it = states_.find(key);
            if (it != states_.end()) {
                return &it->second;
            }

            return nullptr;
        }

        std::map<int, KeyState> states_;
    };

}  // namespace mirinae::key


namespace mirinae::mouse {

    using Clock_t = std::chrono::steady_clock;

    enum class ActionType { move, down, up, mwheel_up, mwheel_down };

    enum class ButtonCode {
        left,
        right,
        middle,
        /* End Of Enum, just for calculating number of elements of Enum class */
        eoe
    };


    struct Event {
        Clock_t::time_point timepoint_ = Clock_t::now();
        ActionType action_ = ActionType::down;
        ButtonCode button_ = ButtonCode::eoe;
        double xpos_ = 0;
        double ypos_ = 0;
    };

    struct EventRel {
        Clock_t::time_point timepoint_ = Clock_t::now();
        double xrel_ = 0;
        double yrel_ = 0;
    };


    class EventAnalyzer {

    public:
        void notify(const Event& e) {
            this->last_event_ = this->this_event_;
            this->this_event_ = e;
        }

    private:
        Event this_event_;
        Event last_event_;
    };

}  // namespace mirinae::mouse


namespace mirinae::touch {

    using Clock_t = std::chrono::steady_clock;

    enum class ActionType { move, down, up };


    struct Event {
        Clock_t::time_point timepoint_ = Clock_t::now();
        ActionType action_ = ActionType::down;
        double xpos_ = 0;
        double ypos_ = 0;
        uint32_t index_ = 0;
    };

}  // namespace mirinae::touch
