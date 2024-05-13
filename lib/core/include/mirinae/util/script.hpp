#pragma once

#include <memory>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}


namespace mirinae {

    // The target table must be on top of the stack
    void push_lua_constant(lua_State* L, const char* name, lua_Number value);


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

        void register_module(const char* name, lua_CFunction funcs);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace mirinae
