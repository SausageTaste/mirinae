#pragma once

#include <memory>

#include <daltools/filesys/filesys.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/system/imgui.hpp"


namespace mirinae::imgui {

    struct IMainWin : public Widget {
        virtual void toggle_show() = 0;
    };

    std::shared_ptr<IMainWin> create_main_win(
        mirinae::HCosmos cosmos,
        dal::HFilesys filesys,
        std::shared_ptr<mirinae::ScriptEngine> script
    );

}  // namespace mirinae::imgui
