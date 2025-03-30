#include "mirinae/scene/phys_world.hpp"

#include <cstdarg>
#include <thread>

#include <Jolt/Jolt.h>
#include <entt/entity/registry.hpp>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"


namespace {

    constexpr JPH::uint cMaxBodies = 1024;
    constexpr JPH::uint cNumBodyMutexes = 0;
    constexpr JPH::uint cMaxBodyPairs = 1024;
    constexpr JPH::uint cMaxContactConstraints = 1024;


    JPH::Vec3 conv_vec(const glm::vec3 &v) { return JPH::Vec3(v.x, v.y, v.z); }

    JPH::Vec3 conv_vec(const glm::dvec3 &v) { return JPH::Vec3(v.x, v.y, v.z); }

    glm::dvec3 conv_vec(const JPH::Vec3 &v) {
        return glm::dvec3(v.GetX(), v.GetY(), v.GetZ());
    }


    static void trace_impl(const char *inFMT, ...) {
        // Format the message
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);

        SPDLOG_INFO("{}", buffer);
    }

    bool AssertFailedImpl(
        const char *inExpression,
        const char *inMessage,
        const char *inFile,
        JPH::uint inLine
    ) {
        spdlog::default_logger_raw()->log(
            spdlog::source_loc(inFile, inLine, inExpression),
            spdlog::level::critical,
            inMessage
        );

        // Breakpoint
        return true;
    };


    class JoltInit {

    public:
        JoltInit() {
            static bool init = false;

            if (!init) {
                init = true;
                JPH::RegisterDefaultAllocator();
                JPH::Trace = trace_impl;
                JPH::Factory::sInstance = new JPH::Factory();
                JPH::RegisterTypes();
            }
        }
    };


    namespace Layers {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    };  // namespace Layers


    namespace BroadPhaseLayers {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::uint NUM_LAYERS(2);
    };  // namespace BroadPhaseLayers


    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {

    public:
        BPLayerInterfaceImpl() {
            // Create a mapping table from object to broad phase layer
            mObjectToBroadPhase[Layers::NON_MOVING] =
                BroadPhaseLayers::NON_MOVING;
            mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        }

        JPH::uint GetNumBroadPhaseLayers() const override {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(
            JPH::ObjectLayer inLayer
        ) const override {
            JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
            return mObjectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char *GetBroadPhaseLayerName(
            BroadPhaseLayer inLayer
        ) const override {
            switch ((BroadPhaseLayer::Type)inLayer) {
                case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
                    return "NON_MOVING";
                case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
                    return "MOVING";
                default:
                    JPH_ASSERT(false);
                    return "INVALID";
            }
        }
#endif  // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

    private:
        JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
    };


    class ObjectVsBroadPhaseLayerFilterImpl
        : public JPH::ObjectVsBroadPhaseLayerFilter {

    public:
        bool ShouldCollide(
            JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2
        ) const override {
            switch (inLayer1) {
                case Layers::NON_MOVING:
                    return inLayer2 == BroadPhaseLayers::MOVING;
                case Layers::MOVING:
                    return true;
                default:
                    JPH_ASSERT(false);
                    return false;
            }
        }
    };


    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {

    public:
        bool ShouldCollide(
            JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2
        ) const override {
            switch (inObject1) {
                case Layers::NON_MOVING:
                    // Non moving only collides with moving
                    return inObject2 == Layers::MOVING;
                case Layers::MOVING:
                    return true;  // Moving collides with everything
                default:
                    JPH_ASSERT(false);
                    return false;
            }
        }
    };


    class MyBodyActivationListener : public JPH::BodyActivationListener {

    public:
        void OnBodyActivated(
            const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData
        ) override {
            SPDLOG_INFO(
                "A body got activated: {}", inBodyID.GetIndexAndSequenceNumber()
            );
        }

        void OnBodyDeactivated(
            const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData
        ) override {
            SPDLOG_INFO(
                "A body got deactivated: {}",
                inBodyID.GetIndexAndSequenceNumber()
            );
        }
    };


    class MyContactListener : public JPH::ContactListener {

    public:
        JPH::ValidateResult OnContactValidate(
            const JPH::Body &inBody1,
            const JPH::Body &inBody2,
            JPH::RVec3Arg inBaseOffset,
            const JPH::CollideShapeResult &inCollisionResult
        ) override {
            // SPDLOG_INFO("Contact validate callback");
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        void OnContactAdded(
            const JPH::Body &inBody1,
            const JPH::Body &inBody2,
            const JPH::ContactManifold &inManifold,
            JPH::ContactSettings &ioSettings
        ) override {
            // SPDLOG_INFO("A contact was added");
        }

        void OnContactPersisted(
            const JPH::Body &inBody1,
            const JPH::Body &inBody2,
            const JPH::ContactManifold &inManifold,
            JPH::ContactSettings &ioSettings
        ) override {
            // SPDLOG_INFO("A contact was persisted");
        }

        void OnContactRemoved(
            const JPH::SubShapeIDPair &inSubShapePair
        ) override {
            // SPDLOG_INFO("A contact was removed");
        }
    };

}  // namespace


namespace {

    class BoxBody {

    public:
        void init(JPH::BodyInterface &body_interf) {
            using namespace JPH::literals;

            JPH::BoxShapeSettings floor_shape_settings(
                JPH::Vec3(1000, 20, 1000)
            );
            floor_shape_settings.SetEmbedded();

            auto floor_shape_result = floor_shape_settings.Create();
            floor_shape = floor_shape_result.Get();

            JPH::BodyCreationSettings floor_settings(
                floor_shape,
                JPH::RVec3(0, -18.5, 0),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Static,
                ::Layers::NON_MOVING
            );

            body_ = body_interf.CreateBody(floor_settings);
        }

        JPH::BodyID id() const { return body_->GetID(); }

    private:
        JPH::ShapeRefC floor_shape;
        JPH::Body *body_;
    };


    class SphereBody {

    public:
        void init(JPH::BodyInterface &body_interf) {
            using namespace JPH::literals;

            JPH::BodyCreationSettings sphere_settings(
                new JPH::SphereShape(0.5f),
                JPH::RVec3(0.0_r, 10.0_r, 0.0_r),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Dynamic,
                Layers::MOVING
            );

            body_ = body_interf.CreateAndAddBody(
                sphere_settings, JPH::EActivation::Activate
            );
        }

        JPH::BodyID id() const { return body_; }

    private:
        JPH::BodyID body_;
    };

}  // namespace


namespace { namespace cpnt {

    class PhysBody {

    public:
        JPH::BodyID id_;
    };


    class PlayerPhysBody {

    public:
        float offset() const { return half_height_ + radius_; }

        glm::dvec3 offset_vec() const {
            return glm::dvec3(0, this->offset(), 0);
        }

        JPH::CharacterVirtual *character_;
        float half_height_ = 0;
        float radius_ = 0;
    };

}}  // namespace ::cpnt


namespace mirinae {

    class PhysWorld::Impl {

    public:
        Impl()
            : temp_alloc_(10 * 1024 * 1024)
            , job_sys_(
                  JPH::cMaxPhysicsJobs,
                  JPH::cMaxPhysicsBarriers,
                  std::thread::hardware_concurrency() - 1
              ) {
            physics_system.Init(
                cMaxBodies,
                cNumBodyMutexes,
                cMaxBodyPairs,
                cMaxContactConstraints,
                broad_phase_layer_interf_,
                obj_vs_broadphase_layer_filter_,
                obj_vs_obj_layer_filter_
            );

            physics_system.SetBodyActivationListener(&body_active_listener_);
            physics_system.SetContactListener(&contact_listener_);
            physics_system.SetGravity(JPH::Vec3(0, -9.81f, 0));

            auto &body_interf = this->body_interf();
            floor_.init(body_interf);
            body_interf.AddBody(floor_.id(), JPH::EActivation::DontActivate);
        }

        void optimize() { physics_system.OptimizeBroadPhase(); }

        void do_frame(double dt) {
            constexpr float OPTIMAL_DT = 1.0 / 60.0;

            auto &body_interf = this->body_interf();
            physics_system.Update(OPTIMAL_DT, 1, &temp_alloc_, &job_sys_);
        }

        void pre_sync(double dt, entt::registry &reg) {
            for (auto &e : reg.view<::cpnt::PlayerPhysBody>()) {
                auto &body = reg.get<::cpnt::PlayerPhysBody>(e);
                auto tform = reg.try_get<cpnt::Transform>(e);
                if (!tform)
                    continue;

                const auto pos_diff = ::conv_vec(tform->pos_) -
                                      body.character_->GetPosition();

                body.character_->SetLinearVelocity(pos_diff / dt);

                JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
                body.character_->ExtendedUpdate(
                    dt,
                    physics_system.GetGravity(),
                    update_settings,
                    physics_system.GetDefaultBroadPhaseLayerFilter(
                        Layers::MOVING
                    ),
                    physics_system.GetDefaultLayerFilter(Layers::MOVING),
                    {},
                    {},
                    temp_alloc_
                );
            }
        }

        void post_sync(double dt, entt::registry &reg) {
            for (auto &e : reg.view<::cpnt::PlayerPhysBody>()) {
                auto &body = reg.get<::cpnt::PlayerPhysBody>(e);
                auto tform = reg.try_get<cpnt::Transform>(e);
                if (!tform)
                    continue;

                const auto pos = body.character_->GetPosition();
                tform->pos_ = ::conv_vec(pos) - body.offset_vec();
            }

            for (auto &e : reg.view<::cpnt::PhysBody>()) {
                auto &body = reg.get<::cpnt::PhysBody>(e);
                auto tform = reg.try_get<cpnt::Transform>(e);
                if (!tform)
                    continue;

                JPH::RVec3 pos;
                JPH::Quat rot;
                this->body_interf().GetPositionAndRotation(body.id_, pos, rot);
                tform->pos_ = { pos.GetX(), pos.GetY(), pos.GetZ() };
                tform->rot_ = {
                    rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()
                };
            }

            return;
        }

        void give_body(entt::entity entity, entt::registry &reg) {
            namespace cpnt = mirinae::cpnt;

            auto tform = reg.try_get<cpnt::Transform>(entity);
            if (!tform) {
                SPDLOG_WARN("Entity does not have a transform component");
                return;
            }

            auto body = reg.try_get<::cpnt::PhysBody>(entity);
            if (body) {
                SPDLOG_WARN("Entity already has a physics body component");
                return;
            }

            body = &reg.emplace<::cpnt::PhysBody>(entity);
            if (!body) {
                SPDLOG_WARN("Failed to add physics body component to entity");
                return;
            }

            JPH::BodyCreationSettings sphere_settings(
                new JPH::SphereShape(tform->scale_.x),
                JPH::RVec3(tform->pos_.x, tform->pos_.y, tform->pos_.z),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Dynamic,
                Layers::MOVING
            );
            sphere_settings.mMassPropertiesOverride.mMass = 10;

            body->id_ = this->body_interf().CreateAndAddBody(
                sphere_settings, JPH::EActivation::Activate
            );
        }

        void give_body_player(
            float height, float radius, entt::entity entity, entt::registry &reg
        ) {
            namespace cpnt = mirinae::cpnt;

            auto tform = reg.try_get<cpnt::Transform>(entity);
            if (!tform) {
                SPDLOG_WARN("Entity does not have a transform component");
                return;
            }

            auto &body = reg.emplace<::cpnt::PlayerPhysBody>(entity);
            body.half_height_ = height * 0.5f;
            body.radius_ = radius;

            JPH::CharacterVirtualSettings settings;
            settings.mShape = new JPH::CapsuleShape(height * 0.5f, radius);
            settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
            // settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(),
            // -0.9f);
            settings.mMass = 70;
            settings.mInnerBodyShape = new JPH::CapsuleShape(
                height * 0.5f, radius
            );

            const auto quat = tform->rot_;
            body.character_ = new JPH::CharacterVirtual(
                &settings,
                ::conv_vec(tform->pos_ + body.offset_vec()),
                JPH::Quat(quat.x, quat.y, quat.z, quat.w),
                &physics_system
            );
        }

    private:
        // Locking body interface
        JPH::BodyInterface &body_interf() {
            return physics_system.GetBodyInterface();
        }

        JoltInit jolt_init_;
        JPH::TempAllocatorImpl temp_alloc_;
        JPH::JobSystemThreadPool job_sys_;
        ::BPLayerInterfaceImpl broad_phase_layer_interf_;
        ::ObjectVsBroadPhaseLayerFilterImpl obj_vs_broadphase_layer_filter_;
        ::ObjectLayerPairFilterImpl obj_vs_obj_layer_filter_;
        JPH::PhysicsSystem physics_system;

        ::MyBodyActivationListener body_active_listener_;
        ::MyContactListener contact_listener_;

        ::BoxBody floor_;
    };


    PhysWorld::PhysWorld() : pimpl_(std::make_unique<Impl>()) {}

    PhysWorld::~PhysWorld() = default;

    void PhysWorld::optimize() { pimpl_->optimize(); }

    void PhysWorld::pre_sync(double dt, entt::registry &reg) {
        pimpl_->pre_sync(dt, reg);
    }

    void PhysWorld::do_frame(double dt) { pimpl_->do_frame(dt); }

    void PhysWorld::post_sync(double dt, entt::registry &reg) {
        pimpl_->post_sync(dt, reg);
    }

    void PhysWorld::give_body(entt::entity entity, entt::registry &reg) {
        pimpl_->give_body(entity, reg);
    }

    void PhysWorld::give_body_player(
        double height, double radius, entt::entity entity, entt::registry &reg
    ) {
        pimpl_->give_body_player(height, radius, entity, reg);
    }

}  // namespace mirinae
