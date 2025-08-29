#include "mirinae/vulkan_pch.h"

#include "mirinae/render/draw_set.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/ren_model.hpp"
#include "mirinae/cpnt/transform.hpp"


// DrawSheet
namespace mirinae {

    void DrawSheet::build(entt::registry& reg) {
        namespace cpnt = mirinae::cpnt;

        for (const auto e : reg.view<cpnt::MdlActorStatic>()) {
            auto& mactor = reg.get<cpnt::MdlActorStatic>(e);
            if (!mactor.model_)
                continue;
            auto renmdl = mactor.get_model<mirinae::RenderModel>();
            if (!renmdl)
                continue;
            auto actor = mactor.get_actor<mirinae::RenderActor>();
            if (!actor)
                continue;

            glm::dmat4 model_mat(1);
            if (auto tfrom = reg.try_get<cpnt::Transform>(e))
                model_mat = tfrom->make_model_mat();

            const auto unit_count = renmdl->render_units_.size();
            for (size_t i = 0; i < unit_count; ++i) {
                if (!mactor.visibility_.get(i))
                    continue;

                auto& unit = renmdl->render_units_[i];
                auto& dst = this->get_static(unit);
                auto& dst_actor = dst.actors_.emplace_back();
                dst_actor.actor_ = actor;
                dst_actor.model_mat_ = model_mat;
            }

            const auto unit_trs_count = renmdl->render_units_alpha_.size();
            for (size_t i = 0; i < unit_trs_count; ++i) {
                if (!mactor.visibility_.get(i + unit_count))
                    continue;

                auto& unit = renmdl->render_units_alpha_[i];
                auto& dst = this->get_static_trs(unit);
                auto& dst_actor = dst.actors_.emplace_back();
                dst_actor.actor_ = actor;
                dst_actor.model_mat_ = model_mat;
            }
        }

        for (const auto e : reg.view<cpnt::MdlActorSkinned>()) {
            auto& mactor = reg.get<cpnt::MdlActorSkinned>(e);
            if (!mactor.model_)
                continue;
            auto renmdl = mactor.get_model<mirinae::RenderModelSkinned>();
            if (!renmdl)
                continue;
            auto actor = mactor.get_actor<mirinae::RenderActorSkinned>();
            if (!actor)
                continue;

            glm::dmat4 model_mat(1);
            if (auto tfrom = reg.try_get<cpnt::Transform>(e))
                model_mat = tfrom->make_model_mat();

            const auto unit_count = renmdl->runits_.size();
            for (size_t i = 0; i < unit_count; ++i) {
                if (!mactor.visibility_.get(i))
                    continue;

                auto& unit = renmdl->runits_[i];
                auto& dst = this->get_skinned(unit);
                auto& dst_actor = dst.actors_.emplace_back();
                dst_actor.actor_ = actor;
                dst_actor.model_mat_ = model_mat;
            }

            const auto unit_trs_count = renmdl->runits_alpha_.size();
            for (size_t i = 0; i < unit_trs_count; ++i) {
                if (!mactor.visibility_.get(i + unit_count))
                    continue;

                auto& unit = renmdl->runits_alpha_[i];
                auto& dst = this->get_skinned_trs(unit);
                auto& dst_actor = dst.actors_.emplace_back();
                dst_actor.actor_ = actor;
                dst_actor.model_mat_ = model_mat;
            }
        }

        this->ocean_ = nullptr;
        for (auto e : reg.view<cpnt::Ocean>()) {
            // Only one ocean is allowed
            this->ocean_ = &reg.get<cpnt::Ocean>(e);
            break;
        }

        this->atmosphere_ = nullptr;
        for (auto e : reg.view<cpnt::AtmosphereSimple>()) {
            // Only one atmosphere is allowed
            this->atmosphere_ = &reg.get<cpnt::AtmosphereSimple>(e);
            break;
        }
    }

    DrawSheet::StaticRenderPairs& DrawSheet::get_static(
        mirinae::RenderUnit& unit
    ) {
        for (auto& x : static_) {
            if (x.unit_ == &unit)
                return x;
        }

        auto& output = static_.emplace_back();
        output.unit_ = &unit;
        return output;
    }

    DrawSheet::StaticRenderPairs& DrawSheet::get_static_trs(
        mirinae::RenderUnit& unit
    ) {
        for (auto& x : static_trs_) {
            if (x.unit_ == &unit)
                return x;
        }

        auto& output = static_trs_.emplace_back();
        output.unit_ = &unit;
        return output;
    }

    DrawSheet::SkinnedRenderPairs& DrawSheet::get_skinned(
        mirinae::RenderUnitSkinned& unit
    ) {
        for (auto& x : skinned_) {
            if (x.unit_ == &unit)
                return x;
        }

        auto& output = skinned_.emplace_back();
        output.unit_ = &unit;
        return output;
    }

    DrawSheet::SkinnedRenderPairs& DrawSheet::get_skinned_trs(
        mirinae::RenderUnitSkinned& unit
    ) {
        for (auto& x : skinned_trs_) {
            if (x.unit_ == &unit)
                return x;
        }

        auto& output = skinned_trs_.emplace_back();
        output.unit_ = &unit;
        return output;
    }

}  // namespace mirinae


// DrawSetStatic
namespace mirinae {

    void DrawSetStatic::fetch(const entt::registry& reg) {
        namespace cpnt = mirinae::cpnt;

        for (const auto e : reg.view<cpnt::MdlActorStatic>()) {
            auto& mactor = reg.get<cpnt::MdlActorStatic>(e);
            if (!mactor.model_)
                continue;
            auto renmdl = mactor.get_model<mirinae::RenderModel>();
            if (!renmdl)
                continue;
            auto actor = mactor.get_actor<mirinae::RenderActor>();
            if (!actor)
                continue;

            glm::dmat4 model_mat(1);
            if (auto tfrom = reg.try_get<cpnt::Transform>(e))
                model_mat = tfrom->make_model_mat();

            const auto unit_count = renmdl->render_units_.size();
            for (size_t i = 0; i < unit_count; ++i) {
                if (!mactor.visibility_.get(i))
                    continue;

                auto& dst = opa_.emplace_back();
                dst.unit_ = &renmdl->render_units_[i];
                dst.actor_ = actor;
                dst.model_mat_ = model_mat;
            }

            const auto unit_trs_count = renmdl->render_units_alpha_.size();
            for (size_t i = 0; i < unit_trs_count; ++i) {
                if (!mactor.visibility_.get(i + unit_count))
                    continue;

                auto& dst = trs_.emplace_back();
                dst.unit_ = &renmdl->render_units_alpha_[i];
                dst.actor_ = actor;
                dst.model_mat_ = model_mat;
            }
        }

        for (const auto e : reg.view<cpnt::MdlActorSkinned>()) {
            auto& mactor = reg.get<cpnt::MdlActorSkinned>(e);
            if (!mactor.model_)
                continue;
            auto renmdl = mactor.get_model<mirinae::RenderModelSkinned>();
            if (!renmdl)
                continue;
            auto actor = mactor.get_actor<mirinae::RenderActorSkinned>();
            if (!actor)
                continue;

            glm::dmat4 model_mat(1);
            if (auto tfrom = reg.try_get<cpnt::Transform>(e))
                model_mat = tfrom->make_model_mat();

            const auto unit_count = renmdl->runits_.size();
            for (size_t i = 0; i < unit_count; ++i) {
                if (!mactor.visibility_.get(i))
                    continue;

                auto& dst = skin_opa_.emplace_back();
                dst.unit_ = &renmdl->runits_.at(i);
                dst.actor_ = actor;
                dst.model_mat_ = model_mat;
                dst.runit_idx_ = i;
            }

            const auto unit_trs_count = renmdl->runits_alpha_.size();
            for (size_t i = 0; i < unit_trs_count; ++i) {
                if (!mactor.visibility_.get(i + unit_count))
                    continue;

                auto& dst = skin_trs_.emplace_back();
                dst.unit_ = &renmdl->runits_alpha_.at(i);
                dst.actor_ = actor;
                dst.model_mat_ = model_mat;
                dst.runit_idx_ = i;
            }
        }
    }

    void DrawSetStatic::clear() {
        opa_.clear();
        trs_.clear();
        skin_opa_.clear();
        skin_trs_.clear();
    }

}  // namespace mirinae


// DrawSetSkinned
namespace mirinae {

    void DrawSetSkinned::fetch(const entt::registry& reg) {
        namespace cpnt = mirinae::cpnt;

        for (const auto e : reg.view<cpnt::MdlActorSkinned>()) {
            auto& mactor = reg.get<cpnt::MdlActorSkinned>(e);
            if (!mactor.model_)
                continue;
            auto renmdl = mactor.get_model<mirinae::RenderModelSkinned>();
            if (!renmdl)
                continue;
            auto actor = mactor.get_actor<mirinae::RenderActorSkinned>();
            if (!actor)
                continue;

            glm::dmat4 model_mat(1);
            if (auto tfrom = reg.try_get<cpnt::Transform>(e))
                model_mat = tfrom->make_model_mat();

            const auto unit_count = renmdl->runits_.size();
            for (size_t i = 0; i < unit_count; ++i) {
                if (!mactor.visibility_.get(i))
                    continue;

                auto& dst = opa_.emplace_back();
                dst.unit_ = &renmdl->runits_[i];
                dst.actor_ = actor;
                dst.model_mat_ = model_mat;
            }

            const auto unit_trs_count = renmdl->runits_alpha_.size();
            for (size_t i = 0; i < unit_trs_count; ++i) {
                if (!mactor.visibility_.get(i + unit_count))
                    continue;

                auto& dst = trs_.emplace_back();
                dst.unit_ = &renmdl->runits_alpha_[i];
                dst.actor_ = actor;
                dst.model_mat_ = model_mat;
            }
        }
    }

    void DrawSetSkinned::clear() {
        opa_.clear();
        trs_.clear();
    }

}  // namespace mirinae
