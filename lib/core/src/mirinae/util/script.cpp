#include "mirinae/util/script.hpp"

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
        lua_getglobal(L, "__mirinae_script_ptr");
        const auto ud_ptr = lua_touserdata(L, -1);
        lua_pop(L, 1);
        if (!ud_ptr)
            return nullptr;

        return static_cast<mirinae::ScriptEngine*>(ud_ptr);
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
        if (const auto buf = find_output_buf(L)) {
            const auto nargs = lua_gettop(L);
            for (int i = 1; i <= nargs; ++i) {
                if (i > 1)
                    buf->append(' ');

                luaL_tolstring(L, i, nullptr);
                if (const auto str = lua_tostring(L, -1)) {
                    buf->append(str);
                } else {
                    buf->append("nil");
                }
                lua_pop(L, 1);
            }
            buf->append('\n');
        } else {
            fmt::print("{}\n", lua_tostring(L, 1));
        }

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

    void push_lua_constant(lua_State* L, const char* name, lua_Number value) {
        lua_pushstring(L, name);
        lua_pushnumber(L, value);
        lua_settable(L, -3);
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
        LuaState state_;
        std::shared_ptr<ITextStream> output_buf_;
    };


    ScriptEngine::ScriptEngine() : pimpl_(std::make_unique<Impl>()) {
        auto L = pimpl_->state_.operator lua_State*();
        lua_pushlightuserdata(L, this);
        lua_setglobal(L, "__mirinae_script_ptr");

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

    ITextStream* ScriptEngine::output_buf() const {
        return pimpl_->output_buf_.get();
    }

    void ScriptEngine::replace_output_buf(std::shared_ptr<ITextStream> texts) {
        pimpl_->output_buf_ = texts;
    }

}  // namespace mirinae
