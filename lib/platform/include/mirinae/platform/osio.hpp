#pragma once

#include <optional>
#include <string>


namespace mirinae {

    // Interface for platform-specific I/O functions
    class IOsIoFunctions {

    public:
        virtual ~IOsIoFunctions() = default;

        virtual bool toggle_fullscreen() = 0;
        virtual bool set_hidden_mouse_mode(bool hidden) = 0;

        virtual std::optional<std::string> get_clipboard() = 0;
        virtual bool set_clipboard(const std::string& text) = 0;
    };

}  // namespace mirinae
