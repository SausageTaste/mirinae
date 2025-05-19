#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/compo.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"


namespace {

    class U_CompoSkyAtmosMain {

    public:
        U_CompoSkyAtmosMain& set_proj_inv(const glm::mat4& m) {
            proj_inv_ = m;
            return *this;
        }

        U_CompoSkyAtmosMain& set_view_inv(const glm::mat4& m) {
            view_inv_ = m;
            return *this;
        }

        U_CompoSkyAtmosMain& set_view_pos_w(const glm::vec3& v) {
            view_pos_w_.x = v.x;
            view_pos_w_.y = v.y;
            view_pos_w_.z = v.z;
            return *this;
        }

        U_CompoSkyAtmosMain& set_sun_dir_w(const glm::vec3& v) {
            sun_direction_w_.x = v.x;
            sun_direction_w_.y = v.y;
            sun_direction_w_.z = v.z;
            return *this;
        }

    private:
        glm::mat4 proj_inv_;
        glm::mat4 view_inv_;
        glm::vec4 view_pos_w_;
        glm::vec4 sun_direction_w_;
    };


    struct FrameData {
        mirinae::HRpImage trans_lut_;
        mirinae::HRpImage multi_scat_;
        mirinae::HRpImage sky_view_lut_;
        mirinae::HRpImage cam_scat_vol_;
        mirinae::Fbuf fbuf_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
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

            mirinae::begin_cmdbuf(cmdbuf_);
            this->record_barriers(cmdbuf_, *gbufs_, *ctxt_);
            this->record(cmdbuf_, fd, *reg_, *rp_, *ctxt_, gbuf_ext);
            mirinae::end_cmdbuf(cmdbuf_);
        }

        static void record_barriers(
            const VkCommandBuffer cmdbuf,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::RpCtxt& ctxt
        ) {
            mirinae::ImageMemoryBarrier{}
                .image(gbufs.depth(ctxt.f_index_.get()).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .old_lay(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .new_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .set_src_acc(VK_ACCESS_SHADER_READ_BIT)
                .set_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
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

            ::U_CompoSkyAtmosMain pc;
            pc.set_proj_inv(ctxt.main_cam_.proj_inv())
                .set_view_inv(ctxt.main_cam_.view_inv())
                .set_view_pos_w(ctxt.main_cam_.view_pos());
            for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                break;
            }
            for (auto e : reg.view<mirinae::cpnt::DLight>()) {
                auto& light = reg.get<mirinae::cpnt::DLight>(e);
                auto& tform = reg.get<mirinae::cpnt::Transform>(e);
                const auto dir = light.calc_to_light_dir(glm::dmat4(1), tform);
                pc.set_sun_dir_w(-dir);
            }

            mirinae::PushConstInfo{}
                .layout(rp.pipe_layout())
                .add_stage_frag()
                .record(cmdbuf, pc);

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmdbuf);
        }

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


// Compo Sky
namespace {

    class RpStates
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<2> {

    public:
        RpStates(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
            namespace cpnt = mirinae::cpnt;
            auto& reg = cosmos.reg();
            auto& desclays = rp_res_.desclays_;

            // Image references
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_[i];
                fd.trans_lut_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("atmos trans LUT:trans_lut_f#{}", i), name_s()
                );
                fd.multi_scat_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("multi scattering CS:multi_scat_f#{}", i),
                    name_s()
                );
                fd.sky_view_lut_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("sky view LUT:sky_view_lut_f#{}", i), name_s()
                );
                fd.cam_scat_vol_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("atmos cam volume:cam_vol_f#{}", i), name_s()
                );
            }

            // Descriptor layout
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
                builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
                desclays.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                auto& desc_layout = desclays.get(name_s() + ":main");

                desc_pool_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.size_info(),
                    device.logi_device()
                );

                auto desc_sets = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_ = desc_sets[i];

                    writer.add_img_info()
                        .set_img_view(fd.trans_lut_->view_.get())
                        .set_sampler(device.samplers().get_cubemap())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 0);
                    writer.add_img_info()
                        .set_img_view(fd.multi_scat_->view_.get())
                        .set_sampler(device.samplers().get_cubemap())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 1);
                    writer.add_img_info()
                        .set_img_view(fd.sky_view_lut_->view_.get())
                        .set_sampler(device.samplers().get_cubemap())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 2);
                    writer.add_img_info()
                        .set_img_view(fd.cam_scat_vol_->view_.get())
                        .set_sampler(device.samplers().get_cubemap())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 3);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline layout
            {
                auto& desc_layout = desclays.get(name_s() + ":main");

                mirinae::PipelineLayoutBuilder{}
                    .desc(desc_layout.layout())
                    .add_frag_flag()
                    .pc<::U_CompoSkyAtmosMain>()
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
                clear_values_.at(0).depthStencil = { 0, 0 };
                clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            }

            return;
        }

        ~RpStates() override {
            for (auto& fd : frame_data_) {
                rp_res_.ren_img_.free_img(fd.trans_lut_->id(), name_s());
                rp_res_.ren_img_.free_img(fd.multi_scat_->id(), name_s());
                rp_res_.ren_img_.free_img(fd.sky_view_lut_->id(), name_s());
                rp_res_.ren_img_.free_img(fd.cam_scat_vol_->id(), name_s());
                fd.fbuf_.destroy(device_.logi_device());
            }

            desc_pool_.destroy(device_.logi_device());
            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "compo_sky"; }

        void on_resize(uint32_t width, uint32_t height) override {
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
        struct FrameData {
            VkDescriptorSet desc_set_;
            VkFramebuffer fbuf_;
        };

        static const mirinae::cpnt::AtmosphereSimple* select_atmos_simple(
            const entt::registry& reg
        ) {
            using Atmos = mirinae::cpnt::AtmosphereSimple;

            for (auto entity : reg.view<Atmos>()) {
                return &reg.get<Atmos>(entity);
            }

            return nullptr;
        }

        void recreate_render_pass(
            mirinae::RenderPass& render_pass, mirinae::VulkanDevice& device
        ) const {
            mirinae::RenderPassBuilder builder;

            builder.attach_desc()
                .add(rp_res_.gbuf_.depth_format())
                .ini_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .fin_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
                .stor_op(VK_ATTACHMENT_STORE_OP_STORE);
            builder.attach_desc()
                .add(rp_res_.gbuf_.compo_format())
                .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .op_pair_load_store();

            builder.color_attach_ref().add_color_attach(1);

            builder.depth_attach_ref().set(0);

            builder.subpass_dep().add().preset_single();

            render_pass.reset(builder.build(device.logi_device()), device);
        }

        void recreate_pipeline(
            mirinae::RpPipeline& pipeline, mirinae::VulkanDevice& device
        ) const {
            mirinae::PipelineBuilder builder{ device };

            builder.shader_stages()
                .add_vert(":asset/spv/compo_sky_vert.spv")
                .add_frag(":asset/spv/compo_sky_atmos_frag.spv");

            builder.depth_stencil_state()
                .depth_test_enable(true)
                .depth_write_enable(false)
                .depth_compare_op(VK_COMPARE_OP_GREATER_OR_EQUAL);

            builder.color_blend_state().add(false, 1);

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
                    .add_attach(rp_res_.gbuf_.depth(i).image_view())
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
        mirinae::DescPool desc_pool_;
    };

}  // namespace


namespace mirinae::rp::compo {

    std::unique_ptr<IRpBase> create_rps_sky_atmos(RpCreateBundle& cbundle) {
        return std::make_unique<RpStates>(
            cbundle.cosmos_, cbundle.rp_res_, cbundle.device_
        );
    }

}  // namespace mirinae::rp::compo
