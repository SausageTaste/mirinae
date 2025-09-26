#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/gbuf/gbuf.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/draw_set.hpp"
#include "mirinae/renderee/terrain.hpp"
#include "mirinae/renderpass/builder.hpp"


namespace {

    class U_GbufTerrainPushConst {

    public:
        U_GbufTerrainPushConst& pvm(
            const glm::dmat4& proj,
            const glm::dmat4& view,
            const glm::dmat4& model
        ) {
            const auto vm = view * model;
            view_model_ = vm;
            pvm_ = proj * vm;
            return *this;
        }

        U_GbufTerrainPushConst& fbuf_size(const VkExtent2D& x) {
            fbuf_size_.x = static_cast<float>(x.width);
            fbuf_size_.y = static_cast<float>(x.height);
            return *this;
        }

        template <typename T>
        U_GbufTerrainPushConst& len_per_texel(T x, T y) {
            len_per_texel_.x = static_cast<float>(x);
            len_per_texel_.y = static_cast<float>(y);
            return *this;
        }

        U_GbufTerrainPushConst& height_scale(float x) {
            height_scale_ = x;
            return *this;
        }

        U_GbufTerrainPushConst& tess_factor(float x) {
            tess_factor_ = x;
            return *this;
        }

    private:
        glm::mat4 pvm_;
        glm::mat4 view_model_;
        glm::vec2 fbuf_size_;
        glm::vec2 len_per_texel_;
        float height_scale_;
        float tess_factor_;
    };


    struct FrameData {
        mirinae::Fbuf fbuf_;
        VkExtent2D fbuf_size_;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;

}  // namespace


namespace {

    class UpdateTask : public mirinae::DependingTask {

    public:
        UpdateTask() { fence_.succeed(this); }

        void init(
            entt::registry& reg,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        ) {
            reg_ = &reg;
            rp_res_ = &rp_res;
            device_ = &device;
        }

        enki::ITaskSet& fence() { return fence_; }

        void prepare() {
            this->set_size(reg_->view<mirinae::cpnt::Terrain>().size());
        }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            auto view = reg_->view<mirinae::cpnt::Terrain>();
            auto it = view.begin() + range.start;
            auto end = view.begin() + range.end;

            for (; it != end; ++it) {
                const auto e = *it;
                auto& terr = reg_->get<mirinae::cpnt::Terrain>(e);
                this->update(terr, *rp_res_, *device_, tex_mut_);
            }
        }

        static void update(
            mirinae::cpnt::Terrain& terr,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device,
            std::mutex& tex_mut
        ) {
            auto unit = terr.ren_unit<mirinae::RenUnitTerrain>();
            if (!unit) {
                std::lock_guard<std::mutex> lock(tex_mut);
                terr.ren_unit_ = std::make_unique<mirinae::RenUnitTerrain>(
                    terr, *rp_res.tex_man_, rp_res.desclays_, device
                );
            }
        }

        mirinae::FenceTask fence_;
        std::mutex tex_mut_;

        entt::registry* reg_ = nullptr;
        mirinae::RpResources* rp_res_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const ::FrameDataArr& frame_data,
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            cmd_pool_ = &cmd_pool;
            device_ = &device;
            frame_data_ = &frame_data;
            gbufs_ = &gbufs;
            reg_ = &reg;
            rp_ = &rp;
        }

        void prepare(const mirinae::RpCtxt& ctxt) { ctxt_ = &ctxt; }

        enki::ITaskSet& fence() { return fence_; }

        void collect_cmdbuf(std::vector<VkCommandBuffer>& out) {
            if (VK_NULL_HANDLE != cmdbuf_) {
                out.push_back(cmdbuf_);
            }
        }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            cmdbuf_ = cmd_pool_->get(ctxt_->f_index_, tid, *device_);
            if (cmdbuf_ == VK_NULL_HANDLE)
                return;

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            this->record_barriers(cmdbuf_, *gbufs_, *ctxt_);
            this->record(
                cmdbuf_,
                frame_data_->at(ctxt_->f_index_.get()),
                *reg_,
                *rp_,
                *ctxt_
            );
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void record_barriers(
            const VkCommandBuffer cmdbuf,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::RpCtxt& ctxt
        ) {
            mirinae::ImageMemoryBarrier{}
                .image(gbufs.depth(ctxt.f_index_).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .old_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .set_src_access(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                );

            mirinae::ImageMemoryBarrier color_barrier{};
            color_barrier.set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .set_src_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_signle_mip_layer();

            color_barrier.image(gbufs.albedo(ctxt.f_index_).image())
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );

            color_barrier.image(gbufs.normal(ctxt.f_index_).image())
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );

            color_barrier.image(gbufs.material(ctxt.f_index_).image())
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const ::FrameData& fd,
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt
        ) {
            namespace cpnt = ::mirinae::cpnt;

            mirinae::RenderPassBeginInfo{}
                .rp(rp.render_pass())
                .fbuf(fd.fbuf_.get())
                .wh(fd.fbuf_size_)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fd.fbuf_size_ }.record_single(cmdbuf);
            mirinae::Rect2D{ fd.fbuf_size_ }.record_scissor(cmdbuf);

            mirinae::PushConstInfo pc_info;
            pc_info.layout(rp.pipe_layout())
                .add_stage_vert()
                .add_stage_tesc()
                .add_stage_tese()
                .add_stage_frag();

            for (auto e : reg.view<cpnt::Terrain>()) {
                auto& terr = reg.get<cpnt::Terrain>(e);
                auto unit = terr.ren_unit<mirinae::RenUnitTerrain>();
                if (!unit->is_ready())
                    continue;

                mirinae::DescSetBindInfo{}
                    .layout(rp.pipe_layout())
                    .add(unit->desc_set())
                    .record(cmdbuf);

                glm::dmat4 model_mat(1);
                if (auto tform = reg.try_get<cpnt::Transform>(e))
                    model_mat = tform->make_model_mat();

                const auto hz = unit->height_map_size();
                const auto x_per_texel = terr.terrain_width_ / hz.width;
                const auto y_per_texel = terr.terrain_height_ / hz.height;

                ::U_GbufTerrainPushConst pc;
                pc.pvm(ctxt.main_cam_.proj(), ctxt.main_cam_.view(), model_mat)
                    .fbuf_size(fd.fbuf_size_)
                    .len_per_texel(x_per_texel, y_per_texel)
                    .height_scale(terr.height_scale_)
                    .tess_factor(terr.tess_factor_);
                pc_info.record(cmdbuf, pc);

                unit->draw_indexed(cmdbuf);
            }

            vkCmdEndRenderPass(cmdbuf);
        }

        const mirinae::DebugLabel DEBUG_LABEL{
            "G-buffer Terrain", 0.12, 0.58, 0.95
        };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const ::FrameDataArr* frame_data_ = nullptr;
        const entt::registry* reg_ = nullptr;
        const mirinae::FbufImageBundle* gbufs_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        void init(
            const ::FrameDataArr& frame_data,
            const mirinae::IRenPass& rp,
            entt::registry& reg,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        ) {
            update_task_.init(reg, rp_res, device);

            record_tasks_.init(
                frame_data, reg, rp_res.gbuf_, rp, rp_res.cmd_pool_, device
            );
        }

        std::string_view name() const override { return "gbuf terrain"; }

        void prepare(const mirinae::RpCtxt& ctxt) override {
            update_task_.prepare();
            record_tasks_.prepare(ctxt);
        }

        void collect_cmdbuf(std::vector<VkCommandBuffer>& out) override {
            record_tasks_.collect_cmdbuf(out);
        }

        enki::ITaskSet* update_task() override { return &update_task_; }

        enki::ITaskSet* update_fence() override {
            return &update_task_.fence();
        }

        enki::ITaskSet* record_task() override { return &record_tasks_; }

        enki::ITaskSet* record_fence() override {
            return &record_tasks_.fence();
        }

    private:
        UpdateTask update_task_;
        DrawTasks record_tasks_;
    };

}  // namespace


namespace {

    VkRenderPass create_renderpass(
        VkFormat depth,
        VkFormat albedo,
        VkFormat normal,
        VkFormat material,
        VkDevice logi_device
    ) {
        mirinae::RenderPassBuilder builder;

        builder.attach_desc()
            .add(depth)
            .ini_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .op_pair_load_store();
        builder.attach_desc()
            .add(albedo)
            .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .op_pair_load_store();
        builder.attach_desc().dup(normal);
        builder.attach_desc().dup(material);

        builder.color_attach_ref()
            .add_color_attach(1)   // albedo
            .add_color_attach(2)   // normal
            .add_color_attach(3);  // material

        builder.depth_attach_ref().set(0);

        builder.subpass_dep().add().preset_single();

        return builder.build(logi_device);
    }

    VkPipeline create_pipeline(
        VkRenderPass render_pass,
        VkPipelineLayout pipe_layout,
        mirinae::VulkanDevice& device
    ) {
        using Vertex = mirinae::RenUnitTerrain::Vertex;
        mirinae::PipelineBuilder builder{ device };

        builder.shader_stages()
            .add_vert(":asset/spv/gbuf_terrain_vert.spv")
            .add_tesc(":asset/spv/gbuf_terrain_tesc.spv")
            .add_tese(":asset/spv/gbuf_terrain_tese.spv")
            .add_frag(":asset/spv/gbuf_terrain_frag.spv");

        builder.vertex_input_state()
            .add_binding<Vertex>()
            .add_attrib_vec3(offsetof(Vertex, pos_))
            .add_attrib_vec2(offsetof(Vertex, texco_));

        builder.input_assembly_state().topology_patch_list();

        builder.tes_state().patch_ctrl_points(4);

        builder.rasterization_state().cull_mode_back();
        // builder.rasterization_state().polygon_mode_line();

        builder.depth_stencil_state()
            .depth_test_enable(true)
            .depth_write_enable(true);

        builder.color_blend_state().add(false, 3);

        builder.dynamic_state()
            .add(VK_DYNAMIC_STATE_LINE_WIDTH)
            .add_viewport()
            .add_scissor();

        return builder.build(render_pass, pipe_layout);
    }


    class RpBase
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<4> {

    public:
        RpBase(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : cosmos_(cosmos), rp_res_(rp_res), device_(device) {
            clear_values_.at(0).depthStencil = { 0, 0 };
            clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(2).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            clear_values_.at(3).color = { 0.0f, 0.0f, 0.0f, 1.0f };

            render_pass_ = ::create_renderpass(
                rp_res.gbuf_.depth_format(),
                rp_res.gbuf_.albedo_format(),
                rp_res.gbuf_.normal_format(),
                rp_res.gbuf_.material_format(),
                device.logi_device()
            );

            mirinae::PipelineLayoutBuilder{}
                .desc(rp_res.desclays_.get("gbuf_terrain:main").layout())
                .add_vertex_flag()
                .add_tesc_flag()
                .add_tese_flag()
                .add_frag_flag()
                .pc<U_GbufTerrainPushConst>(0)
                .build(pipe_layout_, device);

            pipeline_ = ::create_pipeline(render_pass_, pipe_layout_, device);

            this->recreate_fbuf(fdata_);
        }

        ~RpBase() {
            for (auto& fd : fdata_) {
                fd.fbuf_.destroy(device_.logi_device());
            }

            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "gbuf terrain"; }

        void on_resize(uint32_t width, uint32_t height) override {
            this->recreate_fbuf(fdata_);
        }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto task = std::make_unique<RpTask>();
            task->init(fdata_, *this, cosmos_.reg(), rp_res_, device_);
            return task;
        }

    private:
        void recreate_fbuf(::FrameDataArr& fdata) const {
            const auto& gbufs = rp_res_.gbuf_;

            for (int i = 0; i < fdata.size(); ++i) {
                const mirinae::FrameIndex f_idx(i);

                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(render_pass_)
                    .add_attach(gbufs.depth(f_idx).image_view())
                    .add_attach(gbufs.albedo(f_idx).image_view())
                    .add_attach(gbufs.normal(f_idx).image_view())
                    .add_attach(gbufs.material(f_idx).image_view())
                    .set_dim(gbufs.extent());

                auto& fd = fdata.at(i);
                fd.fbuf_.init(fbuf_cinfo.get(), device_.logi_device());
                fd.fbuf_size_ = gbufs.extent();
            }
        }

        ::FrameDataArr fdata_;

        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;
        mirinae::VulkanDevice& device_;
    };

}  // namespace


namespace mirinae::rp::gbuf {

    std::unique_ptr<IRpBase> create_rp_gbuf_terrain(RpCreateBundle& cbundle) {
        return std::make_unique<RpBase>(
            cbundle.cosmos_, cbundle.rp_res_, cbundle.device_
        );
    }

}  // namespace mirinae::rp::gbuf
