#include "mirinae/cpnt/atmos.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/lua/tools.hpp"


#define GET_ENTT_PTR()                                                       \
    const auto entt_ptr = mirinae::find_global_ptr(L, "__mirinae_entt_reg"); \
    if (!entt_ptr)                                                           \
        return luaL_error(L, "Entity registry pointer not found");           \
    auto& reg = *static_cast<entt::registry*>(entt_ptr);


namespace {

    void set_lua_func_to_table(lua_State* L, const luaL_Reg* l, int nup) {
        for (; l->name; l++) {
            int i;
            lua_pushstring(L, l->name);
            for (i = 0; i < nup; i++) /* copy upvalues to the top */
                lua_pushvalue(L, -(nup + 1));
            lua_pushcclosure(L, l->func, nup);
            lua_settable(L, -(nup + 3));
        }
        lua_pop(L, nup); /* remove upvalues */
    }

    void add_metatable_definition(
        lua_State* const L,
        const char* const name,
        const luaL_Reg* const functions
    ) {
        luaL_newmetatable(L, name);
        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2); /* pushes the metatable */
        lua_settable(L, -3);  /* metatable.__index = metatable */
        ::set_lua_func_to_table(L, functions, 0);
    }

    template <typename T>
    T& check_udata(lua_State* L, int idx, const char* type_name) {
        void* ud = luaL_checkudata(L, idx, type_name);
        return *static_cast<T*>(ud);
    }

    template <typename T>
    auto& push_meta_obj(lua_State* const L, const char* const type_name) {
        const auto ud = lua_newuserdata(L, sizeof(T));
        luaL_getmetatable(L, type_name);
        lua_setmetatable(L, -2);

        const auto ud_ptr = static_cast<T*>(ud);
        return *ud_ptr;
    }

}  // namespace


namespace {

    using StructType = mirinae::cpnt::AtmosphereEpic;
    using UDataType = StructType*;

    const char* const UDATA_ID = "mirinae.AtmosphereEpic";

    auto& check_udata(lua_State* const L, const int idx) {
        return check_udata<UDataType>(L, idx, UDATA_ID);
    }


    int get_or_create(lua_State* const L) {
        GET_ENTT_PTR();

        const auto entt_id = luaL_checkinteger(L, 1);
        const auto entity = static_cast<entt::entity>(entt_id);
        if (not reg.valid(entity))
            return luaL_error(L, "Entity does not exist.");

        auto c = reg.try_get<StructType>(entity);
        c = &reg.emplace<StructType>(entity);

        auto& out = push_meta_obj<UDataType>(L, UDATA_ID);
        out = c;
        return 1;
    }

    int get_mie_phase_g(lua_State* const L) {
        GET_ENTT_PTR();
        auto& self = check_udata(L, 1);
        lua_pushnumber(L, self->params_.mie_phase_g_);
        return 1;
    }

}  // namespace


// AtmosphereEpic
namespace mirinae::cpnt {

    int AtmosphereEpic::lua_module(lua_State* L) {
        // Class
        {
            mirinae::LuaFuncList methods;
            methods.add("get_mie_phase_g", get_mie_phase_g);
            ::add_metatable_definition(L, UDATA_ID, methods.data());
        }

        // Module
        {
            mirinae::LuaFuncList funcs;
            funcs.add("get_or_create", get_or_create);
            luaL_newlib(L, funcs.data());
        }

        return 1;
    }

}  // namespace mirinae::cpnt
