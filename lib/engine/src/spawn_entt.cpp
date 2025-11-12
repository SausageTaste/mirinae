#include "mirinae/spawn_entt.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/atmos.hpp"
#include "mirinae/cpnt/camera.hpp"
#include "mirinae/cpnt/envmap.hpp"
#include "mirinae/cpnt/identifier.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/ocean.hpp"
#include "mirinae/cpnt/phys_body.hpp"
#include "mirinae/cpnt/ren_model.hpp"
#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lua/script.hpp"


namespace mirinae {

    void register_modules(ScriptEngine& script) {
        script.register_module(
            "atmos_epic", mirinae::cpnt::AtmosphereEpic::lua_module
        );
    }

    void spawn_entities(CosmosSimulator& cosmos) {
        auto& reg = cosmos.reg();

        // Physice object
        for (int x = 0; x < 2; ++x) {
            for (int y = 0; y < 5; ++y) {
                for (int z = 0; z < 2; ++z) {
                    const auto entt = reg.create();

                    auto& id = reg.emplace<mirinae::cpnt::Id>(entt);
                    id.set_name("physics object");

                    auto& mdl = reg.emplace<mirinae::cpnt::MdlActorStatic>(
                        entt
                    );
                    mdl.model_path_ = "Sung/sphere.dun/sphere.dmd";

                    auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
                    const auto radius = 20;
                    const auto jitter = (x + y + z) * radius * 0.2;
                    tform.pos_ = glm::dvec3{
                        2.2 * radius * x - 50 - jitter,
                        2.2 * radius * y + 2200,
                        2.2 * radius * z + 50 + jitter,
                    };
                    tform.set_scale(radius);

                    cosmos.phys_world().give_body(entt, reg);
                }
            }
        }
        cosmos.phys_world().optimize();

        // Saturn
        {
            const auto entt = reg.create();

            auto& id = reg.emplace<mirinae::cpnt::Id>(entt);
            id.set_name("saturn");

            auto& mdl = reg.emplace<mirinae::cpnt::MdlActorStatic>(entt);
            mdl.model_path_ = "Sung/saturn.dun/saturn.dmd";

            auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
            tform.pos_ = glm::dvec3{ 0, 100000000, 1000000000 };
            tform.set_rotation(0.952791, -0.277713, -0.016126, -0.121671);
            tform.set_scale(70000000);
        }

        // DLight
        {
            constexpr auto d45 = mirinae::cpnt::Transform::Angle::from_deg(45);

            const auto entt = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("Sun Light");

            auto& d = reg.emplace<mirinae::cpnt::DLight>(entt);
            d.color_.set_scaled_color(5);
            d.max_shadow_distance_ = 10000;

            auto& t = reg.emplace<mirinae::cpnt::Transform>(entt);
            t.reset_rotation();
            t.rotate(-d45, glm::vec3{ 1, 0, 0 });
            t.rotate(d45, glm::vec3{ 0, 1, 0 });
        }

        // DLight
        /*
        {
            const auto entt = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("Sun Light 2");

            auto& d = reg.emplace<mirinae::cpnt::DLight>(entt);
            d.color_.set_scaled_color(0, 0, 5);

            auto& t = reg.emplace<mirinae::cpnt::Transform>(entt);
            d.set_light_dir(0.5613, -0.7396, -0.3713, t);
        }

        // SLight
        {
            flashlight_ = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(flashlight_);
            i.set_name("Flashlight");

            auto& s = reg.emplace<mirinae::cpnt::SLight>(flashlight_);
            s.color_.set_scaled_color(0, 0, 0);
            s.inner_angle_.set_deg(10);
            s.outer_angle_.set_deg(25);

            auto& t = reg.emplace<mirinae::cpnt::Transform>(flashlight_);
            t.pos_ = { 0, 2, 0 };
        }
        */

        // SLight
        {
            const auto e = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(e);
            i.set_name("Spotlight 1");

            auto& s = reg.emplace<mirinae::cpnt::SLight>(e);
            s.color_.set_scaled_color(10);
            s.inner_angle_.set_deg(10);
            s.outer_angle_.set_deg(25);

            auto& t = reg.emplace<mirinae::cpnt::Transform>(e);
            t.set_pos(469, 1875, -80);
            t.set_rotation(0.997859, -0.065403, 0, 0);
        }

        // VPLight
        {
            const auto e = reg.create();

            auto& id = reg.emplace<mirinae::cpnt::Id>(e);
            id.set_name("VPLight 1");

            auto& l = reg.emplace<mirinae::cpnt::VPLight>(e);
            l.color_.set_scaled_color(glm::vec3(7, 24, 7) * 10.f);

            auto& t = reg.emplace<mirinae::cpnt::Transform>(e);
            t.pos_ = glm::dvec3(-100, 1500, 0);
        }

        // Main Camera
        {
            const auto entt = reg.create();
            cosmos.scene().main_camera_ = entt;
            cosmos.cam_ctrl().set_camera(entt);

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("Main Camera");

            auto& cam = reg.emplace<mirinae::cpnt::StandardCamera>(entt);
            cam.proj_.near_ = 0.1;
            cam.proj_.far_ = 100000000000;
            cam.exposure_ = 1;

            auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
            tform.pos_ = {
                -114.50,
                6.89,
                -45.62,
            };
            tform.rot_ = glm::normalize(
                glm::dquat{
                    -0.376569,
                    0.056528,
                    0.914417,
                    0.137265,
                }
            );
        }

        // Envmap
        {
            const auto e = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(e);
            i.set_name("Main envmap");

            auto& envmap = reg.emplace<mirinae::cpnt::Envmap>(e);

            auto& tform = reg.emplace<mirinae::cpnt::Transform>(e);
            tform.pos_ = { -99.15, 4.98, -25.26 };
        }

        // Ocean
        {
            const auto entt = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("Ocean");

            auto& ocean = reg.emplace<mirinae::cpnt::Ocean>(entt);
            ocean.height_ = 1500;

            constexpr double len_scale0 = 250;
            constexpr double len_scale1 = 17;
            constexpr double len_scale2 = 5;
            constexpr auto boundary1 = SUNG_TAU / len_scale1 * 6;
            constexpr auto boundary2 = SUNG_TAU / len_scale2 * 6;

            auto cas = &ocean.cascades_[0];
            cas->lod_scale_ = len_scale0;
            cas->cutoff_low_ = 0.0001;
            cas->cutoff_high_ = boundary1;
            cas->L_ = cas->lod_scale_;

            cas = &ocean.cascades_[1];
            cas->lod_scale_ = len_scale1;
            cas->cutoff_low_ = boundary1;
            cas->cutoff_high_ = boundary2;
            cas->L_ = cas->lod_scale_;

            cas = &ocean.cascades_[2];
            cas->lod_scale_ = len_scale2;
            cas->cutoff_low_ = boundary2;
            cas->cutoff_high_ = 9999;
            cas->L_ = cas->lod_scale_;
        }

        // Atmosphere
        {
            const auto entt = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("Atmosphere Simple");

            auto& atm = reg.emplace<mirinae::cpnt::AtmosphereSimple>(entt);
            atm.fog_color_ = { 0.556, 0.707, 0.846 };
            atm.sky_tex_path_ = ":asset/textures/empty_sky.hdr";
            atm.mie_anisotropy_ = 0.8;
        }

        {
            const auto entt = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("Atmosphere Epic");

            auto& atm = reg.emplace<mirinae::cpnt::AtmosphereEpic>(entt);
        }

        // https://manticorp.github.io/unrealheightmap/#latitude/36.271/longitude/-112.357/zoom/14/outputzoom/14/width/4096/height/4096
        // https://manticorp.github.io/unrealheightmap/#latitude/46.453/longitude/10.635/zoom/12/outputzoom/12/width/8129/height/8129

        // Terrain
        {
            const auto entt = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("Terrain");

            auto& terrain = reg.emplace<mirinae::cpnt::Terrain>(entt);
            terrain.height_map_path_ = "Sung/36_271_-112_357_14_4096_4096.png";
            terrain.albedo_map_path_ =
                "Sung/36_271_-112_357_14_4096_4096_albedo_imagery.png";
            terrain.terrain_width_ = 31500;
            terrain.terrain_height_ = 31500;
            terrain.height_scale_ = 2761.32 - 585.625;
            terrain.tile_count_x_ = 20;
            terrain.tile_count_y_ = 20;
            terrain.tess_factor_ = 0.5;

            auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
            tform.pos_ = { 0, 585.625, 0 };

            cosmos.phys_world().give_body_height_field(entt, reg);
        }

        // Terrain
        {
            const auto entt = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("Terrain");

            auto& terrain = reg.emplace<mirinae::cpnt::Terrain>(entt);
            terrain.height_map_path_ = "Sung/35_987_-112_356_14_4096_4096.png";
            terrain.albedo_map_path_ =
                "Sung/35_987_-112_356_14_4096_4096_albedo_imagery.png";
            terrain.terrain_width_ = 31500;
            terrain.terrain_height_ = 31500;
            terrain.height_scale_ = 2090.598 - 702.766;
            terrain.tile_count_x_ = 20;
            terrain.tile_count_y_ = 20;
            terrain.tess_factor_ = 0.5;

            auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
            tform.pos_ = { 0, 702.766, 31500 };

            cosmos.phys_world().give_body_height_field(entt, reg);
        }

        // Terrain
        {
            const auto entt = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("Terrain");

            auto& terrain = reg.emplace<mirinae::cpnt::Terrain>(entt);
            terrain.height_map_path_ = "Sung/35_987_-112_005_14_4096_4096.png";
            terrain.albedo_map_path_ =
                "Sung/35_987_-112_005_14_4096_4096_albedo_imagery.png";
            terrain.terrain_width_ = 31500;
            terrain.terrain_height_ = 31500;
            terrain.height_scale_ = 2433.875 - 706.953;
            terrain.tile_count_x_ = 20;
            terrain.tile_count_y_ = 20;
            terrain.tess_factor_ = 0.5;

            auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
            tform.pos_ = { 31500, 706.953, 31500 };

            cosmos.phys_world().give_body_height_field(entt, reg);
        }

        // Terrain
        {
            const auto entt = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("Terrain");

            auto& terrain = reg.emplace<mirinae::cpnt::Terrain>(entt);
            terrain.height_map_path_ = "Sung/36_272_-112_005_14_4096_4096.png";
            terrain.albedo_map_path_ =
                "Sung/36_272_-112_005_14_4096_4096_albedo_imagery.png";
            terrain.terrain_width_ = 31500;
            terrain.terrain_height_ = 31500;
            terrain.height_scale_ = 2806.492 - 829.977;
            terrain.tile_count_x_ = 20;
            terrain.tile_count_y_ = 20;
            terrain.tess_factor_ = 0.5;

            auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
            tform.pos_ = { 31500, 829.977, 0 };

            cosmos.phys_world().give_body_height_field(entt, reg);
        }

        // City
        {
            const auto entt = reg.create();

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("City");

            auto& mdl = reg.emplace<mirinae::cpnt::MdlActorStatic>(entt);
            mdl.model_path_ = "Sung/bus_game_map.dun/bus_game_map.dmd";

            auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
            tform.pos_ = { 510, 1550, 2000 };
            tform.set_scale(0.1);

            cosmos.phys_world().give_body_triangles(entt, reg);
        }

        // Player model
        {
            const auto entt = reg.create();
            cosmos.cam_ctrl().set_target(entt);

            auto& i = reg.emplace<mirinae::cpnt::Id>(entt);
            i.set_name("Player model");

            auto& mdl = reg.emplace<mirinae::cpnt::MdlActorSkinned>(entt);

            auto& tform = reg.emplace<mirinae::cpnt::Transform>(entt);
            tform.pos_ = { 438, 1875, -66 };

#if false
                cosmos.cam_ctrl().anim_idle_ = "idle_normal_1";
                cosmos.cam_ctrl().anim_walk_ = "evt1_walk_normal_1";
                cosmos.cam_ctrl().anim_run_ = "run_normal_1";
                cosmos.cam_ctrl().anim_sprint_ = "hwan_run_battle_1";
                cosmos.cam_ctrl().player_model_heading_.set_zero();
                mdl.model_path_ = "Sung/artist.dun/artist_subset.dmd";
                mdl.anim_state_.select_anim_name(
                    "idle_normal_1", cosmos.scene().clock()
                );
#else
            cosmos.cam_ctrl().anim_idle_ = "standing";
            cosmos.cam_ctrl().anim_walk_ = "run";
            cosmos.cam_ctrl().anim_run_ = "run";
            cosmos.cam_ctrl().anim_sprint_ = "run";
            cosmos.cam_ctrl().player_model_heading_.set_deg(90);
            mdl.model_path_ =
                "Sung/Character Running.dun/Character Running.dmd";
            mdl.anim_state_.select_anim_index(0, cosmos.scene().clock());
            tform.set_scale(0.12);
#endif

            auto& phys = reg.emplace<mirinae::cpnt::CharacterPhys>(entt);
            phys.height_ = 1;
            phys.radius_ = 0.15;
        }
    }

}  // namespace mirinae
