#pragma once

#include <memory>

#include "mirinae/util/uinput.hpp"


namespace mirinae {

    class IEngine {

    public:
        virtual ~IEngine() = default;

        virtual void do_frame() = 0;
        virtual bool is_ongoing() = 0;

        virtual void notify_window_resize(unsigned width, unsigned height) = 0;
        virtual void notify_key_event(const key::Event& e) = 0;

    };


    std::unique_ptr<IEngine> create_engine();

}
