#include "mirinae/scene/scene.hpp"

#include "mirinae/scene/transform.hpp"


#define GET_SCENE_PTR()                                  \
    const auto scene_ptr = ::find_scene_ptr(L);          \
    if (!scene_ptr)                                      \
        return luaL_error(L, "Scene pointer not found"); \
    auto& scene = *scene_ptr;                            \
    auto& reg = scene.reg_;


namespace {

    union entt_cast_t {
        entt::entity enttid_;
        void* ptr_;
    };

    void* entt_cast(const entt::entity enttid) {
        static_assert(sizeof(void*) >= sizeof(entt::entity));
        entt_cast_t cast;
        cast.ptr_ = nullptr;
        cast.enttid_ = enttid;
        return cast.ptr_;
    }

    entt::entity entt_cast(const void* const ptr) {
        static_assert(sizeof(void*) >= sizeof(entt::entity));
        entt_cast_t cast;
        cast.ptr_ = const_cast<void*>(ptr);
        return cast.enttid_;
    }


    mirinae::Scene* find_scene_ptr(lua_State* const L) {
        const auto usrptr = mirinae::find_global_ptr(L, "__mirinae_scene_ptr");
        const auto scene = static_cast<mirinae::Scene*>(usrptr);
        if (!scene)
            return nullptr;
        if (scene->magic_num_ != mirinae::Scene::MAGIC_NUM)
            return nullptr;
        return scene;
    }


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
    auto& push_meta_obj(lua_State* const L, const char* const type_name) {
        const auto ud = lua_newuserdata(L, sizeof(T));
        luaL_getmetatable(L, type_name);
        lua_setmetatable(L, -2);

        const auto ud_ptr = static_cast<T*>(ud);
        return *ud_ptr;
    }

}  // namespace


namespace { namespace scene {

    namespace cpnt = mirinae::cpnt;


    // TransformView
    namespace tview {

        using Transform = mirinae::cpnt::Transform;
        using UdataType = Transform*;

        const char* const UDATA_ID = "mirinae.transform_view";

        auto& check_udata(lua_State* const L, const int idx) {
            void* const ud = luaL_checkudata(L, idx, UDATA_ID);
            return *static_cast<UdataType*>(ud);
        }


        int get_pos(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);
            lua_pushnumber(L, self->pos_.x);
            lua_pushnumber(L, self->pos_.y);
            lua_pushnumber(L, self->pos_.z);
            return 3;
        }

        int set_pos(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);
            self->pos_.x = luaL_checknumber(L, 2);
            self->pos_.y = luaL_checknumber(L, 3);
            self->pos_.z = luaL_checknumber(L, 4);
            return 0;
        }

        int get_scale(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);
            lua_pushnumber(L, self->scale_.x);
            lua_pushnumber(L, self->scale_.y);
            lua_pushnumber(L, self->scale_.z);
            return 3;
        }

        int set_scale(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);
            self->scale_.x = luaL_checknumber(L, 2);
            self->scale_.y = luaL_checknumber(L, 3);
            self->scale_.z = luaL_checknumber(L, 4);
            return 0;
        }

    }  // namespace tview


    // Entity
    namespace entity {

        using UdataType = entt::entity;

        const char* const UDATA_ID = "mirinae.entity";

        auto& check_udata(lua_State* const L, const int idx) {
            void* const ud = luaL_checkudata(L, idx, UDATA_ID);
            return *static_cast<UdataType*>(ud);
        }


        int get_respath(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);

            if (auto c = reg.try_get<cpnt::StaticModelActor>(self)) {
                lua_pushstring(L, c->model_path_.u8string().c_str());
                return 1;
            } else if (auto c = reg.try_get<cpnt::SkinnedModelActor>(self)) {
                lua_pushstring(L, c->model_path_.u8string().c_str());
                return 1;
            } else {
                return 0;
            }
        }

        int get_transform(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);

            if (auto c = reg.try_get<tview::Transform>(self)) {
                auto& o = ::push_meta_obj<tview::UdataType>(L, tview::UDATA_ID);
                o = c;
                return 1;
            } else {
                return 0;
            }
        }

    }  // namespace entity


    int create_static_model(lua_State* const L) {
        GET_SCENE_PTR();
        const auto model_path = luaL_checkstring(L, 1);
        const auto enttid = reg.create();

        {
            scene.entt_without_model_.push_back(enttid);

            auto& mactor = reg.emplace<cpnt::StaticModelActor>(enttid);
            mactor.model_path_.assign(model_path);

            auto& trans = reg.emplace<cpnt::Transform>(enttid);
        }

        auto& o = ::push_meta_obj<entity::UdataType>(L, entity::UDATA_ID);
        o = enttid;
        return 1;
    }

    int create_skinned_model(lua_State* const L) {
        GET_SCENE_PTR();
        const auto model_path = luaL_checkstring(L, 1);
        const auto enttid = reg.create();

        {
            scene.entt_without_model_.push_back(enttid);

            auto& mactor = reg.emplace<cpnt::SkinnedModelActor>(enttid);
            mactor.model_path_.assign(model_path);

            auto& trans = reg.emplace<cpnt::Transform>(enttid);
        }

        auto& o = ::push_meta_obj<entity::UdataType>(L, entity::UDATA_ID);
        o = enttid;
        return 1;
    }


    int luaopen_scene(lua_State* const L) {
        // TransformView
        {
            mirinae::LuaFuncList methods;
            methods.add("get_pos", tview::get_pos);
            methods.add("set_pos", tview::set_pos);
            methods.add("get_scale", tview::get_scale);
            methods.add("set_scale", tview::set_scale);

            ::add_metatable_definition(L, tview::UDATA_ID, methods.data());
        }

        // Entity
        {
            mirinae::LuaFuncList methods;
            methods.add("get_respath", entity::get_respath);
            methods.add("get_transform", entity::get_transform);

            ::add_metatable_definition(L, entity::UDATA_ID, methods.data());
        }

        // Module
        {
            mirinae::LuaFuncList funcs;
            funcs.add("create_static_model", create_static_model);
            funcs.add("create_skinned_model", create_skinned_model);
            luaL_newlib(L, funcs.data());
        }

        return 1;
    }

}}  // namespace ::scene


namespace mirinae {

    Scene::Scene(ScriptEngine& script) : script_(script) {
        script_.register_global_ptr("__mirinae_scene_ptr", this);
        script_.register_module("scene", ::scene::luaopen_scene);
    }

}  // namespace mirinae
