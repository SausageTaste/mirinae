#pragma once

#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>

#include "mirinae/lightweight/uinput.hpp"
#include "mirinae/platform/osio.hpp"


namespace mirinae {

    class IInputProcessor {

    public:
        virtual ~IInputProcessor() = default;
        virtual bool on_key_event(const key::Event& e) { return false; }
        virtual bool on_text_event(char32_t c) { return false; }
        virtual bool on_mouse_event(const mouse::Event& e) { return false; }
        virtual bool on_mouse_rel_event(const mouse::EventRel& e) {
            return false;
        }
        virtual bool on_touch_event(const touch::Event& e) { return false; }
    };


    class InputProcesserMgr : public IInputProcessor {

    public:
        bool on_key_event(const key::Event& e) override {
            for (auto& x : items_) {
                switch (x.index()) {
                    case 0:
                        if (std::get<0>(x)->on_key_event(e))
                            return true;
                        break;
                    case 1:
                        if (std::get<1>(x)->on_key_event(e))
                            return true;
                        break;
                }
            }
            return false;
        }

        bool on_text_event(char32_t e) override {
            for (auto& x : items_) {
                switch (x.index()) {
                    case 0:
                        if (std::get<0>(x)->on_text_event(e))
                            return true;
                        break;
                    case 1:
                        if (std::get<1>(x)->on_text_event(e))
                            return true;
                        break;
                }
            }
            return false;
        }

        bool on_mouse_event(const mouse::Event& e) override {
            for (auto& x : items_) {
                switch (x.index()) {
                    case 0:
                        if (std::get<0>(x)->on_mouse_event(e))
                            return true;
                        break;
                    case 1:
                        if (std::get<1>(x)->on_mouse_event(e))
                            return true;
                        break;
                }
            }
            return false;
        }

        void add(IInputProcessor* proc) { this->items_.emplace_back(proc); }

        void add(std::unique_ptr<IInputProcessor>&& proc) {
            this->items_.emplace_back(std::move(proc));
        }

    private:
        using Item_t =
            std::variant<IInputProcessor*, std::unique_ptr<IInputProcessor>>;

        std::vector<Item_t> items_;
    };


    class InputActionMapper : public IInputProcessor {

    public:
        enum class ActionType {
            // Move with respect to view direction
            move_forward,
            move_backward,
            move_left,
            move_right,

            look_up,
            look_down,
            look_left,
            look_right,

            // Move with respect to world direction
            translate_up,    // Jump, or ascend
            translate_down,  // Descend
            sprint,
            walk,
        };

        InputActionMapper();

        void give_osio(IOsIoFunctions& osio) { osio_ = &osio; }

        bool on_key_event(const key::Event& e) override;
        bool on_text_event(char32_t c) override;
        bool on_mouse_event(const mouse::Event& e) override;
        bool on_mouse_rel_event(const mouse::EventRel& e) override;
        bool on_touch_event(const touch::Event& e) override;

        double get_value(ActionType action) const;
        double get_value_move_backward() const;
        double get_value_move_right() const;

        // Left, up
        glm::dvec2 get_value_key_look() const;
        // Left, up
        glm::dvec2 get_value_mouse_look() const;

        int get_mwheel_zoom() const;

        void finish_frame();

    private:
        class PointerState {

        public:
            glm::dvec2 start_pos_{ 0, 0 };
            glm::dvec2 last_pos_{ 0, 0 };
            glm::dvec2 consumed_pos_{ 0, 0 };
            glm::dvec2 smoothed_rel_{ 0, 0 };
        };

        IOsIoFunctions* osio_ = nullptr;
        std::unordered_map<int, ActionType> key_map_;
        std::unordered_map<ActionType, double> action_values_;
        PointerState mouse_state_;
        PointerState* move_pointer_ = nullptr;
        PointerState* look_pointer_ = nullptr;
        int mwheel_up_ = false;
        int mwheel_down_ = false;
    };

}  // namespace mirinae
