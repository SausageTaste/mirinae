#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mirinae/lightweight/create_info.hpp"
#include "mirinae/lightweight/input_proc.hpp"
#include "mirinae/platform/filesys.hpp"


namespace mirinae {

    class IEngine : public IInputProcessor {

    public:
        virtual ~IEngine() = default;

        virtual void do_frame() = 0;
        virtual bool is_ongoing() = 0;

        virtual void notify_window_resize(uint32_t width, uint32_t height) = 0;
    };


    std::unique_ptr<IEngine> create_engine(
        mirinae::EngineCreateInfo&& create_info
    );

}  // namespace mirinae
