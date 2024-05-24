#include "mirinae/util/script.hpp"

#include <sstream>

#include <spdlog/spdlog.h>
#include <sung/general/time.hpp>


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


    mirinae::ScriptEngine* find_script_engine(lua_State* const L) {
        if (auto p = mirinae::find_global_ptr(L, "__mirinae_script_ptr")) {
            return static_cast<mirinae::ScriptEngine*>(p);
        }
        return nullptr;
    }

    mirinae::ITextStream* find_output_buf(lua_State* const L) {
        const auto script = find_script_engine(L);
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
        spdlog::info("Lua print: {}", str);

        return 0;
    }

    int time(lua_State* const L) {
        const auto sec = sung::CalenderTime::from_now().to_total_seconds();
        lua_pushnumber(L, sec);
        return 1;
    }

}}}  // namespace ::lua::global


// Functions
namespace mirinae {

    void* find_global_ptr(lua_State* const L, const char* name) {
        lua_getglobal(L, name);
        const auto ud_ptr = lua_touserdata(L, -1);
        lua_pop(L, 1);
        if (!ud_ptr)
            return nullptr;

        return ud_ptr;
    }

}  // namespace mirinae


// LuaFuncList
namespace mirinae {

    LuaFuncList::LuaFuncList() { this->append_null_pair(); }

    void LuaFuncList::add(const char* const name, lua_CFunction func) {
        data_.back().name = name;
        data_.back().func = func;
        this->append_null_pair();
    }

    const luaL_Reg* LuaFuncList::data() const { return data_.data(); }

    void LuaFuncList::reserve(const size_t s) { data_.reserve(s + 1); }

    void LuaFuncList::append_null_pair() {
        data_.emplace_back();
        data_.back().func = nullptr;
        data_.back().name = nullptr;
    }

}  // namespace mirinae


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
            spdlog::warn("Lua error: {}", err);
            if (auto buf = pimpl_->output_buf_.get()) {
                buf->append(err);
                buf->append('\n');
            }
        }
    }

    void ScriptEngine::register_module(const char* name, lua_CFunction funcs) {
        luaL_requiref(pimpl_->state_, name, funcs, 0);
    }

    void ScriptEngine::register_global_ptr(const char* name, void* ptr) {
        auto L = pimpl_->L();
        lua_pushlightuserdata(L, ptr);
        lua_setglobal(L, name);

        spdlog::debug(
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
