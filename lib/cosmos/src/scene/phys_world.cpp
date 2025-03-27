#include "mirinae/scene/phys_world.hpp"

#include <cstdarg>
#include <thread>

#include <Jolt/Jolt.h>

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

            int a = 1;
            int *aa = &a;
            int &aaa = *aa;
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
    };


    PhysWorld::PhysWorld() : pimpl_(std::make_unique<Impl>()) {}

    PhysWorld::~PhysWorld() = default;

}  // namespace mirinae
