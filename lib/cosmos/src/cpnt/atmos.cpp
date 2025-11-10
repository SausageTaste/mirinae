#include "mirinae/cpnt/atmos.hpp"

#include <imgui.h>
#include <entt/entity/registry.hpp>

#include "mirinae/lua/tools.hpp"


#define GET_ENTT_PTR()                                                       \
    const auto entt_ptr = mirinae::find_global_ptr(L, "__mirinae_entt_reg"); \
    if (!entt_ptr)                                                           \
        return luaL_error(L, "Entity registry pointer not found");           \
    auto& reg = *static_cast<entt::registry*>(entt_ptr);


namespace {

    bool render_color_intensity(mirinae::ColorIntensity& ci) {
        bool output = false;

        ImGui::PushID(reinterpret_cast<const void*>(&ci));
        output |= ImGui::ColorEdit3("Color", &ci.color()[0]);
        output |= ImGui::SliderFloat(
            "Intensity",
            &ci.intensity(),
            0.0,
            10.0,
            "%.6f",
            ImGuiSliderFlags_Logarithmic
        );
        ImGui::PopID();

        return output;
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


// Lua
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


// AtmosParams
namespace mirinae {

    void AtmosParams::set_default_values() {
        absorption_extinction_.set_scaled_color(0.00065, 0.00188, 0.00008);

        mie_density_exp_scale_ = -0.83333;
        absorption_density_0_layer_width_ = 25;
        absorption_density_0_constant_ = -0.66667;
        absorption_density_0_linear_ = 0.06667;
        absorption_density_1_constant_ = 2.66667;
        absorption_density_1_linear_ = -0.06667;

        rayleigh_density_exp_scale_ = -0.125;

        mie_phase_g_ = 0.8;
        rayleigh_scattering_.set_scaled_color(0.0058, 0.01356, 0.0331);
        mie_scattering_.set_scaled_color(0.004);
        mie_absorption_.set_scaled_color(0.00044);
        mie_extinction_.set_scaled_color(0.00444);

        ground_albedo_.set_scaled_color(0);
        radius_bottom_ = 6360;
        radius_top_ = 6460;
    }

    void AtmosParams::render_imgui() {
        {
            float radius = radius_bottom_;
            float thickness = std::abs(radius_top_ - radius_bottom_);
            ImGui::DragFloat("Radius (km)", &radius, 10);
            ImGui::SliderFloat("Thickness (km)", &thickness, 0, 1000);
            radius_bottom_ = radius;
            radius_top_ = radius + std::abs(thickness);

            ImGui::Text("Ground albedo");
            ::render_color_intensity(ground_albedo_);
        }
        ImGui::Separator();
        {
            ImGui::Text("Rayleigh scattering");
            ::render_color_intensity(rayleigh_scattering_);
        }
        ImGui::Separator();
        {
            ImGui::Text("Mie scattering");
            ::render_color_intensity(mie_scattering_);
            ImGui::Text("Mie extinction");
            ::render_color_intensity(mie_extinction_);
            ImGui::Text("Mie absorption");
            ::render_color_intensity(mie_absorption_);
        }
        ImGui::Separator();
        {
            ImGui::DragFloat(
                "Rayleigh density exp scale",
                &rayleigh_density_exp_scale_,
                0.001f,
                -10.f,
                10.f
            );
            ImGui::DragFloat(
                "Mie density exp scale",
                &mie_density_exp_scale_,
                0.001f,
                -10.f,
                10.f
            );
            ImGui::SliderFloat("Mie phase G", &mie_phase_g_, 0.01f, 1.f);
        }
        ImGui::Separator();
        {
            ImGui::DragFloat(
                "Absorption 0 layer with",
                &absorption_density_0_layer_width_,
                0.01f
            );
            ImGui::DragFloat(
                "Absorption 0 const", &absorption_density_0_constant_, 0.01f
            );
            ImGui::DragFloat(
                "Absorption 0 linear", &absorption_density_0_linear_, 0.01f
            );
            ImGui::DragFloat(
                "Absorption 1 const", &absorption_density_1_constant_, 0.01f
            );
            ImGui::DragFloat(
                "Absorption 1 linear", &absorption_density_1_linear_, 0.01f
            );

            ImGui::Text("Absorption extinction");
            ::render_color_intensity(absorption_extinction_);
        }
        ImGui::Separator();

        if (ImGui::Button("Reset")) {
            this->set_default_values();
        }
    }

}  // namespace mirinae


// AtmosphereEpic
namespace mirinae::cpnt {

    AtmosphereEpic::AtmosphereEpic() { params_.set_default_values(); }

    void AtmosphereEpic::render_imgui() { params_.render_imgui(); }

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

    float AtmosphereEpic::radius_bottom() const {
        return params_.radius_bottom_;
    }

    float AtmosphereEpic::radius_top() const { return params_.radius_top_; }

}  // namespace mirinae::cpnt
