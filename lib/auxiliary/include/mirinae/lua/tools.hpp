#pragma once

#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}


namespace mirinae {

    void* find_global_ptr(lua_State* const L, const char* name);


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

}  // namespace mirinae
