#include "mirinae/util/script.hpp"

#include <spdlog/spdlog.h>


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

}  // namespace


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


    ScriptEngine::ScriptEngine() : pimpl_(std::make_unique<Impl>()) {}

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

    void ScriptEngine::replace_output_buf(std::shared_ptr<ITextStream> texts) {
        pimpl_->output_buf_ = texts;
    }

}  // namespace mirinae
