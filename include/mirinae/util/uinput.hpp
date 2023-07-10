#pragma once

#include <array>
#include <chrono>


namespace mirinae::key {

    enum class ActionType { down, up };

    enum class KeyCode {
        /* Alphabets */
        a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z,
        /* Horizontal numbers */
        n0, n1, n2, n3, n4, n5, n6, n7, n8, n9,
        /* Misc in keyboard main area */
        backquote, minus, equal, lbracket, rbracket, backslash, semicolon, quote, comma, period, slash,
        /* Special characters */
        space, enter, backspace, tab,
        /* No characters */
        escape, lshfit, rshfit, lctrl, rctrl, lalt, ralt, up, down, left, right,
        /* End Of Enum, just for calculating number of elements of Enum class */
        eoe
    };


    // TODO: Rename this to KeyActionType.
    struct Event {
        std::chrono::steady_clock::time_point timepoint = std::chrono::steady_clock::now();
        ActionType action_type = ActionType::down;
        KeyCode key = KeyCode::eoe;
    };


    class EventAnalyzer {

    public:
        struct KeyState {
            std::chrono::steady_clock::time_point timepoint = std::chrono::steady_clock::now();
            bool pressed = false;
        };

    public:
        void notify(const Event& e) {
            const auto index = this->convert_key_to_index(e.key);
            if (index >= this->states.size())
                return;

            this->states[index].timepoint = e.timepoint;
            this->states[index].pressed = (e.action_type == ActionType::down);
        }

        KeyState& operator[](const KeyCode key) {
            return this->states[this->convert_key_to_index(key)];
        }

        const KeyState& operator[](const KeyCode key) const {
            return this->states[this->convert_key_to_index(key)];
        }

    private:
        static size_t convert_key_to_index(const KeyCode key) {
            return static_cast<size_t>(key);
        }

        static constexpr auto KEY_SPEC_SIZE_U = static_cast<unsigned int>(KeyCode::eoe);
        std::array<KeyState, KEY_SPEC_SIZE_U> states;

    };

}
