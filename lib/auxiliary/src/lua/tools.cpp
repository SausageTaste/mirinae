#include "mirinae/lua/tools.hpp"

#include "mirinae/lightweight/include_spdlog.hpp"


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


// LuaStateView
namespace mirinae {

    void LuaStateView::set_global_ptr(const char* name, void* ptr) {
        lua_pushlightuserdata(L_, ptr);
        lua_setglobal(L_, name);
    }

    void* LuaStateView::find_global_ptr(const char* name) {
        lua_getglobal(L_, name);
        const auto ud_ptr = lua_touserdata(L_, -1);
        lua_pop(L_, 1);
        if (!ud_ptr)
            return nullptr;

        return ud_ptr;
    }

    void LuaStateView::define_metatable(
        const char* name, const luaL_Reg* functions
    ) {
        luaL_newmetatable(L_, name);
        lua_pushstring(L_, "__index");
        lua_pushvalue(L_, -2); /* pushes the metatable */
        lua_settable(L_, -3);  /* metatable.__index = metatable */
        this->set_func_to_table(functions, 0);
    }

    void LuaStateView::define_metatable(
        const char* name, const LuaFuncList& functions
    ) {
        this->define_metatable(name, functions.data());
    }

    void LuaStateView::new_lib(const luaL_Reg* const functions) {
        luaL_newlib(L_, functions);
    }

    void LuaStateView::new_lib(const LuaFuncList& functions) {
        this->new_lib(functions.data());
    }

    void* LuaStateView::push_meta_obj(const char* type_name, size_t size) {
        const auto ud = lua_newuserdata(L_, size);
        luaL_getmetatable(L_, type_name);
        lua_setmetatable(L_, -2);
        return ud;
    }

    // Private methods

    void LuaStateView::set_func_to_table(const luaL_Reg* l, int nup) {
        for (; l->name; l++) {
            int i;
            lua_pushstring(L_, l->name);
            for (i = 0; i < nup; i++) /* copy upvalues to the top */
                lua_pushvalue(L_, -(nup + 1));
            lua_pushcclosure(L_, l->func, nup);
            lua_settable(L_, -(nup + 3));
        }
        lua_pop(L_, nup); /* remove upvalues */
    }

}  // namespace mirinae
