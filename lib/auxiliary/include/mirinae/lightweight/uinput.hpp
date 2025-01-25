#pragma once

#include <array>
#include <chrono>
#include <optional>


namespace mirinae::key {

    class EventAnalyzer;

    using Clock_t = std::chrono::steady_clock;

    enum class ActionType { down, up };

    // clang-format off

    enum class KeyCode {
        /* Alphabets */
        a, b, c, d, e, f, g, h, i, j, k, l, m,
        n, o, p, q, r, s, t, u, v, w, x, y, z,
        /* Horizontal numbers */
        n0, n1, n2, n3, n4, n5, n6, n7, n8, n9,
        /* Misc in keyboard main area */
        backquote, minus, equal, lbracket, rbracket, backslash, semicolon,
        quote, comma, period, slash,
        /* Special characters */
        space, enter, backspace, tab,
        /* Numpad */
        np0, np1, np2, np3, np4, np5, np6, np7, np8, np9,
        np_mul, np_add, np_sub, np_dot, np_div, num_enter,
        /* No characters */
        escape, lshfit, rshfit, lctrl, rctrl, lalt, ralt, up, down, left, right,
        /* End Of Enum, just for calculating number of elements of Enum class */
        eoe
    };

    // clang-format on


    // TODO: Rename this to KeyActionType.
    struct Event {
        Clock_t::time_point timepoint = Clock_t::now();
        ActionType action_type = ActionType::down;
        KeyCode key = KeyCode::eoe;
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
            const auto index = static_cast<size_t>(e.key);
            if (index >= this->states.size())
                return;

            this->states[index].timepoint = e.timepoint;
            this->states[index].pressed =
                (e.action_type == key::ActionType::down);
        }

        bool is_pressed(KeyCode key) const {
            const auto index = static_cast<size_t>(key);
            if (index >= this->states.size())
                return false;

            return this->states[index].pressed;
        }

        std::optional<Clock_t::time_point> get_timepoint(KeyCode key) const {
            const auto index = static_cast<size_t>(key);
            if (index >= this->states.size())
                return std::nullopt;

            return this->states[index].timepoint;
        }

    private:
        static size_t convert_key_to_index(const KeyCode key) {
            return static_cast<size_t>(key);
        }

        static constexpr auto KEYSPEC_SIZE = (unsigned)(KeyCode::eoe);
        std::array<KeyState, KEYSPEC_SIZE> states;
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
