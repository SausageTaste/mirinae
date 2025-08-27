#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/misc/misc.hpp"

#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/mem_cinfo.hpp"
#include "mirinae/renderpass/builder.hpp"


namespace {

    struct DebugVertex {
        glm::vec4 pos_;
        glm::vec4 color_;
    };


    class VertexArray {

    public:
        void add(const glm::vec4& pos, const glm::vec4& color) {
            auto& v = data_.emplace_back();
            v.pos_ = pos;
            v.color_ = color;
        }

        void clear() { data_.clear(); }

        const ::DebugVertex* data() const {
            if (data_.empty())
                return nullptr;
            else
                return data_.data();
        }

        size_t size() const { return data_.size(); }

    private:
        std::vector<::DebugVertex> data_;
    };


    class DebugVertexBuffer {

    public:
        void set_data(
            const DebugVertex* vertices,
            size_t vertex_count,
            mirinae::VulkanDevice& device
        ) {
            const auto buf_size = vertex_count * sizeof(::DebugVertex);

            if (0 == buf_size) {
                vertex_count_ = 0;
                return;
            }

            if (buf_.size() < buf_size) {
                mirinae::BufferCreateInfo cinfo;
                cinfo.set_size(buf_size)
                    .add_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                    .add_usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
                    .add_alloc_flag_host_access_seq_write();
                buf_.init(cinfo, device.mem_alloc());
            }

            buf_.set_data(vertices, buf_size, device.mem_alloc());
            vertex_count_ = vertex_count;
        }

        void destroy(mirinae::VulkanDevice& device) {
            buf_.destroy(device.mem_alloc());
        }

        void record_bind(VkCommandBuffer cmdbuf) const {
            VkBuffer vertex_buffers[] = { buf_.buffer() };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(cmdbuf, 0, 1, vertex_buffers, offsets);
        }

        size_t vertex_count() const { return vertex_count_; }

    private:
        mirinae::Buffer buf_;
        size_t vertex_count_ = 0;
    };


    struct FrameData {
        ::DebugVertexBuffer vertex_buf_;
        ::VertexArray vertices_;
        mirinae::Fbuf fbuf_;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;

}  // namespace


namespace {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            ::FrameDataArr& frame_data,
            const mirinae::DebugRender& debug_ren,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            cmd_pool_ = &cmd_pool;
            debug_ren_ = &debug_ren;
            device_ = &device;
            frame_data_ = &frame_data;
            gbufs_ = &gbufs;
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
            const auto res = this->record(
                cmdbuf_,
                *debug_ren_,
                *rp_,
                *ctxt_,
                gbufs_->extent(),
                frame_data_->at(ctxt_->f_index_.get()),
                *device_
            );
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);

            if (!res) {
                cmdbuf_ = VK_NULL_HANDLE;
                return;
            }
        }

        static void record_barriers(
            const VkCommandBuffer cmdbuf,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::RpCtxt& ctxt
        ) {
            mirinae::ImageMemoryBarrier{}
                .image(gbufs.depth(ctxt.f_index_.get()).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .old_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .add_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
                .add_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                );

            mirinae::ImageMemoryBarrier{}
                .image(gbufs.compo(ctxt.f_index_.get()).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .add_src_acc(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .add_dst_acc(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );
        }

        static bool record(
            const VkCommandBuffer cmdbuf,
            const mirinae::DebugRender& debug_ren,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt,
            const VkExtent2D& fbuf_ext,
            ::FrameData& fd,
            mirinae::VulkanDevice& device
        ) {
            auto& vertices = fd.vertices_;
            vertices.clear();

            for (auto& tri : debug_ren.tri_) {
                vertices.add(tri.vertices_[0], tri.color_);
                vertices.add(tri.vertices_[1], tri.color_);
                vertices.add(tri.vertices_[2], tri.color_);
            }

            const auto& proj_mat = ctxt.main_cam_.proj();
            const auto& view_mat = ctxt.main_cam_.view();
            const auto proj_inv = ctxt.main_cam_.proj_inv();
            const auto view_inv = ctxt.main_cam_.view_inv();
            const auto pv = proj_mat * view_mat;

            for (auto& tri : debug_ren.tri_world_) {
                vertices.add(pv * glm::dvec4(tri.vertices_[0], 1), tri.color_);
                vertices.add(pv * glm::dvec4(tri.vertices_[1], 1), tri.color_);
                vertices.add(pv * glm::dvec4(tri.vertices_[2], 1), tri.color_);
            }

            for (auto& mactor : debug_ren.meshes_) {
                auto& mesh = *mactor.mesh_;
                const auto tri_count = mesh.idx_.size() / 3;
                const auto pvm = glm::mat4(pv * mactor.model_mat_);

                for (size_t i = 0; i < tri_count; ++i) {
                    const auto i0 = mesh.idx_[3 * i + 0];
                    const auto i1 = mesh.idx_[3 * i + 1];
                    const auto i2 = mesh.idx_[3 * i + 2];

                    const auto& v0 = mesh.vtx_[i0];
                    const auto& v1 = mesh.vtx_[i1];
                    const auto& v2 = mesh.vtx_[i2];

                    vertices.add(pvm * glm::vec4(v0.pos_, 1), v0.color_);
                    vertices.add(pvm * glm::vec4(v1.pos_, 1), v1.color_);
                    vertices.add(pvm * glm::vec4(v2.pos_, 1), v2.color_);
                }
            }

            if (vertices.size() == 0) {
                return false;
            }

            fd.vertex_buf_.set_data(vertices.data(), vertices.size(), device);

            mirinae::RenderPassBeginInfo{}
                .rp(rp.render_pass())
                .fbuf(fd.fbuf_.get())
                .wh(fbuf_ext)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cmdbuf);

            fd.vertex_buf_.record_bind(cmdbuf);
            vkCmdDraw(cmdbuf, fd.vertex_buf_.vertex_count(), 1, 0, 0);

            vkCmdEndRenderPass(cmdbuf);
            return true;
        }

        const mirinae::DebugLabel DEBUG_LABEL{ "Debug" };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;
        ::VertexArray vertices_;

        const mirinae::DebugRender* debug_ren_ = nullptr;
        const mirinae::FbufImageBundle* gbufs_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        ::FrameDataArr* frame_data_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        void init(
            ::FrameDataArr& frame_data,
            const mirinae::DebugRender& debug_ren,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(
                frame_data, debug_ren, gbufs, rp, cmd_pool, device
            );
        }

        std::string_view name() const override { return "debug render pass"; }

        void prepare(const mirinae::RpCtxt& ctxt) override {
            record_tasks_.prepare(ctxt);
        }

        void collect_cmdbuf(std::vector<VkCommandBuffer>& out) override {
            record_tasks_.collect_cmdbuf(out);
        }

        enki::ITaskSet* record_task() override { return &record_tasks_; }

        enki::ITaskSet* record_fence() override {
            return &record_tasks_.fence();
        }

    private:
        DrawTasks record_tasks_;
    };

}  // namespace


namespace {

    class RenderPassDebug
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<2> {

    public:
        RenderPassDebug(
            const mirinae::DebugRender& debug_ren,
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : debug_ren_(debug_ren)
            , cosmos_(cosmos)
            , rp_res_(rp_res)
            , device_(device) {
            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}.build(pipe_layout_, device_);
            }

            this->on_resize(rp_res_.gbuf_.width(), rp_res_.gbuf_.height());
        }

        ~RenderPassDebug() {
            for (auto& fd : fdata_) {
                fd.fbuf_.destroy(device_.logi_device());
                fd.vertex_buf_.destroy(device_);
            }

            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "debug mesh"; }

        void on_resize(uint32_t width, uint32_t height) override {
            auto& gbufs = rp_res_.gbuf_;

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(gbufs.depth_format())
                    .ini_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .fin_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();
                builder.attach_desc()
                    .add(gbufs.compo_format())
                    .ini_lay(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_lay(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();

                builder.depth_attach_ref().set(0);
                builder.color_attach_ref().add_color_attach(1);

                builder.subpass_dep().add().preset_single();

                render_pass_.reset(
                    builder.build(device_.logi_device()), device_
                );
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device_ };

                builder.shader_stages()
                    .add_vert(":asset/spv/debug_mesh_vert.spv")
                    .add_frag(":asset/spv/debug_mesh_frag.spv");

                builder.vertex_input_state()
                    .add_binding<::DebugVertex>()
                    .add_attrib_vec4(offsetof(DebugVertex, pos_))
                    .add_attrib_vec4(offsetof(DebugVertex, color_));

                builder.rasterization_state().polygon_mode_line();

                builder.depth_stencil_state()
                    .depth_test_enable(true)
                    .depth_write_enable(false)
                    .depth_compare_op(VK_COMPARE_OP_GREATER_OR_EQUAL);

                builder.color_blend_state().add(true, 1);

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_.reset(
                    builder.build(render_pass_, pipe_layout_), device_
                );
            }

            // Framebuffers
            for (size_t i = 0; i < fdata_.size(); i++) {
                auto& fd = fdata_.at(i);

                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(render_pass_.get())
                    .set_dim(gbufs.extent())
                    .add_attach(gbufs.depth(i).image_view())
                    .add_attach(gbufs.compo(i).image_view());

                fd.fbuf_.init(fbuf_cinfo.get(), device_.logi_device());
            }
        }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto task = std::make_unique<RpTask>();
            task->init(
                fdata_,
                debug_ren_,
                rp_res_.gbuf_,
                *this,
                rp_res_.cmd_pool_,
                device_
            );
            return task;
        }

    private:
        const mirinae::DebugRender& debug_ren_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;
        mirinae::VulkanDevice& device_;

        FrameDataArr fdata_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<mirinae::IRpBase> create_rp_debug(
        RpCreateBundle& bundle, const DebugRender& debug_ren
    ) {
        return std::make_unique<RenderPassDebug>(
            debug_ren, bundle.cosmos_, bundle.rp_res_, bundle.device_
        );
    }

}  // namespace mirinae::rp
