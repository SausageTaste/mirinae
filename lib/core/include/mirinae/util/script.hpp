#pragma once

#include <memory>


namespace mirinae {

    class ScriptEngine {

    public:
        ScriptEngine();
        ~ScriptEngine();

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
