#pragma once

#include <memory>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "mirinae/util/text_data.hpp"


namespace mirinae {

    template <typename T>
    T* find_global_ptr(lua_State* const L, const char* name) {
        lua_getglobal(L, name);
        const auto ud_ptr = lua_touserdata(L, -1);
        lua_pop(L, 1);
        if (!ud_ptr)
            return nullptr;

        return static_cast<T*>(ud_ptr);
    }


    class LuaFuncList {

    public:
        LuaFuncList();
        void add(const char* const name, lua_CFunction func);
        const luaL_Reg* data() const;
        void reserve(const size_t s);

    private:
        void append_null_pair();

        std::vector<luaL_Reg> data_;
    };


    class ScriptEngine {

    public:
        ScriptEngine();
        ~ScriptEngine();

        void exec(const char* script);
        void register_module(const char* name, lua_CFunction funcs);
        void register_global_ptr(const char* name, void* ptr);

        // Might be nullptr
        ITextStream* output_buf() const;
        void replace_output_buf(std::shared_ptr<ITextStream> texts);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
