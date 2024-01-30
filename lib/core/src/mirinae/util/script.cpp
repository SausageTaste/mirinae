#include "mirinae/util/script.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}


namespace {

    class LuaState {

    public:
        LuaState() {
            state_ = luaL_newstate();
            luaL_openlibs(state_);

            luaL_dostring(state_, "print('Hello, Lua script!')");
        }

        ~LuaState() {
            lua_close(state_);
            state_ = nullptr;
        }

        operator lua_State* () {
            return state_;
        }

    private:
        lua_State* state_;

    };

}


namespace mirinae {

    class ScriptEngine::Impl {

    public:
        LuaState state_;

    };


    ScriptEngine::ScriptEngine()
        : pimpl_(std::make_unique<Impl>())
    {

    }

    ScriptEngine::~ScriptEngine() = default;

}
