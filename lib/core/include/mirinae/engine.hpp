#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mirinae/platform/filesys.hpp"
#include "mirinae/util/create_info.hpp"
#include "mirinae/util/uinput.hpp"


namespace mirinae {

    class IEngine {

    public:
        virtual ~IEngine() = default;

        virtual void do_frame() = 0;
        virtual bool is_ongoing() = 0;

        virtual void notify_window_resize(uint32_t width, uint32_t height) = 0;
        virtual void notify_key_event(const key::Event& e) = 0;
        virtual void notify_mouse_event(const mouse::Event& e) = 0;

    };


    std::unique_ptr<IEngine> create_engine(mirinae::EngineCreateInfo&& create_info);

}
