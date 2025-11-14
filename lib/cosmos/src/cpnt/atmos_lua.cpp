#include "mirinae/cpnt/atmos.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/lua/tools.hpp"


#define GET_ENTT_PTR()                                              \
    mirinae::LuaStateView ll{ L };                                  \
    const auto entt_ptr = ll.find_global_ptr("__mirinae_entt_reg"); \
    if (!entt_ptr)                                                  \
        return ll.error("Entity registry pointer not found");       \
    auto& reg = *static_cast<entt::registry*>(entt_ptr);


namespace {

    using StructType = mirinae::cpnt::AtmosphereEpic;
    using UDataType = StructType*;

    const char* const UDATA_ID = "mirinae.AtmosphereEpic";

    auto* check_udata(mirinae::LuaStateView ll, const int idx) {
        return ll.check_udata<UDataType>(idx, UDATA_ID);
    }


    int get(lua_State* const L) {
        GET_ENTT_PTR();

        const auto entt_id = luaL_checkinteger(L, 1);
        const auto entity = static_cast<entt::entity>(entt_id);
        if (!reg.valid(entity))
            return ll.error("Entity does not exist");

        if (auto c = reg.try_get<StructType>(entity))
            return ll.return_meta_obj(c, UDATA_ID);

        return ll.error("Component does not exist on the entity");
    }

    int get_or_create(lua_State* const L) {
        GET_ENTT_PTR();

        const auto entt_id = luaL_checkinteger(L, 1);
        const auto entity = static_cast<entt::entity>(entt_id);
        if (!reg.valid(entity))
            return ll.error("Entity does not exist");

        auto c = reg.try_get<StructType>(entity);
        if (nullptr == c)
            c = &reg.emplace<StructType>(entity);

        return ll.return_meta_obj(c, UDATA_ID);
    }

    int get_mie_phase_g(lua_State* const L) {
        GET_ENTT_PTR();

        auto pp_self = check_udata(L, 1);
        if (!pp_self)
            return ll.error("Invalid `self`");
        auto p_self = *pp_self;
        if (!p_self)
            return ll.error("Invalid `self`");
        auto& self = *p_self;

        lua_pushnumber(L, self.params_.mie_phase_g_);
        return 1;
    }

}  // namespace


// AtmosphereEpic
namespace mirinae::cpnt {

    int AtmosphereEpic::lua_module(lua_State* L) {
        mirinae::LuaStateView ll{ L };

        // Class
        {
            mirinae::LuaFuncList methods;
            methods.add("get_mie_phase_g", get_mie_phase_g);
            ll.define_metatable(UDATA_ID, methods);
        }

        // Module
        {
            mirinae::LuaFuncList funcs;
            funcs.add("get", get);
            funcs.add("get_or_create", get_or_create);
            ll.new_lib(funcs);
        }

        return 1;
    }

}  // namespace mirinae::cpnt
