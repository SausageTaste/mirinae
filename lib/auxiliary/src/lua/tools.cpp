#include "mirinae/lua/tools.hpp"

#include "mirinae/lightweight/include_spdlog.hpp"


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
