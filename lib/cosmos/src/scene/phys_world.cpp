#include "mirinae/scene/phys_world.hpp"

#include <cstdarg>
#include <thread>

// #define MIRINAE_JOLT_DEBUG_RENDERER

#include <Jolt/Jolt.h>
#include <daltools/common/task_sys.hpp>
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
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#ifdef MIRINAE_JOLT_DEBUG_RENDERER
    #include <Jolt/Renderer/DebugRenderer.h>
#endif

#include "mirinae/cpnt/phys_body.hpp"
#include "mirinae/cpnt/ren_model.hpp"
#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lightweight/task.hpp"


namespace {

    constexpr JPH::uint cMaxBodies = 1024;
    constexpr JPH::uint cNumBodyMutexes = 0;
    constexpr JPH::uint cMaxBodyPairs = 1024;
    constexpr JPH::uint cMaxContactConstraints = 1024;


    template <typename T>
    JPH::Vec3 conv_vec(const glm::tvec3<T> &v) {
        return JPH::Vec3(
            static_cast<float>(v.x),
            static_cast<float>(v.y),
            static_cast<float>(v.z)
        );
    }

    template <typename T>
    JPH::Quat conv_quat(const glm::tquat<T> &q) {
        return JPH::Quat(
            static_cast<float>(q.x),
            static_cast<float>(q.y),
            static_cast<float>(q.z),
            static_cast<float>(q.w)
        );
    }

    glm::dvec3 conv_vec(const JPH::Vec3 &v) {
        return glm::dvec3(v.GetX(), v.GetY(), v.GetZ());
    }

    glm::dvec3 conv_vec(const JPH::Float3 &v) {
        return glm::dvec3(v.x, v.y, v.z);
    }

    glm::dvec4 conv_vec(const JPH::Vec4 &v) {
        return glm::dvec4(v.GetX(), v.GetY(), v.GetZ(), v.GetW());
    }

    glm::dquat conv_quat(const JPH::Quat &q) {
        return glm::dquat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
    }

    glm::mat4 conv_mat(const JPH::Mat44 &m) {
        return glm::mat4(
            ::conv_vec(m.GetColumn4(0)),
            ::conv_vec(m.GetColumn4(1)),
            ::conv_vec(m.GetColumn4(2)),
            ::conv_vec(m.GetColumn4(3))
        );
    }

}  // namespace


namespace {

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


    class JoltEnkiTaskSystem : public JPH::JobSystemWithBarrier {

    private:
        class MyJob
            : public JPH::JobSystem::Job
            , public enki::ITaskSet {

        public:
            MyJob(
                const char *inJobName,
                const JPH::ColorArg inColor,
                JobSystem *inJobSystem,
                const JobFunction &inJobFunction,
                const JPH::uint32 inNumDependencies
            )
                : Job(inJobName,
                      inColor,
                      inJobSystem,
                      inJobFunction,
                      inNumDependencies) {}

            void ExecuteRange(enki::TaskSetPartition r, uint32_t tid) override {
                this->Execute();
                this->Release();
            }
        };

        class JobPool {

        public:
            JobPool() = default;

            ~JobPool() noexcept { this->clear(); }

            JobPool(const JobPool &) = delete;
            JobPool &operator=(const JobPool &) = delete;
            JobPool(JobPool &&) = delete;
            JobPool &operator=(JobPool &&) = delete;

            MyJob *alloc(
                const char *inJobName,
                const JPH::ColorArg inColor,
                JobSystem *inJobSystem,
                const JobFunction &inJobFunction,
                const JPH::uint32 inNumDependencies
            ) {
                for (size_t i = 0; i < N; ++i) {
                    bool expected = false;
                    if (used_[i].compare_exchange_strong(
                            expected, true, std::memory_order_acquire
                        )) {
                        auto out = new (data_ + i) MyJob(
                            inJobName,
                            inColor,
                            inJobSystem,
                            inJobFunction,
                            inNumDependencies
                        );
                        return out;
                    }
                }

                SPDLOG_CRITICAL("No more jobs available in the pool!");
                return nullptr;
            }

            void free(MyJob *inJob) {
                const auto i_begin = reinterpret_cast<uintptr_t>(data_);
                const auto i_end = reinterpret_cast<uintptr_t>(data_ + N);
                const auto i_job = reinterpret_cast<uintptr_t>(inJob);

                if (i_job < i_begin || i_job >= i_end) {
                    SPDLOG_WARN("Trying to free an invalid job pointer");
                    return;
                }

                const auto index = (i_job - i_begin) / sizeof(MyJob);
                MIRINAE_ASSERT(index < N);

                if (!used_[index].load(std::memory_order_acquire)) {
                    SPDLOG_WARN("Double free or corrupted job pool: {}", index);
                    return;
                }

                if (!inJob->IsDone())
                    dal::tasker().WaitforTask(inJob);

                inJob->~MyJob();
                used_[index].store(false, std::memory_order_release);
            }

            void clear() {
                for (size_t i = 0; i < N; ++i) {
                    if (used_[i]) {
                        used_[i] = false;
                        reinterpret_cast<MyJob *>(data_ + i)->~MyJob();
                    }
                }
            }

            size_t active_count() {
                return std::count(used_.begin(), used_.end(), true);
            }

        private:
            constexpr static size_t N = 1024;
            std::aligned_storage_t<sizeof(MyJob), alignof(MyJob)> data_[N];
            std::array<std::atomic_bool, N> used_;
        };

    public:
        int GetMaxConcurrency() const override {
            return dal::tasker().GetNumTaskThreads() - 1;
        }

        JobHandle CreateJob(
            const char *inName,
            const JPH::ColorArg inColor,
            const JobFunction &inJobFunction,
            const JPH::uint32 inNumDependencies = 0
        ) override {
            return JobHandle(jobs_.alloc(
                inName, inColor, this, inJobFunction, inNumDependencies
            ));
        }

        void FreeJob(Job *inJob) override {
            auto task = static_cast<MyJob *>(inJob);
            jobs_.free(task);
        }

        void QueueJob(Job *inJob) override {
            auto task = static_cast<MyJob *>(inJob);
            task->AddRef();
            dal::tasker().AddTaskSetToPipe(task);
        }

        void QueueJobs(Job **inJobs, JPH::uint inNumJobs) override {
            for (JPH::uint i = 0; i < inNumJobs; ++i) {
                auto task = static_cast<MyJob *>(inJobs[i]);
                task->AddRef();
                dal::tasker().AddTaskSetToPipe(task);
            }
        }

    private:
        JobPool jobs_;
    };


#ifdef MIRINAE_JOLT_DEBUG_RENDERER

    class DebugRen : public JPH::DebugRenderer {

    private:
        class MyBatch : public JPH::RefTargetVirtual {

        public:
            void AddRef() override { ++ref_count_; }

            void Release() override { --ref_count_; }

            void append(const mirinae::DebugMesh::Vertex &v) {
                mesh_.idx_.push_back(mesh_.vtx_.size());
                mesh_.vtx_.push_back(v);
            }

            mirinae::DebugMesh mesh_;
            std::atomic_size_t ref_count_;
        };

    public:
        DebugRen() {
            this->Initialize();
            return;
        }

        void DrawLine(
            const JPH::RVec3Arg inFrom,
            const JPH::RVec3Arg inTo,
            const JPH::ColorArg inColor
        ) override {
            // SPDLOG_INFO("Draw line from {} to {}", inFrom, inTo);
        }

        void DrawTriangle(
            const JPH::RVec3Arg inV1,
            const JPH::RVec3Arg inV2,
            const JPH::RVec3Arg inV3,
            const JPH::ColorArg inColor,
            const ECastShadow inCastShadow
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
            const JPH::RMat44Arg inModelMatrix,
            const JPH::AABox &inWorldSpaceBounds,
            const float inLODScaleSq,
            const JPH::ColorArg inModelColor,
            const GeometryRef &inGeometry,
            const ECullMode inCullMode,
            const ECastShadow inCastShadow,
            const EDrawMode inDrawMode
        ) override {
            if (!debug_ren_)
                return;

            auto ptr = inGeometry->mLODs[0].mTriangleBatch.GetPtr();
            auto batch = static_cast<const MyBatch *>(ptr);
            if (!batch)
                return;

            debug_ren_->mesh(batch->mesh_, ::conv_mat(inModelMatrix));
        }

        Batch CreateTriangleBatch(
            const Triangle *inTriangles, const int inTriangleCount
        ) override {
            auto batch = std::make_unique<MyBatch>();

            for (int i = 0; i < inTriangleCount; ++i) {
                auto &src_tri = inTriangles[i];

                for (int j = 0; j < 3; ++j) {
                    mirinae::DebugMesh::Vertex dst_vtx;
                    auto &src_vtx = src_tri.mV[j];
                    dst_vtx.pos_ = ::conv_vec(src_vtx.mPosition);
                    dst_vtx.color_ = ::conv_vec(src_vtx.mColor.ToVec4());
                    batch->append(dst_vtx);
                }
            }

            return batch.release();
        }

        Batch CreateTriangleBatch(
            const Vertex *inVertices,
            const int inVertexCount,
            const JPH::uint32 *inIndices,
            const int inIndexCount
        ) override {
            auto batch = std::make_unique<MyBatch>();

            batch->mesh_.vtx_.reserve(inVertexCount);
            for (int i = 0; i < inVertexCount; ++i) {
                auto &v = batch->mesh_.vtx_.emplace_back();
                v.pos_ = ::conv_vec(inVertices[i].mPosition);
                v.color_ = ::conv_vec(inVertices[i].mColor.ToVec4());
            }

            batch->mesh_.idx_.assign(inIndices, inIndices + inIndexCount);

            return batch.release();
        }

        void DrawText3D(
            const JPH::RVec3Arg inPosition,
            const JPH::string_view &inString,
            const JPH::ColorArg inColor,
            const float inHeight
        ) override {}

        mirinae::IDebugRen *debug_ren_ = nullptr;
    };

#endif  // JPH_DEBUG_RENDERER

}  // namespace


namespace {

    class MeshBuilder {

    public:
        void add_tri(
            const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2
        ) {
            indices_.push_back(
                JPH::IndexedTriangle(
                    this->add_vtx(v0), this->add_vtx(v1), this->add_vtx(v2), 0
                )
            );
        }

        auto &vtx() const { return vertices_; }
        auto &idx() const { return indices_; }

    private:
        JPH::uint32 add_vtx(const JPH::Float3 &v) {
            const auto idx = vertices_.size();
            vertices_.push_back(v);
            return idx;
        }

        JPH::uint32 add_vtx(const glm::vec3 &v) {
            return this->add_vtx(JPH::Float3(v.x, v.y, v.z));
        }

        JPH::uint32 add_vtx(const JPH::RVec3 &v) {
            return this->add_vtx(JPH::Float3(v.GetX(), v.GetY(), v.GetZ()));
        }

        JPH::VertexList vertices_;
        JPH::IndexedTriangleList indices_;
    };


    class ModelAccessor : public mirinae::IModelAccessor {

    public:
        bool position(const glm::vec3 &p) override {
            vtx_buf_[vtx_count_] = p * scale_;
            ++vtx_count_;

            if (vtx_count_ >= vtx_buf_.size()) {
                mesh_builder_.add_tri(vtx_buf_[0], vtx_buf_[1], vtx_buf_[2]);
                vtx_count_ = 0;
            }

            return true;
        }

        void set_scale(const glm::vec3 &scale) { scale_ = scale; }

        auto &vtx() const { return mesh_builder_.vtx(); }
        auto &idx() const { return mesh_builder_.idx(); }

    private:
        MeshBuilder mesh_builder_;
        std::array<glm::vec3, 3> vtx_buf_;
        glm::vec3 scale_{ 1, 1, 1 };
        size_t vtx_count_ = 0;
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

            shape_ = new JPH::SphereShape(0.5f);

            JPH::BodyCreationSettings sphere_settings(
                shape_,
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
        JPH::Ref<JPH::Shape> shape_;
    };


    class CharacterPhysBody : public mirinae::ICharacterPhysBody {

    public:
        void init(
            const mirinae::cpnt::CharacterPhys &phys,
            const mirinae::cpnt::Transform *tform,
            JPH::PhysicsSystem &phys_sys
        ) {
            half_height_ = static_cast<float>(phys.height_ * 0.5);
            radius_ = static_cast<float>(phys.radius_);

            JPH::CharacterVirtualSettings settings;
            settings.mShape = new JPH::CapsuleShape(half_height_, radius_);
            settings.mMaxSlopeAngle = JPH::DegreesToRadians(45);
            settings.mSupportingVolume = JPH::Plane(
                JPH::Vec3::sAxisY(), -half_height_ * 0.75f
            );
            settings.mMass = 70;
            settings.mInnerBodyShape = settings.mShape;

            JPH::RVec3 pos(0, 0, 0);
            auto rot = JPH::Quat::sIdentity();
            if (tform) {
                pos = ::conv_vec(tform->pos_);
                rot = ::conv_quat(tform->rot_);
            }

            character_ = new JPH::CharacterVirtual(
                &settings, pos + this->offset_vec(), rot, &phys_sys
            );
        }

        void extended_update(
            float dt,
            JPH::TempAllocatorImpl &temp_alloc,
            JPH::PhysicsSystem &physics_system
        ) {
            if (!character_)
                return;

            character_->ExtendedUpdate(
                static_cast<float>(dt),
                physics_system.GetGravity(),
                update_settings_,
                physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                physics_system.GetDefaultLayerFilter(Layers::MOVING),
                {},
                {},
                temp_alloc
            );
        }

        void set_linear_vel(const JPH::RVec3 &vel) {
            if (!character_)
                return;

            character_->SetLinearVelocity(vel);
        }

        JPH::CharacterVirtual &chara() { return *character_; }

        JPH::RVec3 world_pos() const {
            return character_->GetPosition() - this->offset_vec();
        }

    private:
        float offset() const { return half_height_ + radius_; }

        JPH::RVec3 offset_vec() const {
            return JPH::RVec3(0, this->offset(), 0);
        }

        JPH::CharacterVirtual::ExtendedUpdateSettings update_settings_;
        JPH::CharacterVirtual *character_;
        float half_height_ = 0;
        float radius_ = 0;
    };

}  // namespace


namespace { namespace cpnt {

    class PhysBody {

    public:
        JPH::BodyID id_;
    };


    class MeshBody {

    public:
        bool try_init(
            const mirinae::cpnt::MdlActorStatic *modl,
            const mirinae::cpnt::Transform *tform,
            JPH::BodyInterface &body_interf
        ) {
            namespace cpnt = mirinae::cpnt;

            if (!modl) {
                // SPDLOG_WARN("Entity does not have a model component");
                return false;
            }

            if (!modl->model_) {
                // SPDLOG_WARN("Entity does not have a model ren unit");
                return false;
            }
            if (!modl->model_->is_ready()) {
                // SPDLOG_WARN("Model is not ready");
                return false;
            }

            JPH::Vec3 pos(0, 0, 0);
            JPH::Quat rot = JPH::Quat::sIdentity();
            if (tform) {
                pos = ::conv_vec(tform->pos_);
                rot = ::conv_quat(tform->rot_);
                model_acc_.set_scale(tform->scale_);
            }

            modl->model_->access_positions(model_acc_);

            JPH::MeshShapeSettings mesh_shape(
                model_acc_.vtx(), model_acc_.idx()
            );
            mesh_shape.mActiveEdgeCosThresholdAngle = 0.999f;
            auto res = mesh_shape.Create();
            if (res.HasError()) {
                SPDLOG_ERROR("Failed to create mesh shape: {}", res.GetError());
                return false;
            }
            shape_ = res.Get();

            JPH::BodyCreationSettings body_settings(
                shape_, pos, rot, JPH::EMotionType::Static, ::Layers::NON_MOVING
            );

            id_ = body_interf.CreateAndAddBody(
                body_settings, JPH::EActivation::DontActivate
            );

            ready_ = true;
            return true;
        }

        JPH::BodyID id_;
        JPH::Ref<JPH::Shape> shape_;
        ::ModelAccessor model_acc_;
        bool ready_ = false;
    };


    class HeightFieldBody {

    public:
        bool try_init(
            const mirinae::cpnt::Terrain *terr,
            const mirinae::cpnt::Transform *tform,
            JPH::BodyInterface &body_interf
        ) {
            namespace cpnt = mirinae::cpnt;

            if (!terr) {
                // SPDLOG_WARN("Entity does not have a terrain component");
                return false;
            }

            if (!terr->ren_unit_) {
                // SPDLOG_WARN("Entity does not have a terrain render unit");
                return false;
            }

            const auto height_map = terr->ren_unit_->height_map();
            if (!height_map) {
                // SPDLOG_WARN("Entity does not have a height map");
                return false;
            }

            const auto img2d = height_map->as<dal::TDataImage2D<uint8_t>>();
            if (!img2d) {
                // SPDLOG_WARN("Entity does not have a 2D image");
                return false;
            }

            if (4 != img2d->channels()) {
                // SPDLOG_WARN("Height map does not have 4 channels");
                return false;
            }
            if (img2d->width() != img2d->height()) {
                // SPDLOG_WARN("Height map is not square");
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

            JPH::HeightFieldShapeSettings shape_settings(
                height_data_.data(),
                JPH::Vec3(0, 0, 0),
                JPH::Vec3(
                    60 * 24 / double(height_data_len_ - 1),
                    64,
                    60 * 24 / double(height_data_len_ - 1)
                ),
                height_data_len_
            );

            auto result = shape_settings.Create();
            if (result.HasError()) {
                SPDLOG_ERROR(
                    "Failed to create height field shape: {}", result.GetError()
                );
                return false;
            }
            shape_ = result.Get();

            JPH::BodyCreationSettings body_settings(
                shape_,
                JPH::RVec3(),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Static,
                Layers::NON_MOVING
            );

            if (tform) {
                body_settings.mPosition = ::conv_vec(tform->pos_);
                body_settings.mRotation = ::conv_quat(tform->rot_);
            }

            id_ = body_interf.CreateAndAddBody(
                body_settings, JPH::EActivation::DontActivate
            );

            ready_ = true;
            return true;
        }

        JPH::BodyID id_;
        JPH::Ref<JPH::Shape> shape_;
        std::vector<float> height_data_;
        uint32_t height_data_len_ = 0;
        bool ready_ = false;
    };

}}  // namespace ::cpnt


// Tasks
namespace {

    class TaskPreSync_Mesh : public mirinae::DependingTask {

    public:
        void prepare(entt::registry &reg, JPH::BodyInterface &body_interf) {
            reg_ = &reg;
            body_interf_ = &body_interf;

            auto view = reg.view<::cpnt::MeshBody>();
            m_SetSize = view.size();
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            namespace cpnt = mirinae::cpnt;

            someone_is_preparing_ = false;
            someone_finished_preparing_ = false;

            if (!reg_)
                return;
            if (!body_interf_)
                return;

            auto view = reg_->view<::cpnt::MeshBody>();
            auto begin = view.begin() + range.start;
            auto end = view.begin() + range.end;

            for (auto it = begin; it != end; ++it) {
                entt::entity e = *it;
                auto &body = view.get<::cpnt::MeshBody>(e);
                if (body.ready_)
                    continue;

                auto tform = reg_->try_get<cpnt::Transform>(e);
                auto modl = reg_->try_get<cpnt::MdlActorStatic>(e);
                if (body.try_init(modl, tform, *body_interf_))
                    someone_finished_preparing_ = true;
                else
                    someone_is_preparing_ = true;
            }
        }

    private:
        entt::registry *reg_ = nullptr;
        JPH::BodyInterface *body_interf_ = nullptr;
        bool someone_is_preparing_ = false;
        bool someone_finished_preparing_ = false;
    };


    class TaskPreSync_Height : public mirinae::DependingTask {

    public:
        void prepare(entt::registry &reg, JPH::BodyInterface &body_interf) {
            reg_ = &reg;
            body_interf_ = &body_interf;

            auto view = reg.view<::cpnt::HeightFieldBody>();
            m_SetSize = view.size();
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            namespace cpnt = mirinae::cpnt;

            someone_is_preparing_ = false;
            someone_finished_preparing_ = false;

            if (!reg_)
                return;
            if (!body_interf_)
                return;

            auto view = reg_->view<::cpnt::HeightFieldBody>();
            auto begin = view.begin() + range.start;
            auto end = view.begin() + range.end;

            for (auto it = begin; it != end; ++it) {
                const auto e = *it;
                auto &body = view.get<::cpnt::HeightFieldBody>(e);
                if (body.ready_)
                    continue;

                auto tform = reg_->try_get<cpnt::Transform>(e);
                auto terr = reg_->try_get<cpnt::Terrain>(e);
                if (body.try_init(terr, tform, *body_interf_))
                    someone_finished_preparing_ = true;
                else
                    someone_is_preparing_ = true;
            }
        }

    private:
        entt::registry *reg_ = nullptr;
        JPH::BodyInterface *body_interf_ = nullptr;
        bool someone_is_preparing_ = false;
        bool someone_finished_preparing_ = false;
    };


    class TaskPreSync_Player : public mirinae::DependingTask {

    public:
        void prepare(
            entt::registry &reg,
            JPH::PhysicsSystem &phys_sys,
            JPH::BodyInterface &body_interf,
            JPH::TempAllocatorImpl &temp_alloc
        ) {
            reg_ = &reg;
            phys_sys_ = &phys_sys;
            body_interf_ = &body_interf;
            temp_alloc_ = &temp_alloc;

            auto view = reg.view<mirinae::cpnt::CharacterPhys>();
            m_SetSize = view.size();
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            namespace cpnt = mirinae::cpnt;

            constexpr float dt_rcp = 60;
            constexpr float dt = 1.f / dt_rcp;
            constexpr float push_force_factor = 100 * dt_rcp;

            auto &reg = *reg_;
            auto &phys_sys = *phys_sys_;
            auto &bodies = *body_interf_;

            for (auto &e : reg.view<cpnt::CharacterPhys>()) {
                auto &phys = reg.get<cpnt::CharacterPhys>(e);
                auto tform = reg.try_get<cpnt::Transform>(e);
                auto body = phys.ren_unit<::CharacterPhysBody>();

                if (!body) {
                    auto p_body = std::make_unique<::CharacterPhysBody>();
                    p_body->init(phys, tform, phys_sys);
                    body = p_body.get();
                    phys.ren_unit_ = std::move(p_body);
                }

                if (!tform)
                    continue;

                const auto cur_pos = ::conv_vec(tform->pos_);
                const auto pos_diff = cur_pos - body->world_pos();
                auto vel = pos_diff * dt_rcp;

                switch (body->chara().GetGroundState()) {
                    case JPH::CharacterBase::EGroundState::InAir:
                        // vel += phys_sys.GetGravity();
                        break;
                }

                body->set_linear_vel(vel);
                body->extended_update(dt, *temp_alloc_, phys_sys);

                for (auto &contact : body->chara().GetActiveContacts()) {
                    const auto c_motion = bodies.GetMotionType(contact.mBodyB);
                    if (c_motion != JPH::EMotionType::Dynamic)
                        continue;

                    const auto push_dir = -contact.mContactNormal;
                    const auto push_force = push_dir * push_force_factor;
                    bodies.AddForce(contact.mBodyB, push_force);
                }
            }
        }

    private:
        entt::registry *reg_ = nullptr;
        JPH::PhysicsSystem *phys_sys_ = nullptr;
        JPH::BodyInterface *body_interf_ = nullptr;
        JPH::TempAllocatorImpl *temp_alloc_ = nullptr;
    };


    class TaskUpdate : public mirinae::DependingTask {

    public:
        void prepare(
            JPH::PhysicsSystem &phys_sys,
            JPH::JobSystem &job_sys,
            JPH::TempAllocatorImpl &temp_alloc
        ) {
            phys_sys_ = &phys_sys;
            job_sys_ = &job_sys;
            temp_alloc_ = &temp_alloc;
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            constexpr float dt_rcp = 60;
            constexpr float dt = 1.f / dt_rcp;
            constexpr float push_force_factor = 100 * dt_rcp;

            const auto res = phys_sys_->Update(dt, 1, temp_alloc_, job_sys_);
        }

    private:
        JPH::PhysicsSystem *phys_sys_ = nullptr;
        JPH::JobSystem *job_sys_ = nullptr;
        JPH::TempAllocatorImpl *temp_alloc_ = nullptr;
    };


    class TaskPostSync_PhysBody : public mirinae::DependingTask {

    public:
        void prepare(entt::registry &reg, JPH::BodyInterface &body_interf) {
            reg_ = &reg;
            body_interf_ = &body_interf;
            m_SetSize = reg.view<::cpnt::PhysBody>().size();
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            auto view = reg_->view<::cpnt::PhysBody>();
            auto begin = view.begin() + range.start;
            auto end = view.begin() + range.end;

            for (auto it = begin; it != end; ++it) {
                const auto e = *it;
                auto &body = reg_->get<::cpnt::PhysBody>(e);
                auto tform = reg_->try_get<mirinae::cpnt::Transform>(e);
                if (!tform)
                    continue;

                JPH::RVec3 pos;
                JPH::Quat rot;
                body_interf_->GetPositionAndRotation(body.id_, pos, rot);
                tform->pos_ = ::conv_vec(pos);
                tform->rot_ = ::conv_quat(rot);
            }
        }

    private:
        entt::registry *reg_ = nullptr;
        JPH::BodyInterface *body_interf_ = nullptr;
    };


    class TaskPostSync_Player : public mirinae::DependingTask {

    public:
        void prepare(entt::registry &reg) {
            reg_ = &reg;
            m_SetSize = reg_->view<mirinae::cpnt::CharacterPhys>().size();
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            auto view = reg_->view<mirinae::cpnt::CharacterPhys>();
            auto begin = view.begin() + range.start;
            auto end = view.begin() + range.end;

            for (auto it = begin; it != end; ++it) {
                const auto e = *it;
                auto &phys = reg_->get<mirinae::cpnt::CharacterPhys>(e);
                auto body = phys.ren_unit<::CharacterPhysBody>();
                if (!body)
                    continue;
                auto tform = reg_->try_get<mirinae::cpnt::Transform>(e);
                if (!tform)
                    continue;

                tform->pos_ = ::conv_vec(body->world_pos());
            }
        }

    private:
        entt::registry *reg_ = nullptr;
    };


    class TaskPhysWorld : public mirinae::StageTask {

    public:
        TaskPhysWorld(
            entt::registry &reg,
            JPH::PhysicsSystem &phys_sys,
            JPH::BodyInterface &body_interf,
            JPH::JobSystem &job_sys,
            JPH::TempAllocatorImpl &temp_alloc
        )
            : StageTask("PhysWorld")
            , reg_(&reg)
            , phys_sys_(&phys_sys)
            , body_interf_(&body_interf)
            , job_sys_(&job_sys)
            , temp_alloc_(&temp_alloc) {
            // Pre
            pre_mesh_.succeed(this);
            pre_height_.succeed(this);
            pre_player_.succeed(&pre_mesh_, &pre_height_);
            // Update
            update_.succeed(&pre_player_);
            // Post
            post_phys_body_.succeed(&update_);
            post_player_.succeed(&update_);
            // Fence
            fence_.succeed(&post_phys_body_, &post_player_);
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            pre_mesh_.prepare(*reg_, *body_interf_);
            pre_height_.prepare(*reg_, *body_interf_);
            pre_player_.prepare(*reg_, *phys_sys_, *body_interf_, *temp_alloc_);
            update_.prepare(*phys_sys_, *job_sys_, *temp_alloc_);
            post_phys_body_.prepare(*reg_, *body_interf_);
            post_player_.prepare(*reg_);
        }

        enki::ITaskSet *get_fence() override { return &fence_; }

    private:
        entt::registry *reg_ = nullptr;
        JPH::PhysicsSystem *phys_sys_ = nullptr;
        JPH::BodyInterface *body_interf_ = nullptr;
        JPH::JobSystem *job_sys_ = nullptr;
        JPH::TempAllocatorImpl *temp_alloc_ = nullptr;

        TaskPreSync_Mesh pre_mesh_;
        TaskPreSync_Height pre_height_;
        TaskPreSync_Player pre_player_;
        TaskUpdate update_;
        TaskPostSync_PhysBody post_phys_body_;
        TaskPostSync_Player post_player_;

        mirinae::FenceTask fence_;
    };

}  // namespace


namespace mirinae {

    class PhysWorld::Impl {

    public:
        Impl() : temp_alloc_(10 * 1024 * 1024) {
            job_sys_.Init(JPH::cMaxPhysicsBarriers);
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
            // floor_.init(body_interf);
            // body_interf.AddBody(floor_.id(), JPH::EActivation::DontActivate);

#ifdef MIRINAE_JOLT_DEBUG_RENDERER
            JPH::DebugRenderer::sInstance = &debug_ren_;
#endif
        }

        void register_tasks(TaskGraph &tasks, entt::registry &reg) {
            auto &stage = tasks.stages_.emplace_back();
            stage.task_ = std::make_unique<TaskPhysWorld>(
                reg, physics_system, this->body_interf(), job_sys_, temp_alloc_
            );
        }

        void optimize() { physics_system.OptimizeBroadPhase(); }

        void give_debug_ren(IDebugRen &debug_ren) {
#ifdef MIRINAE_JOLT_DEBUG_RENDERER
            debug_ren_.debug_ren_ = &debug_ren;
#endif
        }

        void remove_debug_ren() {
#ifdef MIRINAE_JOLT_DEBUG_RENDERER
            debug_ren_.debug_ren_ = nullptr;
#endif
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
                ::conv_vec(tform->pos_),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Dynamic,
                Layers::MOVING
            );
            sphere_settings.mMassPropertiesOverride.mMass = 10;

            body->id_ = this->body_interf().CreateAndAddBody(
                sphere_settings, JPH::EActivation::Activate
            );
        }

        void give_body_triangles(entt::entity entity, entt::registry &reg) {
            namespace cpnt = mirinae::cpnt;
            auto &body = reg.emplace<::cpnt::MeshBody>(entity);
        }

        void give_body_height_field(entt::entity entity, entt::registry &reg) {
            namespace cpnt = mirinae::cpnt;
            auto &body = reg.emplace<::cpnt::HeightFieldBody>(entity);
        }

    private:
        // Locking body interface
        JPH::BodyInterface &body_interf() {
            return physics_system.GetBodyInterface();
        }

        JoltInit jolt_init_;
        JPH::TempAllocatorImpl temp_alloc_;
        ::JoltEnkiTaskSystem job_sys_;
        ::BPLayerInterfaceImpl broad_phase_layer_interf_;
        ::ObjectVsBroadPhaseLayerFilterImpl obj_vs_broadphase_layer_filter_;
        ::ObjectLayerPairFilterImpl obj_vs_obj_layer_filter_;
        JPH::PhysicsSystem physics_system;

        ::MyBodyActivationListener body_active_listener_;
        ::MyContactListener contact_listener_;

#ifdef MIRINAE_JOLT_DEBUG_RENDERER
        JPH::BodyManager::DrawSettings debug_ren_settings_;
        ::DebugRen debug_ren_;
#endif

        ::BoxBody floor_;
        bool someone_is_preparing_ = false;
    };


    PhysWorld::PhysWorld() : pimpl_(std::make_unique<Impl>()) {}

    PhysWorld::~PhysWorld() = default;

    void PhysWorld::register_tasks(TaskGraph &tasks, entt::registry &reg) {
        pimpl_->register_tasks(tasks, reg);
    }

    void PhysWorld::optimize() { pimpl_->optimize(); }

    void PhysWorld::give_debug_ren(IDebugRen &debug_ren) {
        pimpl_->give_debug_ren(debug_ren);
    }

    void PhysWorld::remove_debug_ren() {}

    void PhysWorld::pre_sync(double dt, entt::registry &reg) {}

    void PhysWorld::do_frame(double dt) {}

    void PhysWorld::post_sync(double dt, entt::registry &reg) {}

    void PhysWorld::give_body(entt::entity entity, entt::registry &reg) {
        pimpl_->give_body(entity, reg);
    }

    void PhysWorld::give_body_triangles(
        entt::entity entity, entt::registry &reg
    ) {
        pimpl_->give_body_triangles(entity, reg);
    }

    void PhysWorld::give_body_height_field(
        entt::entity entity, entt::registry &reg
    ) {
        pimpl_->give_body_height_field(entity, reg);
    }

}  // namespace mirinae
