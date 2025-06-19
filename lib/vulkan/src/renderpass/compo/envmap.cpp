#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/compo.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"


namespace {

    class U_CompoEnvmapPushConst {

    public:
        U_CompoEnvmapPushConst& set_proj_inv(const glm::mat4& m) {
            proj_inv_ = m;
            return *this;
        }

        U_CompoEnvmapPushConst& set_view_inv(const glm::mat4& m) {
            view_inv_ = m;
            return *this;
        }

        U_CompoEnvmapPushConst& set_fog_color(const glm::vec3& v) {
            fog_color_density_.x = v.r;
            fog_color_density_.y = v.g;
            fog_color_density_.z = v.b;
            return *this;
        }

        U_CompoEnvmapPushConst& set_fog_density(float density) {
            fog_color_density_.w = density;
            return *this;
        }

    private:
        glm::mat4 view_inv_;
        glm::mat4 proj_inv_;
        glm::vec4 fog_color_density_{ 0 };
    };


    struct FrameData {
        mirinae::Fbuf fbuf_;
        VkDescriptorSet desc_set_env_ = VK_NULL_HANDLE;
        VkDescriptorSet desc_set_main_ = VK_NULL_HANDLE;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;

}  // namespace


// Tasks
namespace { namespace task {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const ::FrameDataArr& fdata,
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            cmd_pool_ = &cmd_pool;
            device_ = &device;
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
            this->record(cmdbuf_, fd, *reg_, *rp_, *ctxt_, gbuf_ext);
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const ::FrameData& fd,
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt,
            const VkExtent2D& fbuf_ext
        ) {
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
                .first_set(0)
                .set(fd.desc_set_main_)
                .record(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(rp.pipe_layout())
                .first_set(1)
                .set(fd.desc_set_env_)
                .record(cmdbuf);

            U_CompoEnvmapPushConst pc;
            pc.set_proj_inv(ctxt.main_cam_.proj_inv())
                .set_view_inv(ctxt.main_cam_.view_inv());
            for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                pc.set_fog_color(atmos.fog_color_)
                    .set_fog_density(atmos.fog_density_);
                break;
            }

            mirinae::PushConstInfo{}
                .layout(rp.pipe_layout())
                .add_stage_frag()
                .record(cmdbuf, pc);

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmdbuf);
        }

        const mirinae::DebugLabel DEBUG_LABEL{ "Compo Envmap", 1, 0.96, 0.61 };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const ::FrameDataArr* fdata_ = nullptr;
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
            const ::FrameDataArr& fdata,
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(fdata, reg, gbufs, rp, cmd_pool, device);
        }

        std::string_view name() const override { return "composition dlights"; }

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

}}  // namespace ::task


// Compo Envmap
namespace {

    class RpStatesCompoEnvmap
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<1> {

    public:
        RpStatesCompoEnvmap(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
            namespace cpnt = mirinae::cpnt;
            auto& reg = cosmos.reg();
            auto& desclays = rp_res_.desclays_;

            // Desc layout: main
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
                builder
                    .add_img_frag(1)   // depth
                    .add_img_frag(1)   // albedo
                    .add_img_frag(1)   // normal
                    .add_img_frag(1);  // material
                desclays.add(builder, device.logi_device());
            }

            // Desc layout: envmaps
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":envmaps" };
                builder
                    .add_img_frag(1)   // u_env_diffuse
                    .add_img_frag(1)   // u_env_specular
                    .add_img_frag(1);  // u_env_lut
                desclays.add(builder, device.logi_device());
            }

            // Desc sets: main
            this->recreate_desc_sets(frame_data_, desc_pool_main_, device);

            // Desc sets: envmaps
            {
                auto& desc_layout = desclays.get(name_s() + ":envmaps");

                desc_pool_env_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.size_info(),
                    device.logi_device()
                );

                auto desc_sets = desc_pool_env_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_env_ = desc_sets[i];

                    // Diffuse envmap
                    writer.add_img_info()
                        .set_img_view(rp_res.envmaps_->diffuse_at(0))
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_env_, 0);

                    // Specular envmap
                    writer.add_img_info()
                        .set_img_view(rp_res.envmaps_->specular_at(0))
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_env_, 1);

                    // LUT
                    writer.add_img_info()
                        .set_img_view(rp_res.envmaps_->brdf_lut())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_env_, 2);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclays.get(name_s() + ":main").layout())
                    .desc(desclays.get(name_s() + ":envmaps").layout())
                    .add_frag_flag()
                    .pc<U_CompoEnvmapPushConst>()
                    .build(pipe_layout_, device);
            }

            // Render pass
            this->recreate_render_pass(render_pass_, device);

            // Pipeline
            this->recreate_pipeline(pipeline_, device);

            // Framebuffers
            this->recreate_fbufs(frame_data_, device);

            // Misc
            {
                clear_values_.at(0).color = { 0, 0, 0, 1 };
            }

            return;
        }

        ~RpStatesCompoEnvmap() override {
            for (auto& fd : frame_data_) {
                fd.fbuf_.destroy(device_.logi_device());
            }

            desc_pool_main_.destroy(device_.logi_device());
            desc_pool_env_.destroy(device_.logi_device());
            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "compo_envmap"; }

        void on_resize(uint32_t width, uint32_t height) override {
            this->recreate_desc_sets(frame_data_, desc_pool_main_, device_);
            this->recreate_render_pass(render_pass_, device_);
            this->recreate_pipeline(pipeline_, device_);
            this->recreate_fbufs(frame_data_, device_);
        }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<task::RpTask>();
            out->init(
                frame_data_,
                cosmos_.reg(),
                rp_res_.gbuf_,
                *this,
                rp_res_.cmd_pool_,
                device_
            );
            return out;
        }

    private:
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
                auto& fd = fdata[i];
                fd.desc_set_main_ = desc_sets[i];

                // Depth
                writer.add_img_info()
                    .set_img_view(gbufs.depth(i).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_main_, 0);
                // Albedo
                writer.add_img_info()
                    .set_img_view(gbufs.albedo(i).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_main_, 1);
                // Normal
                writer.add_img_info()
                    .set_img_view(gbufs.normal(i).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_main_, 2);
                // Material
                writer.add_img_info()
                    .set_img_view(gbufs.material(i).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_main_, 3);
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
                .add_vert(":asset/spv/compo_envmap_vert.spv")
                .add_frag(":asset/spv/compo_envmap_frag.spv");

            builder.rasterization_state().cull_mode_back();

            builder.color_blend_state().add().set_additive_blend();

            builder.dynamic_state().add_viewport().add_scissor();

            pipeline.reset(builder.build(render_pass_, pipe_layout_), device);
        }

        void recreate_fbufs(
            ::FrameDataArr& fdata, mirinae::VulkanDevice& device
        ) const {
            for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(render_pass_)
                    .set_dim(rp_res_.gbuf_.extent())
                    .add_attach(rp_res_.gbuf_.compo(i).image_view());
                fdata.at(i).fbuf_.reset(
                    fbuf_cinfo.build(device), device.logi_device()
                );
            }
        }

        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        FrameDataArr frame_data_;
        mirinae::DescPool desc_pool_main_, desc_pool_env_;
    };

}  // namespace


namespace mirinae::rp::compo {

    std::unique_ptr<IRpBase> create_rps_envmap(RpCreateBundle& cbundle) {
        return std::make_unique<RpStatesCompoEnvmap>(
            cbundle.cosmos_, cbundle.rp_res_, cbundle.device_
        );
    }

}  // namespace mirinae::rp::compo
