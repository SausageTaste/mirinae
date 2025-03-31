#include "mirinae/scene/phys_world.hpp"

#include <cstdarg>
#include <thread>

#define JPH_DEBUG_RENDERER

#include <Jolt/Jolt.h>
#include <daltools/img/img2d.hpp>
#include <entt/entity/registry.hpp>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Renderer/DebugRenderer.h>

#include "mirinae/cpnt/terrain.hpp"
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

    glm::dvec4 conv_vec(const JPH::Vec4 &v) {
        return glm::dvec4(v.GetX(), v.GetY(), v.GetZ(), v.GetW());
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


    class DebugRenderer : public JPH::DebugRenderer {

    public:
        void DrawLine(
            JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor
        ) override {
            // SPDLOG_INFO("Draw line from {} to {}", inFrom, inTo);
        }

        void DrawTriangle(
            JPH::RVec3Arg inV1,
            JPH::RVec3Arg inV2,
            JPH::RVec3Arg inV3,
            JPH::ColorArg inColor,
            ECastShadow inCastShadow
        ) override {
            if (!debug_ren_)
                return;

            debug_ren_->tri(
                ::conv_vec(inV1),
                ::conv_vec(inV2),
                ::conv_vec(inV3),
                ::conv_vec(inColor.ToVec4())
            );
        }

        void DrawGeometry(
            JPH::RMat44Arg inModelMatrix,
            const JPH::AABox &inWorldSpaceBounds,
            float inLODScaleSq,
            JPH::ColorArg inModelColor,
            const GeometryRef &inGeometry,
            ECullMode inCullMode = ECullMode::CullBackFace,
            ECastShadow inCastShadow = ECastShadow::On,
            EDrawMode inDrawMode = EDrawMode::Solid
        ) override {
            SPDLOG_INFO("Draw geometry with model matrix");
        }

        Batch CreateTriangleBatch(
            const Triangle *inTriangles, int inTriangleCount
        ) override {
            return nullptr;
        }

        Batch CreateTriangleBatch(
            const Vertex *inVertices,
            int inVertexCount,
            const JPH::uint32 *inIndices,
            int inIndexCount
        ) override {
            return nullptr;
        }

        void DrawText3D(
            JPH::RVec3Arg inPosition,
            const JPH::string_view &inString,
            JPH::ColorArg inColor = JPH::Color::sWhite,
            float inHeight = 0.5f
        ) override {}

        mirinae::IDebugRen *debug_ren_ = nullptr;
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
                JPH::RVec3(0, -18, 0),
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


    class HeightFieldBody {

    public:
        bool try_populate_height_data(
            entt::entity entity,
            entt::registry &reg,
            JPH::BodyInterface &body_interf
        ) {
            namespace cpnt = mirinae::cpnt;

            auto terr = reg.try_get<cpnt::Terrain>(entity);
            if (!terr) {
                SPDLOG_WARN("Entity does not have a terrain component");
                return false;
            }

            if (!terr->ren_unit_) {
                SPDLOG_WARN("Entity does not have a terrain render unit");
                return false;
            }

            const auto height_map = terr->ren_unit_->height_map();
            if (!height_map) {
                SPDLOG_WARN("Entity does not have a height map");
                return false;
            }

            const auto img2d = height_map->as<dal::TDataImage2D<uint8_t>>();
            if (!img2d) {
                SPDLOG_WARN("Entity does not have a 2D image");
                return false;
            }

            if (4 != img2d->channels()) {
                SPDLOG_WARN("Height map does not have 4 channels");
                return false;
            }
            if (img2d->width() != img2d->height()) {
                SPDLOG_WARN("Height map is not square");
                return false;
            }

            height_data_len_ = img2d->width();

            const auto texel_count = img2d->width() * img2d->height();
            height_data_.resize(texel_count);
            for (uint32_t i = 0; i < texel_count; ++i) {
                const auto *texel = img2d->texel_ptr(
                    i % img2d->width(), i / img2d->width()
                );
                height_data_[i] = static_cast<float>(texel[0]) / 255.0f;
            }

            shape_settings_ = std::make_unique<JPH::HeightFieldShapeSettings>(
                height_data_.data(),
                JPH::Vec3(0, 0, 0),
                JPH::Vec3(
                    60 * 24 / height_data_len_, 64, 60 * 24 / height_data_len_
                ),
                height_data_len_
            );

            body_settings_ = JPH::BodyCreationSettings(
                shape_settings_.get(),
                JPH::RVec3(-260, -48.5, -590),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Static,
                Layers::NON_MOVING
            );

            id_ = body_interf.CreateAndAddBody(
                body_settings_, JPH::EActivation::DontActivate
            );

            ready_ = true;
            return false;
        }

        JPH::BodyID id_;
        std::unique_ptr<JPH::HeightFieldShapeSettings> shape_settings_;
        JPH::BodyCreationSettings body_settings_;
        std::vector<float> height_data_;
        uint32_t height_data_len_ = 0;
        bool ready_ = false;
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

            JPH::DebugRenderer::sInstance = &debug_ren_;
        }

        void optimize() { physics_system.OptimizeBroadPhase(); }

        void give_debug_ren(IDebugRen &debug_ren) {
            debug_ren_.debug_ren_ = &debug_ren;
        }

        void remove_debug_ren() {}

        void do_frame(double dt) {
            constexpr float OPTIMAL_DT = 1.0 / 60.0;

            auto &body_interf = this->body_interf();
            physics_system.Update(OPTIMAL_DT, 1, &temp_alloc_, &job_sys_);
        }

        void pre_sync(double dt, entt::registry &reg) {
            auto &bodies = this->body_interf();
            const auto dt_rcp = static_cast<float>(1.0 / dt);
            const auto push_force_factor = 100 * dt_rcp;

            for (auto e : reg.view<::cpnt::HeightFieldBody>()) {
                auto &body = reg.get<::cpnt::HeightFieldBody>(e);

                if (!body.ready_)
                    body.try_populate_height_data(e, reg, bodies);
            }

            for (auto &e : reg.view<::cpnt::PlayerPhysBody>()) {
                auto &body = reg.get<::cpnt::PlayerPhysBody>(e);
                auto tform = reg.try_get<cpnt::Transform>(e);
                if (!tform)
                    continue;

                const auto pos_diff = ::conv_vec(tform->pos_) -
                                      body.character_->GetPosition();
                body.character_->SetLinearVelocity(pos_diff * dt_rcp);

                JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
                body.character_->ExtendedUpdate(
                    static_cast<float>(dt),
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

                for (auto &contact : body.character_->GetActiveContacts()) {
                    const auto c_motion = bodies.GetMotionType(contact.mBodyB);
                    if (c_motion != JPH::EMotionType::Dynamic)
                        continue;

                    const auto push_dir = -contact.mContactNormal;
                    const auto push_force = push_dir * push_force_factor;
                    bodies.AddForce(contact.mBodyB, push_force);
                }
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

            /*
            physics_system.DrawBodies(
                JPH::BodyManager::DrawSettings(), &debug_ren_
            );
            */

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

        void give_body_height_field(entt::entity entity, entt::registry &reg) {
            namespace cpnt = mirinae::cpnt;

            auto terr = reg.try_get<cpnt::Terrain>(entity);
            if (!terr) {
                SPDLOG_WARN("Entity does not have a terrain component");
                return;
            }

            auto &body = reg.emplace<::cpnt::HeightFieldBody>(entity);
            body.try_populate_height_data(entity, reg, this->body_interf());
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

        ::DebugRenderer debug_ren_;
        ::MyBodyActivationListener body_active_listener_;
        ::MyContactListener contact_listener_;

        ::BoxBody floor_;
    };


    PhysWorld::PhysWorld() : pimpl_(std::make_unique<Impl>()) {}

    PhysWorld::~PhysWorld() = default;

    void PhysWorld::optimize() { pimpl_->optimize(); }

    void PhysWorld::give_debug_ren(IDebugRen &debug_ren) {
        pimpl_->give_debug_ren(debug_ren);
    }

    void PhysWorld::remove_debug_ren() {}

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

    void PhysWorld::give_body_height_field(
        entt::entity entity, entt::registry &reg
    ) {
        pimpl_->give_body_height_field(entity, reg);
    }

    void PhysWorld::give_body_player(
        double height, double radius, entt::entity entity, entt::registry &reg
    ) {
        pimpl_->give_body_player(height, radius, entity, reg);
    }

}  // namespace mirinae
