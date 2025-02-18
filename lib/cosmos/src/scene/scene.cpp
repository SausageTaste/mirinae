#include "mirinae/scene/scene.hpp"

#include <sung/basic/aabb.hpp>

#include "mirinae/cpnt/camera.hpp"
#include "mirinae/cpnt/identifier.hpp"
#include "mirinae/cpnt/ocean.hpp"
#include "mirinae/cpnt/ren_model.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"


#define GET_SCENE_PTR()                                  \
    const auto scene_ptr = ::find_scene_ptr(L);          \
    if (!scene_ptr)                                      \
        return luaL_error(L, "Scene pointer not found"); \
    auto& scene = *scene_ptr;                            \
    auto& reg = scene.reg_;


namespace {

    const char* const SCENE_PTR_NAME = "__mirinae_cosmos_scene_ptr";

    mirinae::Scene* find_scene_ptr(lua_State* const L) {
        const auto usrptr = mirinae::find_global_ptr(L, SCENE_PTR_NAME);
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

        int rotate(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);
            const auto angle = luaL_checknumber(L, 2);
            const auto x_axis = luaL_checknumber(L, 3);
            const auto y_axis = luaL_checknumber(L, 4);
            const auto z_axis = luaL_checknumber(L, 5);
            self->rotate(
                Transform::Angle::from_deg(angle),
                glm::vec3{ x_axis, y_axis, z_axis }
            );
            return 0;
        }

        int get_quat(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);
            lua_pushnumber(L, self->rot_.w);
            lua_pushnumber(L, self->rot_.x);
            lua_pushnumber(L, self->rot_.y);
            lua_pushnumber(L, self->rot_.z);
            return 4;
        }

        int set_quat(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);
            self->rot_.w = luaL_checknumber(L, 2);
            self->rot_.x = luaL_checknumber(L, 3);
            self->rot_.y = luaL_checknumber(L, 4);
            self->rot_.z = luaL_checknumber(L, 5);
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

            if (lua_gettop(L) == 2) {
                self->scale_.x = luaL_checknumber(L, 2);
                self->scale_.y = luaL_checknumber(L, 2);
                self->scale_.z = luaL_checknumber(L, 2);
            } else {
                self->scale_.x = luaL_checknumber(L, 2);
                self->scale_.y = luaL_checknumber(L, 3);
                self->scale_.z = luaL_checknumber(L, 4);
            }

            return 0;
        }

    }  // namespace tview


    // AnimStateView
    namespace animv {

        using UdataType = mirinae::SkinAnimState*;

        const char* const UDATA_ID = "mirinae.anim_state_view";

        auto& check_udata(lua_State* const L, const int idx) {
            void* const ud = luaL_checkudata(L, idx, UDATA_ID);
            return *static_cast<UdataType*>(ud);
        }


        int get_cur_anim_idx(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = *check_udata(L, 1);

            if (const auto idx = self.get_cur_anim_idx())
                lua_pushinteger(L, *idx);
            else
                lua_pushnil(L);

            return 1;
        }

        int get_cur_anim_name(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = *check_udata(L, 1);

            if (const auto name = self.get_cur_anim_name())
                lua_pushstring(L, name->c_str());
            else
                lua_pushnil(L);

            return 1;
        }

        int get_anim_count(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = *check_udata(L, 1);
            lua_pushinteger(L, self.anims().size());
            return 1;
        }

        int get_anim_names(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = *check_udata(L, 1);

            lua_newtable(L);
            for (size_t i = 0; i < self.anims().size(); ++i) {
                lua_pushstring(L, self.anims()[i].name_.c_str());
                lua_rawseti(L, -2, i + 1);
            }

            return 1;
        }

        int get_anim_name_by_idx(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = *check_udata(L, 1);
            const auto anim_index = luaL_checkinteger(L, 2);

            if (anim_index < 0 || anim_index >= self.anims().size())
                return 0;

            lua_pushstring(L, self.anims()[anim_index].name_.c_str());
            return 1;
        }

        int set_anim_name(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = *check_udata(L, 1);
            const auto anim_name = luaL_checkstring(L, 2);

            self.select_anim_name(anim_name, scene.clock());
            return 0;
        }

        int set_anim_idx(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = *check_udata(L, 1);
            const auto anim_index = luaL_checkinteger(L, 2);

            if (anim_index < 0)
                self.deselect_anim(scene.clock());
            else
                self.select_anim_index((size_t)anim_index, scene.clock());

            return 0;
        }

        int set_anim_speed(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = *check_udata(L, 1);
            const auto speed = luaL_checknumber(L, 2);

            self.set_play_speed(speed);
            return 0;
        }

    }  // namespace animv


    // Entity
    namespace entity {

        using UdataType = entt::entity;

        const char* const UDATA_ID = "mirinae.entity";

        auto& check_udata(lua_State* const L, const int idx) {
            void* const ud = luaL_checkudata(L, idx, UDATA_ID);
            return *static_cast<UdataType*>(ud);
        }


        int get_id(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);
            lua_pushinteger(L, static_cast<lua_Integer>(self));
            return 1;
        }

        int get_respath(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);

            if (auto c = reg.try_get<cpnt::MdlActorStatic>(self)) {
                lua_pushstring(L, c->model_path_.u8string().c_str());
                return 1;
            } else if (auto c = reg.try_get<cpnt::MdlActorSkinned>(self)) {
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

        int get_anim_state(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);

            auto mactor = reg.try_get<cpnt::MdlActorSkinned>(self);
            if (!mactor)
                return luaL_error(L, "This entity is not a skinned model.");

            auto& o = ::push_meta_obj<animv::UdataType>(L, animv::UDATA_ID);
            o = &mactor->anim_state_;
            return 1;
        }

    }  // namespace entity


    int get_entt_by_id(lua_State* const L) {
        GET_SCENE_PTR();
        const auto id = luaL_checkinteger(L, 1);
        const auto entity = static_cast<entt::entity>(id);

        if (reg.valid(entity)) {
            auto& o = ::push_meta_obj<entity::UdataType>(L, entity::UDATA_ID);
            o = entity;
            return 1;
        } else {
            return 0;
        }
    }

    int get_cam_pos(lua_State* const L) {
        GET_SCENE_PTR();
        auto cam = reg.try_get<cpnt::Transform>(scene.main_camera_);

        if (cam) {
            lua_pushnumber(L, cam->pos_.x);
            lua_pushnumber(L, cam->pos_.y);
            lua_pushnumber(L, cam->pos_.z);
        } else {
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
        }
        return 3;
    }

    int get_cam_quat(lua_State* const L) {
        GET_SCENE_PTR();
        auto cam = reg.try_get<cpnt::Transform>(scene.main_camera_);

        if (cam) {
            lua_pushnumber(L, cam->rot_.w);
            lua_pushnumber(L, cam->rot_.x);
            lua_pushnumber(L, cam->rot_.y);
            lua_pushnumber(L, cam->rot_.z);
        } else {
            lua_pushnumber(L, 1);
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
        }
        return 4;
    }

    int get_cam_dir(lua_State* const L) {
        GET_SCENE_PTR();
        auto cam = reg.try_get<cpnt::Transform>(scene.main_camera_);

        if (cam) {
            const auto dir = cam->make_forward_dir();
            lua_pushnumber(L, dir.x);
            lua_pushnumber(L, dir.y);
            lua_pushnumber(L, dir.z);
        } else {
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
        }

        return 3;
    }

    int create_static_actor(lua_State* const L) {
        GET_SCENE_PTR();
        const auto model_path = luaL_checkstring(L, 1);
        const auto enttid = reg.create();

        {
            scene.entt_without_model_.push_back(enttid);

            auto& mactor = reg.emplace<cpnt::MdlActorStatic>(enttid);
            mactor.model_path_.assign(model_path);

            auto& id = reg.emplace<cpnt::Id>(enttid);
            id.set_name(mactor.model_path_.filename().u8string().c_str());

            auto& trans = reg.emplace<cpnt::Transform>(enttid);
        }

        auto& o = ::push_meta_obj<entity::UdataType>(L, entity::UDATA_ID);
        o = enttid;
        return 1;
    }

    int create_skinned_actor(lua_State* const L) {
        GET_SCENE_PTR();
        const auto model_path = luaL_checkstring(L, 1);
        const auto enttid = reg.create();

        {
            scene.entt_without_model_.push_back(enttid);

            auto& mactor = reg.emplace<cpnt::MdlActorSkinned>(enttid);
            mactor.model_path_.assign(model_path);

            auto& id = reg.emplace<cpnt::Id>(enttid);
            id.set_name(mactor.model_path_.filename().u8string().c_str());

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
            methods.add("rotate", tview::rotate);
            methods.add("get_quat", tview::get_quat);
            methods.add("set_quat", tview::set_quat);
            methods.add("get_scale", tview::get_scale);
            methods.add("set_scale", tview::set_scale);

            ::add_metatable_definition(L, tview::UDATA_ID, methods.data());
        }

        // AnimStateView
        {
            mirinae::LuaFuncList methods;
            methods.add("get_cur_anim_idx", animv::get_cur_anim_idx);
            methods.add("get_cur_anim_name", animv::get_cur_anim_name);
            methods.add("get_anim_count", animv::get_anim_count);
            methods.add("get_anim_names", animv::get_anim_names);
            methods.add("get_anim_name_by_idx", animv::get_anim_name_by_idx);
            methods.add("set_anim_name", animv::set_anim_name);
            methods.add("set_anim_idx", animv::set_anim_idx);
            methods.add("set_anim_speed", animv::set_anim_speed);

            ::add_metatable_definition(L, animv::UDATA_ID, methods.data());
        }

        // Entity
        {
            mirinae::LuaFuncList methods;
            methods.add("get_id", entity::get_id);
            methods.add("get_respath", entity::get_respath);
            methods.add("get_transform", entity::get_transform);
            methods.add("get_anim_state", entity::get_anim_state);

            ::add_metatable_definition(L, entity::UDATA_ID, methods.data());
        }

        // Module
        {
            mirinae::LuaFuncList funcs;
            funcs.add("get_entt_by_id", get_entt_by_id);
            funcs.add("get_cam_pos", get_cam_pos);
            funcs.add("get_cam_quat", get_cam_quat);
            funcs.add("get_cam_dir", get_cam_dir);
            funcs.add("create_static_actor", create_static_actor);
            funcs.add("create_skinned_actor", create_skinned_actor);
            luaL_newlib(L, funcs.data());
        }

        return 1;
    }

}}  // namespace ::scene


// Scene
namespace mirinae {

    Scene::Scene(const sung::SimClock& global_clock, ScriptEngine& script)
        : script_(script) {
        clock_.sync_rt(global_clock);

        script_.register_global_ptr(SCENE_PTR_NAME, this);
        script_.register_module("scene", scene::luaopen_scene);
    }

    void Scene::do_frame() {
        clock_.tick();

        reg_.view<cpnt::Ocean>().each([this](cpnt::Ocean& ocean) {
            ocean.do_frame(clock_);
        });

        return;
    }

    void Scene::pick_entt(const sung::LineSegment3& ray) {
        SPDLOG_DEBUG(
            "Ray: ({:.2f}, {:.2f}, {:.2f}) -> ({:.2f}, {:.2f}, {:.2f})",
            ray.pos().x(),
            ray.pos().y(),
            ray.pos().z(),
            ray.end().x(),
            ray.end().y(),
            ray.end().z()
        );
    }

}  // namespace mirinae
