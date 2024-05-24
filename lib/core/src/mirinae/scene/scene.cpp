#include "mirinae/scene/scene.hpp"

#include "mirinae/scene/transform.hpp"


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

}  // namespace


namespace { namespace scene {

    /**
     * Test with following lua code:
     * `scene = require('scene')`
     * `e = scene.create_static_model("asset/models/sponza/sponza.dmd")`
     *
     * @param str model_path
     * @return Entity ID as lightuserdata
     */
    int create_static_model(lua_State* const L) {
        const auto scene = ::find_scene_ptr(L);
        if (!scene)
            return luaL_error(L, "Scene pointer not found");

        const auto model_path = luaL_checkstring(L, 1);
        const auto enttid = scene->reg_.create();

        {
            scene->entt_without_model_.push_back(enttid);

            auto& mactor = scene->reg_.emplace<mirinae::cpnt::StaticModelActor>(
                enttid
            );
            mactor.model_path_.assign(model_path);

            auto& trans = scene->reg_.emplace<mirinae::cpnt::Transform>(enttid);
            trans.scale_ = glm::vec3{ 0.01f, 0.01f, 0.01f };
        }

        lua_pushlightuserdata(L, ::entt_cast(enttid));
        return 1;
    }


    /**
     * Test with following lua code:
     * `scene = require('scene')`
     * `e = scene.create_skinned_model("ThinMatrix/Character Running.dmd")`
     *
     * @param str model_path
     * @return Entity ID as lightuserdata
     */
    int create_skinned_model(lua_State* const L) {
        const auto scene = ::find_scene_ptr(L);
        if (!scene)
            return luaL_error(L, "Scene pointer not found");

        const auto model_path = luaL_checkstring(L, 1);
        const auto enttid = scene->reg_.create();

        {
            scene->entt_without_model_.push_back(enttid);

            auto& mactor =
                scene->reg_.emplace<mirinae::cpnt::SkinnedModelActor>(enttid);
            mactor.model_path_.assign(model_path);

            auto& trans = scene->reg_.emplace<mirinae::cpnt::Transform>(enttid);
        }

        lua_pushlightuserdata(L, ::entt_cast(enttid));
        return 1;
    }


    int luaopen_scene(lua_State* const L) {
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
