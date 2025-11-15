#include "mirinae/lua/script.hpp"

#include <sstream>

#include <sung/basic/time.hpp>

#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lua/tools.hpp"


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

        operator lua_State*() { return state_; }

    private:
        lua_State* state_;
    };


    mirinae::ScriptEngine* find_script_engine(mirinae::LuaStateView ll) {
        if (auto p = ll.find_global_ptr("__mirinae_script_ptr")) {
            return static_cast<mirinae::ScriptEngine*>(p);
        }
        return nullptr;
    }

    mirinae::ITextStream* find_output_buf(mirinae::LuaStateView ll) {
        const auto script = find_script_engine(ll);
        if (!script)
            return nullptr;

        return script->output_buf();
    }

}  // namespace


// Lua module: global
namespace { namespace lua { namespace global {

    // print(str)
    int print(lua_State* const L) {
        std::stringstream ss;

        const auto nargs = lua_gettop(L);
        for (int i = 1; i <= nargs; ++i) {
            if (i > 1)
                ss << ' ';

            luaL_tolstring(L, i, nullptr);
            if (const auto str = lua_tostring(L, -1)) {
                ss << str;
            } else {
                ss << "nil";
            }
            lua_pop(L, 1);
        }

        const auto str = ss.str();
        if (const auto buf = find_output_buf(L)) {
            buf->append(str);
            buf->append('\n');
        }
        SPDLOG_INFO("Lua print: {}", str);

        return 0;
    }

    int time(lua_State* const L) {
        const auto sec = sung::get_time_unix();
        lua_pushnumber(L, sec);
        return 1;
    }

}}}  // namespace ::lua::global


namespace mirinae {

    class ScriptEngine::Impl {

    public:
        lua_State* L() { return state_.operator lua_State*(); }

        LuaState state_;
        std::shared_ptr<ITextStream> output_buf_;
    };


    ScriptEngine::ScriptEngine() : pimpl_(std::make_unique<Impl>()) {
        auto L = pimpl_->state_.operator lua_State*();
        this->register_global_ptr("__mirinae_script_ptr", this);

        LuaFuncList funcs;
        funcs.add("print", ::lua::global::print);
        funcs.add("time", ::lua::global::time);
        lua_getglobal(L, "_G");
        luaL_setfuncs(L, funcs.data(), 0);
        lua_pop(L, 1);
    }

    ScriptEngine::~ScriptEngine() = default;

    void ScriptEngine::exec(const char* script) {
        if (luaL_dostring(pimpl_->state_, script)) {
            const auto err = lua_tostring(pimpl_->state_, -1);
            SPDLOG_WARN("Lua error: {}", err);
            if (auto buf = pimpl_->output_buf_.get()) {
                buf->append(err);
                buf->append('\n');
            }
        }
    }

    void ScriptEngine::register_module(const char* name, luaCFunc_t func) {
        luaL_requiref(pimpl_->state_, name, func, 0);
    }

    void ScriptEngine::register_global_ptr(const char* name, void* ptr) {
        mirinae::LuaStateView ll{ pimpl_->L() };
        ll.set_global_ptr(name, ptr);

        SPDLOG_DEBUG(
            "Registered global pointer: {} -> {}",
            name,
            reinterpret_cast<size_t>(ptr)
        );
    }

    ITextStream* ScriptEngine::output_buf() const {
        return pimpl_->output_buf_.get();
    }

    void ScriptEngine::replace_output_buf(std::shared_ptr<ITextStream> texts) {
        pimpl_->output_buf_ = texts;
    }

}  // namespace mirinae
