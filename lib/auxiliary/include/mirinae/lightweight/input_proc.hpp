#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "mirinae/lightweight/uinput.hpp"


namespace mirinae {

    class IInputProcessor {

    public:
        virtual ~IInputProcessor() = default;
        virtual bool on_key_event(const key::Event& e) { return false; }
        virtual bool on_text_event(char32_t c) { return false; }
        virtual bool on_mouse_event(const mouse::Event& e) { return false; }
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

}  // namespace mirinae
