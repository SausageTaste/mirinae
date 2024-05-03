#pragma once


namespace mirinae {

    // Interface for platform-specific I/O functions
    class IOsIoFunctions {

    public:
        virtual ~IOsIoFunctions() = default;

        virtual bool set_hidden_mouse_mode(bool hidden) = 0;

    };

}
