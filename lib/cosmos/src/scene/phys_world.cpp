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
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
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
            JPH::RegisterDefaultAllocator();
            JPH::Trace = trace_impl;
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
    } jolt_init_;


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
            SPDLOG_INFO("Contact validate callback");
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        void OnContactAdded(
            const JPH::Body &inBody1,
            const JPH::Body &inBody2,
            const JPH::ContactManifold &inManifold,
            JPH::ContactSettings &ioSettings
        ) override {
            SPDLOG_INFO("A contact was added");
        }

        void OnContactPersisted(
            const JPH::Body &inBody1,
            const JPH::Body &inBody2,
            const JPH::ContactManifold &inManifold,
            JPH::ContactSettings &ioSettings
        ) override {
            SPDLOG_INFO("A contact was persisted");
        }

        void OnContactRemoved(
            const JPH::SubShapeIDPair &inSubShapePair
        ) override {
            SPDLOG_INFO("A contact was removed");
        }
    };

}  // namespace


namespace {

    class BoxBody {

    public:
        void init(JPH::BodyInterface &body_interf) {
            using namespace JPH::literals;

            JPH::BoxShapeSettings floor_shape_settings(
                JPH::Vec3(100.0f, 10.0f, 100.0f)
            );
            floor_shape_settings.SetEmbedded();

            auto floor_shape_result = floor_shape_settings.Create();
            floor_shape = floor_shape_result.Get();

            JPH::BodyCreationSettings floor_settings(
                floor_shape,
                JPH::RVec3(0.0_r, -1.0_r, 0.0_r),
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

            auto &body_interf = this->body_interf();
            floor_.init(body_interf);
            body_interf.AddBody(floor_.id(), JPH::EActivation::DontActivate);
        }

        void do_frame(double dt) {
            constexpr float OPTIMAL_DT = 1.0 / 60.0;

            auto &body_interf = this->body_interf();
            physics_system.Update(OPTIMAL_DT, 1, &temp_alloc_, &job_sys_);
        }

        void sync_tforms(entt::registry &reg) {
            for (auto &e : reg.view<::cpnt::PhysBody>()) {
                auto &body = reg.get<::cpnt::PhysBody>(e);
                auto tform = reg.try_get<cpnt::Transform>(e);
                if (!tform)
                    continue;

                const auto pos = this->body_interf().GetCenterOfMassPosition(
                    body.id_
                );
                tform->pos_ = { pos.GetX(), pos.GetY(), pos.GetZ() };

                continue;
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

            body->id_ = this->body_interf().CreateAndAddBody(
                sphere_settings, JPH::EActivation::Activate
            );
            this->body_interf().SetLinearVelocity(
                body->id_, JPH::Vec3(1, -5, 0)
            );
        }

    private:
        // Locking body interface
        JPH::BodyInterface &body_interf() {
            return physics_system.GetBodyInterface();
        }

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

    void PhysWorld::do_frame(double dt) { pimpl_->do_frame(dt); }

    void PhysWorld::sync_tforms(entt::registry &reg) {
        pimpl_->sync_tforms(reg);
    }

    void PhysWorld::give_body(entt::entity entity, entt::registry &reg) {
        pimpl_->give_body(entity, reg);
    }

}  // namespace mirinae
