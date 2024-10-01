#include "mirinae/scene/scene.hpp"

#include <sung/general/aabb.hpp>


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

            self.select_anim_name(anim_name, scene.ftime());
            return 0;
        }

        int set_anim_idx(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = *check_udata(L, 1);
            const auto anim_index = luaL_checkinteger(L, 2);

            if (anim_index < 0)
                self.deselect_anim(scene.ftime());
            else
                self.select_anim_index((size_t)anim_index, scene.ftime());

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

        int get_anim_state(lua_State* const L) {
            GET_SCENE_PTR();
            auto& self = check_udata(L, 1);

            auto mactor = reg.try_get<cpnt::SkinnedModelActor>(self);
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
        auto cam = reg.try_get<cpnt::StandardCamera>(scene.main_camera_);

        if (cam) {
            lua_pushnumber(L, cam->view_.pos_.x);
            lua_pushnumber(L, cam->view_.pos_.y);
            lua_pushnumber(L, cam->view_.pos_.z);
        } else {
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
        }
        return 3;
    }

    int get_cam_quat(lua_State* const L) {
        GET_SCENE_PTR();
        auto cam = reg.try_get<cpnt::StandardCamera>(scene.main_camera_);

        if (cam) {
            lua_pushnumber(L, cam->view_.rot_.w);
            lua_pushnumber(L, cam->view_.rot_.x);
            lua_pushnumber(L, cam->view_.rot_.y);
            lua_pushnumber(L, cam->view_.rot_.z);
        } else {
            lua_pushnumber(L, 1);
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
        }
        return 4;
    }

    int create_static_actor(lua_State* const L) {
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

    int create_skinned_actor(lua_State* const L) {
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
            methods.add("rotate", tview::rotate);
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
            funcs.add("create_static_actor", create_static_actor);
            funcs.add("create_skinned_actor", create_skinned_actor);
            luaL_newlib(L, funcs.data());
        }

        return 1;
    }

}}  // namespace ::scene


// DLight
namespace mirinae::cpnt {

    glm::vec3 DLight::calc_to_light_dir(const glm::dmat4 view_mat) const {
        const auto v = view_mat * transform_.make_model_mat() *
                       glm::dvec4(0, 0, 1, 0);
        return glm::normalize(glm::vec3(v));
    };

    /*
    glm::dmat4 DLight::make_proj_mat() const {
        auto p = glm::orthoRH_ZO<double>(-10, 10, -10, 10, -50, 50);
        return p;
    }

    glm::dmat4 DLight::make_view_mat() const {
        return transform_.make_view_mat();
    }

    glm::dmat4 DLight::make_light_mat() const {
        return make_proj_mat() * make_view_mat();
    }*/

    glm::dmat4 DLight::make_light_mat(const std::array<glm::dvec3, 8>& p
    ) const {
        const auto view_mat = transform_.make_view_mat();

        sung::AABB3<double> aabb;
        aabb.set(p[0].x, p[0].y, p[0].z);

        for (auto& v : p) {
            const auto v4 = view_mat * glm::dvec4(v, 1);
            aabb.expand_to_span(v4.x, v4.y, v4.z);
        }

        // Why the hell???
        auto proj_mat = glm::orthoRH_ZO<double>(
            aabb.x_min(),
            aabb.x_max(),
            -aabb.y_max(),
            -aabb.y_min(),
            -2 * aabb.z_max() + aabb.z_min(),
            -aabb.z_min()
        );
        proj_mat[1][1] *= -1;

        return proj_mat * view_mat;
    }

    void DLight::set_light_dir(glm::dvec3 dir) {
        dir = glm::normalize(dir);
        const auto axis = glm::cross(glm::dvec3{ 0, 0, -1 }, dir);
        const auto cos_angle = glm::dot(glm::dvec3{ 0, 0, -1 }, dir);
        const auto angle = sung::acos_safe(cos_angle);
        transform_.reset_rotation();
        transform_.rotate(Transform::Angle::from_rad(angle), axis);
    }

}  // namespace mirinae::cpnt


// SLight
namespace mirinae::cpnt {

    glm::vec3 SLight::calc_view_space_pos(const glm::dmat4 view_mat) const {
        const auto v = view_mat * glm::dvec4(transform_.pos_, 1);
        return glm::vec3(v);
    }

    glm::vec3 SLight::calc_to_light_dir(const glm::dmat4 view_mat) const {
        const auto v = view_mat * transform_.make_model_mat() *
                       glm::dvec4(0, 0, 1, 0);
        return glm::normalize(glm::vec3(v));
    };

    glm::dmat4 SLight::make_proj_mat() const {
        return mirinae::make_perspective<double>(
            outer_angle_ * 2, 1.0, 0.1, max_distance_
        );
    }

    glm::dmat4 SLight::make_view_mat() const {
        return transform_.make_view_mat();
    }

    glm::dmat4 SLight::make_light_mat() const {
        return make_proj_mat() * make_view_mat();
    }

}  // namespace mirinae::cpnt


// Scene
namespace mirinae {

    Scene::Scene(ScriptEngine& script) : script_(script) {
        script_.register_global_ptr(SCENE_PTR_NAME, this);
        script_.register_module("scene", scene::luaopen_scene);
    }

}  // namespace mirinae
