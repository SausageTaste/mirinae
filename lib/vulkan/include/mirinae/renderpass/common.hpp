#pragma once

#include <map>

#include <enkiTS/TaskScheduler.h>
#include <entt/fwd.hpp>

#include "mirinae/cpnt/camera.hpp"
#include "mirinae/lightweight/debug_ren.hpp"
#include "mirinae/render/renderee.hpp"


namespace mirinae {

    class CosmosSimulator;
    class DrawSheet;


    class RenderPass {

    public:
        RenderPass();
        ~RenderPass();

        RenderPass& operator=(VkRenderPass rp);

        operator VkRenderPass() const;
        VkRenderPass operator*() const;
        VkRenderPass get() const;

        void reset(VkRenderPass rp, mirinae::VulkanDevice& device);
        void destroy(mirinae::VulkanDevice& device);

    public:
        VkRenderPass rp_ = VK_NULL_HANDLE;
    };


    class RpPipeline {

    public:
        RpPipeline();
        ~RpPipeline();

        bool create(const VkComputePipelineCreateInfo&, VulkanDevice&);

        RpPipeline& operator=(VkPipeline handle);

        operator VkPipeline() const;
        VkPipeline operator*() const;
        VkPipeline get() const;

        void reset(VkPipeline handle, VulkanDevice& device);
        void destroy(VulkanDevice& device);

    public:
        VkPipeline handle_ = VK_NULL_HANDLE;
    };


    class RpPipeLayout {

    public:
        RpPipeLayout();
        ~RpPipeLayout();

        RpPipeLayout& operator=(VkPipelineLayout handle);

        operator VkPipelineLayout&();
        operator VkPipelineLayout() const;
        VkPipelineLayout operator*() const;
        VkPipelineLayout get() const;

        void reset(VkPipelineLayout handle, VulkanDevice& device);
        void destroy(VulkanDevice& device);

    public:
        VkPipelineLayout handle_ = VK_NULL_HANDLE;
    };


    class FbufImageBundle {

    public:
        void init(
            uint32_t max_frames_in_flight,
            uint32_t width,
            uint32_t height,
            mirinae::ITextureManager& tex_man,
            mirinae::VulkanDevice& device
        );

        void destroy();

        uint32_t width() const;
        uint32_t height() const;
        VkExtent2D extent() const;

        VkFormat depth_format() const { return depth_.front()->format(); }
        VkFormat albedo_format() const { return albedo_.front()->format(); }
        VkFormat normal_format() const { return normal_.front()->format(); }
        VkFormat material_format() const { return material_.front()->format(); }
        VkFormat compo_format() const { return compo_.front()->format(); }

        mirinae::ITexture& depth(uint32_t f_index);
        mirinae::ITexture& albedo(uint32_t f_index);
        mirinae::ITexture& normal(uint32_t f_index);
        mirinae::ITexture& material(uint32_t f_index);
        mirinae::ITexture& compo(uint32_t f_index);

    private:
        std::vector<std::unique_ptr<mirinae::ITexture>> depth_;
        std::vector<std::unique_ptr<mirinae::ITexture>> albedo_;
        std::vector<std::unique_ptr<mirinae::ITexture>> normal_;
        std::vector<std::unique_ptr<mirinae::ITexture>> material_;
        std::vector<std::unique_ptr<mirinae::ITexture>> compo_;
    };


    class IRenderPassBundle {

    public:
        IRenderPassBundle(mirinae::VulkanDevice& device) : device_(device) {}

        virtual ~IRenderPassBundle() = default;
        virtual void destroy() = 0;

        virtual VkFramebuffer fbuf_at(uint32_t index) const = 0;
        virtual const VkClearValue* clear_values() const = 0;
        virtual uint32_t clear_value_count() const = 0;

        VkRenderPass renderpass() const { return renderpass_; }
        VkPipeline pipeline() const { return pipeline_; }
        VkPipelineLayout pipeline_layout() const { return layout_; }

    protected:
        mirinae::VulkanDevice& device_;
        mirinae::RenderPass renderpass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout layout_;
    };


    class IRenderPassRegistry {

    public:
        virtual ~IRenderPassRegistry() = default;

        virtual void add(
            const std::string& name, std::unique_ptr<IRenderPassBundle>&& rp
        ) = 0;

        virtual const IRenderPassBundle& get(const std::string& name) const = 0;

        template <typename T, typename... Args>
        void add(const std::string& name, Args&&... args) {
            this->add(name, std::make_unique<T>(std::forward<Args>(args)...));
        }
    };


    struct IShadowMapBundle {
        struct IDlightShadowMap {
            virtual ~IDlightShadowMap() = default;

            virtual VkImage img(mirinae::FrameIndex f_idx) const = 0;
            virtual VkImageView view(mirinae::FrameIndex f_idx) const = 0;
            virtual VkFramebuffer fbuf(mirinae::FrameIndex f_idx) const = 0;
            virtual entt::entity entt() const = 0;
        };

        struct IDlightShadowMapBundle {
            virtual ~IDlightShadowMapBundle() = default;
            virtual uint32_t count() const = 0;
            virtual IDlightShadowMap& at(uint32_t index) = 0;
            virtual const IDlightShadowMap& at(uint32_t index) const = 0;
        };

        virtual ~IShadowMapBundle() = default;

        virtual IDlightShadowMapBundle& dlights() = 0;
        virtual const IDlightShadowMapBundle& dlights() const = 0;

        virtual uint32_t slight_count() const = 0;
        virtual entt::entity slight_entt_at(size_t idx) = 0;
        virtual VkImage slight_img_at(size_t idx) = 0;
        virtual VkImageView slight_view_at(size_t idx) = 0;
    };
    using HShadowMaps = std::shared_ptr<IShadowMapBundle>;


    struct IEnvmapBundle {
        virtual ~IEnvmapBundle() = default;
        virtual uint32_t count() const = 0;
        virtual glm::dvec3 pos_at(uint32_t index) const = 0;
        virtual VkImageView diffuse_at(uint32_t index) const = 0;
        virtual VkImageView specular_at(uint32_t index) const = 0;
        virtual VkImageView brdf_lut() const = 0;
    };
    using HEnvmapBundle = std::shared_ptr<IEnvmapBundle>;


    class RpCommandPool {

    public:
        RpCommandPool();
        ~RpCommandPool();

        void init(VulkanDevice& device);
        void destroy(VulkanDevice& device);

        void reset_pool(FrameIndex, VulkanDevice&);
        VkCommandBuffer get(FrameIndex, uint32_t threadnum, VulkanDevice&);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };


    class RenderTargetManager {

    public:
        using str = std::string;

        class Image {

        public:
            Image(const str& id) : id_(id) {}
            const std::string& id() const { return id_; }

            mirinae::Image img_;
            mirinae::ImageView view_;

        private:
            std::string id_;
        };
        using HImage = std::shared_ptr<Image>;

    public:
        RenderTargetManager(VulkanDevice& device);
        ~RenderTargetManager();

        // Remove the user from user list of the image.
        // If the user list becomes empty, the image will be freed.
        void free_img(const str& id, const str& user_id);

        // The user will be a writer.
        // The ID will be made like this: `<user_id>:<name>`.
        HImage new_img(const str& name, const str& user_id);

        // The user will be a reader.
        // Returns nullptr if the image does not exist.
        HImage get_img_reader(const str& id, const str& user_id);

    private:
        class ImageRecord;

        VulkanDevice& device_;
        std::map<str, ImageRecord> imgs_;
    };

    using RpImage = RenderTargetManager::Image;
    using HRpImage = std::shared_ptr<RpImage>;


    class RpResources {

    public:
        RpResources(sung::HTaskSche task_sche, VulkanDevice& device);
        ~RpResources();

        DesclayoutManager desclays_;
        FbufImageBundle gbuf_;
        HEnvmapBundle envmaps_;
        HShadowMaps shadow_maps_;
        HTexMgr tex_man_;
        RenderTargetManager ren_img_;
        RpCommandPool cmd_pool_;

        VulkanDevice& device_;
    };


    struct ViewFrustum {
        void update(const glm::dmat4& proj, const glm::dmat4& view);

        std::array<glm::vec3, 8> vtx_;
        std::array<glm::vec3, 6> axes_;
        glm::dmat4 view_inv_;
    };


    class DebugRender : public IDebugRen {

    public:
        struct Triangle {
            std::array<glm::vec4, 4> vertices_;
            glm::vec4 color_{ 1, 0, 0, 0.5f };
        };

        struct TriangleWorld {
            std::array<glm::vec3, 4> vertices_;
            glm::vec4 color_{ 1, 0, 0, 0.5f };
        };

    public:
        void tri(
            const glm::vec3& p0,
            const glm::vec3& p1,
            const glm::vec3& p2,
            const glm::vec4& color
        ) override {
            auto& t = tri_world_.emplace_back();
            t.vertices_[0] = p0;
            t.vertices_[1] = p1;
            t.vertices_[2] = p2;
            t.color_ = color;
        }

        void mesh(const DebugMesh& mesh, const glm::mat4& model) override {
            auto& dst = meshes_.emplace_back();
            dst.mesh_ = &mesh;
            dst.model_mat_ = model;
        }

    public:
        void clear() {
            tri_.clear();
            tri_world_.clear();
            meshes_.clear();
        }

        Triangle& new_tri() { return tri_.emplace_back(); }

        void add_tri(
            const glm::vec4& p0, const glm::vec4& p1, const glm::vec4& p2
        ) {
            auto& tri = this->new_tri();
            tri.vertices_[0] = p0;
            tri.vertices_[1] = p1;
            tri.vertices_[2] = p2;
        }

        void add_tri(
            const glm::vec4& p0,
            const glm::vec4& p1,
            const glm::vec4& p2,
            const glm::vec4& color
        ) {
            auto& tri = this->new_tri();
            tri.vertices_[0] = p0;
            tri.vertices_[1] = p1;
            tri.vertices_[2] = p2;
            tri.color_ = color;
        }

        struct MeshActor {
            const DebugMesh* mesh_;
            glm::dmat4 model_mat_;
        };

        std::vector<Triangle> tri_;
        std::vector<TriangleWorld> tri_world_;
        std::vector<MeshActor> meshes_;
    };


    class CamGeometry {

    public:
        void update(
            const cpnt::StandardCamera& cam,
            const TransformQuat<double>& tform,
            const uint32_t width,
            const uint32_t height
        ) {
            proj_mat_ = cam.proj_.make_proj_mat(width, height);
            view_mat_ = tform.make_view_mat();
            view_pos_ = tform.pos_;
            fov_ = cam.proj_.fov_;

            proj_inv_ = glm::inverse(proj_mat_);
            view_inv_ = glm::inverse(view_mat_);

            view_frustum_.update(proj_mat_, view_mat_);
        }

        auto& proj() const { return proj_mat_; }
        auto& proj_inv() const { return proj_inv_; }
        auto& view() const { return view_mat_; }
        auto& view_inv() const { return view_inv_; }
        auto pv() const { return proj_mat_ * view_mat_; }

        auto& view_pos() const { return view_pos_; }
        auto& fov() const { return fov_; }
        auto& view_frustum() const { return view_frustum_; }

    private:
        ViewFrustum view_frustum_;
        glm::dmat4 proj_mat_;
        glm::dmat4 view_mat_;
        glm::dmat4 proj_inv_;
        glm::dmat4 view_inv_;
        glm::dvec3 view_pos_;
        sung::TAngle<double> fov_;
    };


    struct CamCache : public CamGeometry {

    public:
        void update(
            const cpnt::StandardCamera& cam,
            const TransformQuat<double>& tform,
            const uint32_t width,
            const uint32_t height
        ) {
            CamGeometry::update(cam, tform, width, height);

            exposure_ = cam.exposure_;
            gamma_ = cam.gamma_;
        }

        float exposure_ = 1;
        float gamma_ = 1;
    };


    // Must be thread safe at all cost.
    struct RpCtxt {
        CamCache main_cam_;
        FrameIndex f_index_;
        ShainImageIndex i_index_;
        double dt_;
    };


    struct RpContext : public RpCtxt {
        std::shared_ptr<CosmosSimulator> cosmos_;
        std::shared_ptr<DrawSheet> draw_sheet_;
        RpResources* rp_res_;
        DebugRender debug_ren_;
        ViewFrustum view_frustum_;
        glm::dmat4 proj_mat_;
        glm::dmat4 view_mat_;
        glm::dvec3 view_pos_;
        VkCommandBuffer cmdbuf_;
    };


    struct IRpStates {
        virtual ~IRpStates() = default;
        virtual const std::string& name() const = 0;

        virtual void record(const RpContext& context) {}
    };

    using URpStates = std::unique_ptr<IRpStates>;


    struct IPipelinePair {
        virtual ~IPipelinePair() = default;
        virtual VkPipeline pipeline() const = 0;
        virtual VkPipelineLayout pipe_layout() const = 0;
    };


    struct IRenPass : public IPipelinePair {
        virtual VkRenderPass render_pass() const = 0;
        virtual const VkClearValue* clear_values() const = 0;
        virtual uint32_t clear_value_count() const = 0;
    };


    template <uint32_t N>
    class RenPassBundle : public IRenPass {

    public:
        VkRenderPass render_pass() const override { return render_pass_.get(); }

        VkPipeline pipeline() const override { return pipeline_.get(); }

        VkPipelineLayout pipe_layout() const override {
            return pipe_layout_.get();
        }

        const VkClearValue* clear_values() const override {
            return clear_values_.data();
        }

        uint32_t clear_value_count() const override { return N; }

    protected:
        void destroy_render_pass_elements(mirinae::VulkanDevice& device) {
            render_pass_.destroy(device);
            pipeline_.destroy(device);
            pipe_layout_.destroy(device);
        }

        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
        std::array<VkClearValue, N> clear_values_;
    };


    struct IRpTask {
        virtual ~IRpTask() = default;
        virtual std::string_view name() const = 0;
        virtual void prepare(const mirinae::RpCtxt&) = 0;
        virtual void collect_cmdbuf(std::vector<VkCommandBuffer>&) {}

        virtual enki::ITaskSet* update_task() { return nullptr; }
        virtual enki::ITaskSet* update_fence() { return nullptr; }
        virtual enki::ITaskSet* record_task() { return nullptr; }
        virtual enki::ITaskSet* record_fence() { return nullptr; }
    };


    struct IRpBase {
        virtual ~IRpBase() = default;
        virtual std::string_view name() const = 0;
        virtual void render_imgui() {}
        virtual void on_resize(uint32_t width, uint32_t height) {}

        virtual std::unique_ptr<IRpTask> create_task() = 0;
    };


    struct RpCreateBundle {
        RpCreateBundle(
            CosmosSimulator& cosmos, RpResources& rp_res, VulkanDevice& device
        )
            : cosmos_(cosmos), rp_res_(rp_res), device_(device) {}

        CosmosSimulator& cosmos_;
        RpResources& rp_res_;
        VulkanDevice& device_;
    };

}  // namespace mirinae
