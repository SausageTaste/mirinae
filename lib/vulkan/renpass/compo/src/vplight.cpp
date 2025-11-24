#include "mirinae/vulkan/renpass/compo/compo.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/vulkan/base/render/cmdbuf.hpp"
#include "mirinae/vulkan/base/render/draw_set.hpp"
#include "mirinae/vulkan/base/renderpass/builder.hpp"


namespace {

    struct U_CompoVplightPushConst {

    public:
        U_CompoVplightPushConst& set_proj_inv(const glm::mat4& m) {
            proj_inv_ = m;
            return *this;
        }

        U_CompoVplightPushConst& set_light_pos_v(const glm::vec3& v) {
            light_pos_v_.x = v.x;
            light_pos_v_.y = v.y;
            light_pos_v_.z = v.z;
            return *this;
        }

        U_CompoVplightPushConst& set_light_color(const glm::vec3& v) {
            light_color_.x = v.x;
            light_color_.y = v.y;
            light_color_.z = v.z;
            return *this;
        }

    private:
        glm::mat4 proj_inv_;
        glm::vec4 light_pos_v_;
        glm::vec4 light_color_;
    };

    static_assert(sizeof(U_CompoVplightPushConst) < 128);


    struct FrameData {
        mirinae::Fbuf fbuf_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;

}  // namespace


// Tasks
namespace {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            const mirinae::IShadowMapBundle& shadow_maps,
            ::FrameDataArr& fdata,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            cmd_pool_ = &cmd_pool;
            device_ = &device;
            shadows_ = &shadow_maps;
            fdata_ = &fdata;
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

            auto& fd = fdata_->at(ctxt_->f_index_.get());
            const auto gbuf_ext = gbufs_->extent();

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            this->record_barriers(cmdbuf_, *gbufs_, *ctxt_);
            this->record(cmdbuf_, fd, *reg_, *rp_, *ctxt_, gbuf_ext);
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
                .old_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .new_lay(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
                .add_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                .set_dst_acc(VK_ACCESS_SHADER_READ_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const ::FrameData& fd,
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt,
            const VkExtent2D& fbuf_ext
        ) {
            const auto& view_mat = ctxt.main_cam_.view();
            const auto& view_inv = ctxt.main_cam_.view_inv();

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

            mirinae::DescSetBindInfo{}
                .layout(rp.pipe_layout())
                .set(fd.desc_set_)
                .record(cmdbuf);

            for (auto e : reg.view<mirinae::cpnt::VPLight>()) {
                auto vplight = reg.try_get<mirinae::cpnt::VPLight>(e);
                if (nullptr == vplight)
                    continue;

                glm::dvec3 pos_m{ 0 };
                if (auto tform = reg.try_get<mirinae::cpnt::Transform>(e))
                    pos_m = tform->pos_;
                const auto pos_v = view_mat * glm::dvec4(pos_m, 1);

                U_CompoVplightPushConst pc;
                pc.set_proj_inv(ctxt.main_cam_.proj_inv())
                    .set_light_pos_v(pos_v)
                    .set_light_color(vplight->volume_light_color());

                mirinae::PushConstInfo{}
                    .layout(rp.pipe_layout())
                    .add_stage_frag()
                    .record(cmdbuf, pc);

                vkCmdDraw(cmdbuf, 3, 1, 0, 0);
            }

            vkCmdEndRenderPass(cmdbuf);
        }

        const mirinae::DebugLabel DEBUG_LABEL{ "Compo VPLight", 1, 0.96, 0.61 };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const entt::registry* reg_ = nullptr;
        const mirinae::FbufImageBundle* gbufs_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::IShadowMapBundle* shadows_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        ::FrameDataArr* fdata_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        void init(
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            const mirinae::IShadowMapBundle& shadow_maps,
            ::FrameDataArr& fdata,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(
                reg, gbufs, rp, shadow_maps, fdata, cmd_pool, device
            );
        }

        std::string_view name() const override { return "composition vplight"; }

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


// Compo Slight
namespace {

    class RpStatesCompoVplight
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<1> {

    public:
        RpStatesCompoVplight(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
            namespace cpnt = mirinae::cpnt;
            auto& reg = cosmos.reg();
            auto& desclays = rp_res_.desclays_;

            // Desc layout
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
                builder
                    .add_img_frag(1)   // depth
                    .add_img_frag(1)   // albedo
                    .add_img_frag(1)   // normal
                    .add_img_frag(1);  // material
                desclays.add(builder, device.logi_device());
            }

            // Desc sets
            this->recreate_desc_sets(frame_data_, desc_pool_, device_);

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclays.get(name_s() + ":main").layout())
                    .add_frag_flag()
                    .pc<U_CompoVplightPushConst>()
                    .build(pipe_layout_, device);
            }

            // Render pass
            this->recreate_render_pass(render_pass_, device_);

            // Pipeline
            this->recreate_pipeline(pipeline_, device_);

            // Framebuffers
            this->recreate_fbufs(frame_data_, device_);

            // Misc
            {
                clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            }

            return;
        }

        ~RpStatesCompoVplight() override {
            for (auto& fd : frame_data_) {
                fd.fbuf_.destroy(device_.logi_device());
            }

            desc_pool_.destroy(device_.logi_device());
            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "compo_slight"; }

        void on_resize(uint32_t width, uint32_t height) override {
            this->recreate_desc_sets(frame_data_, desc_pool_, device_);
            this->recreate_render_pass(render_pass_, device_);
            this->recreate_pipeline(pipeline_, device_);
            this->recreate_fbufs(frame_data_, device_);
        }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<::RpTask>();
            out->init(
                cosmos_.reg(),
                rp_res_.gbuf_,
                *this,
                *rp_res_.shadow_maps_,
                frame_data_,
                rp_res_.cmd_pool_,
                device_
            );
            return out;
        }

    private:
        static entt::entity select_atmos_simple(entt::registry& reg) {
            for (auto entity : reg.view<mirinae::cpnt::AtmosphereSimple>())
                return entity;

            return entt::null;
        }

        void recreate_desc_sets(
            ::FrameDataArr& fdata,
            mirinae::DescPool& desc_pool,
            mirinae::VulkanDevice& device
        ) const {
            auto& gbufs = rp_res_.gbuf_;
            auto& desclays = rp_res_.desclays_;
            auto& desc_layout = desclays.get(name_s() + ":main");

            desc_pool.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desc_layout.size_info(),
                device.logi_device()
            );

            auto desc_sets = desc_pool.alloc(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desc_layout.layout(),
                device.logi_device()
            );

            mirinae::DescWriter writer;
            for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                const mirinae::FrameIndex f_idx(i);

                auto& fd = fdata[i];
                fd.desc_set_ = desc_sets[i];

                // Depth
                writer.add_img_info()
                    .set_img_view(gbufs.depth(f_idx).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 0);
                // Albedo
                writer.add_img_info()
                    .set_img_view(gbufs.albedo(f_idx).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 1);
                // Normal
                writer.add_img_info()
                    .set_img_view(gbufs.normal(f_idx).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 2);
                // Material
                writer.add_img_info()
                    .set_img_view(gbufs.material(f_idx).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 3);
            }
            writer.apply_all(device.logi_device());
        }

        void recreate_render_pass(
            mirinae::RenderPass& render_pass, mirinae::VulkanDevice& device
        ) const {
            mirinae::RenderPassBuilder builder;

            builder.attach_desc()
                .add(rp_res_.gbuf_.compo_format())
                .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .op_pair_load_store();

            builder.color_attach_ref().add_color_attach(0);

            builder.subpass_dep().add().preset_single();

            render_pass.reset(builder.build(device.logi_device()), device);
        }

        void recreate_pipeline(
            mirinae::RpPipeline& pipeline, mirinae::VulkanDevice& device
        ) const {
            mirinae::PipelineBuilder builder{ device };

            builder.shader_stages()
                .add_vert(":asset/spv/compo_vplight_vert.spv")
                .add_frag(":asset/spv/compo_vplight_frag.spv");

            builder.rasterization_state().cull_mode_back();

            builder.color_blend_state().add().set_additive_blend();

            builder.dynamic_state().add_viewport().add_scissor();

            pipeline.reset(builder.build(render_pass_, pipe_layout_), device);
        }

        void recreate_fbufs(
            ::FrameDataArr& fdata, mirinae::VulkanDevice& device
        ) const {
            const auto& gbuf = rp_res_.gbuf_;

            for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                const mirinae::FrameIndex f_idx(i);

                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(render_pass_)
                    .set_dim(gbuf.width(), gbuf.height())
                    .add_attach(gbuf.compo(f_idx).image_view());

                fdata.at(i).fbuf_.reset(
                    fbuf_cinfo.build(device), device.logi_device()
                );
            }
        }

        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        FrameDataArr frame_data_;

        std::shared_ptr<mirinae::ITexture> sky_tex_;
        mirinae::DescPool desc_pool_;
    };

}  // namespace


namespace mirinae::rp::compo {

    std::unique_ptr<IRpBase> create_rps_vplight(RpCreateBundle& cbundle) {
        return std::make_unique<RpStatesCompoVplight>(
            cbundle.cosmos_, cbundle.rp_res_, cbundle.device_
        );
    }

}  // namespace mirinae::rp::compo
