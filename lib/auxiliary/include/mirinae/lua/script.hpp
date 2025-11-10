#pragma once

#include <memory>
#include <vector>

#include "mirinae/lightweight/text_data.hpp"
#include "mirinae/lua/fwd.hpp"


namespace mirinae {

    class ScriptEngine {

    public:
        ScriptEngine();
        ~ScriptEngine();

        void exec(const char* script);
        void register_module(const char* name, void* funcs);
        void register_global_ptr(const char* name, void* ptr);

        // Might be nullptr
        ITextStream* output_buf() const;
        void replace_output_buf(std::shared_ptr<ITextStream> texts);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
