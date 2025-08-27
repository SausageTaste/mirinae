#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/transp/transp.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/draw_set.hpp"
#include "mirinae/render/mem_cinfo.hpp"
#include "mirinae/renderpass/builder.hpp"


namespace {

    class U_TranspFrame {

    public:
        U_TranspFrame& set_proj(const glm::dmat4& v) {
            proj_ = v;
            proj_inv_ = glm::inverse(v);
            return *this;
        }

        U_TranspFrame& set_view(const glm::dmat4& v) {
            view_ = v;
            view_inv_ = glm::inverse(v);
            return *this;
        }

        // Dlights

        U_TranspFrame& set_dlight_mat(size_t idx, const glm::mat4& m) {
            dlight_mats_[idx] = m;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_dlight_cascade_depths(const T* arr) {
            dlight_cascade_depths_.x = static_cast<float>(arr[0]);
            dlight_cascade_depths_.y = static_cast<float>(arr[1]);
            dlight_cascade_depths_.z = static_cast<float>(arr[2]);
            dlight_cascade_depths_.w = static_cast<float>(arr[3]);
            return *this;
        }

        U_TranspFrame& set_dlight_dir(const glm::dvec3& v) {
            dlight_dir_.x = static_cast<float>(v.x);
            dlight_dir_.y = static_cast<float>(v.y);
            dlight_dir_.z = static_cast<float>(v.z);
            return *this;
        }

        U_TranspFrame& set_dlight_color(const glm::vec3& v) {
            dlight_color_.x = v.r;
            dlight_color_.y = v.g;
            dlight_color_.z = v.b;
            return *this;
        }

        // Misc

        template <typename T>
        U_TranspFrame& set_mie_anisotropy(T v) {
            mie_anisotropy_ = static_cast<float>(v);
            return *this;
        }

    private:
        glm::mat4 view_;
        glm::mat4 view_inv_;
        glm::mat4 proj_;
        glm::mat4 proj_inv_;

        // Directional light
        glm::mat4 dlight_mats_[4];
        glm::vec4 dlight_dir_;
        glm::vec4 dlight_color_;
        glm::vec4 dlight_cascade_depths_;

        float mie_anisotropy_;
    };


    struct FrameData {
        mirinae::HRpImage trans_lut_;
        mirinae::HRpImage cam_scat_vol_;
        mirinae::Fbuf fbuf_;
        mirinae::Buffer ubuf_;
        VkDescriptorSet desc_ = VK_NULL_HANDLE;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;

}  // namespace


namespace {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            const mirinae::IShadowMapBundle& shadows,
            ::FrameDataArr& frame_data,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            cmd_pool_ = &cmd_pool;
            device_ = &device;
            frame_data_ = &frame_data;
            gbufs_ = &gbufs;
            reg_ = &reg;
            rp_ = &rp;
            shadows_ = &shadows;
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

            auto& fd = frame_data_->at(ctxt_->f_index_.get());
            const auto fbuf_ext = gbufs_->extent();

            draw_set_.clear();
            draw_set_.fetch(*reg_);

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            this->update_ubuf(*reg_, *shadows_, *ctxt_, fd, *device_);
            this->record_barriers(cmdbuf_, *gbufs_, *ctxt_);
            this->record(cmdbuf_, fd, draw_set_, *rp_, *ctxt_, fbuf_ext);
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void update_ubuf(
            const entt::registry& reg,
            const mirinae::IShadowMapBundle& shadows,
            const mirinae::RpCtxt& ctxt,
            ::FrameData& fd,
            mirinae::VulkanDevice& device
        ) {
            namespace cpnt = mirinae::cpnt;

            auto& view_mat = ctxt.main_cam_.view();
            auto& view_inv = ctxt.main_cam_.view_inv();

            U_TranspFrame ubuf;
            ubuf.set_proj(ctxt.main_cam_.proj())
                .set_view(ctxt.main_cam_.view());

            for (auto e : reg.view<cpnt::AtmosphereSimple>()) {
                auto& atmos = reg.get<cpnt::AtmosphereSimple>(e);
                ubuf.set_mie_anisotropy(atmos.mie_anisotropy_);
                break;
            }

            for (uint32_t i = 0; i < shadows.dlights().count(); ++i) {
                auto& dlight = shadows.dlights().at(i);
                if (entt::null == dlight.entt())
                    continue;

                auto dlit = reg.try_get<cpnt::DLight>(dlight.entt());
                if (!dlit)
                    continue;

                const auto& light = reg.get<cpnt::DLight>(dlight.entt());
                const auto& tform = reg.get<cpnt::Transform>(dlight.entt());
                const auto& cascades = light.cascades_;
                const auto& casc_arr = cascades.cascades_;

                ubuf.set_dlight_mat(0, casc_arr[0].light_mat_ * view_inv)
                    .set_dlight_mat(1, casc_arr[1].light_mat_ * view_inv)
                    .set_dlight_mat(2, casc_arr[2].light_mat_ * view_inv)
                    .set_dlight_mat(3, casc_arr[3].light_mat_ * view_inv)
                    .set_dlight_cascade_depths(cascades.far_depths_.data())
                    .set_dlight_dir(light.calc_to_light_dir(view_mat, tform))
                    .set_dlight_color(light.color_.scaled_color());
                break;
            }

            fd.ubuf_.set_data(ubuf, device.mem_alloc());
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
            const mirinae::DrawSetStatic& draw_set,
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

            mirinae::DescSetBindInfo descset_info{ rp.pipe_layout() };
            descset_info.first_set(0).set(fd.desc_).record(cmdbuf);

            for (auto& pair : draw_set.trs()) {
                auto& unit = *pair.unit_;
                auto& actor = *pair.actor_;

                unit.record_bind_vert_buf(cmdbuf);

                descset_info.first_set(1)
                    .set(unit.get_desc_set(ctxt.f_index_.get()))
                    .record(cmdbuf);

                descset_info.first_set(2)
                    .set(actor.get_desc_set(ctxt.f_index_.get()))
                    .record(cmdbuf);

                vkCmdDrawIndexed(cmdbuf, unit.vertex_count(), 1, 0, 0, 0);
            }

            vkCmdEndRenderPass(cmdbuf);
        }

        const mirinae::DebugLabel DEBUG_LABEL{
            "Trans Static", 0.7, 0.62, 0.86
        };

        mirinae::FenceTask fence_;
        mirinae::DrawSetStatic draw_set_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const entt::registry* reg_ = nullptr;
        const mirinae::FbufImageBundle* gbufs_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::IShadowMapBundle* shadows_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        ::FrameDataArr* frame_data_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        void init(
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            const mirinae::IShadowMapBundle& shadows,
            ::FrameDataArr& frame_data,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(
                reg, gbufs, rp, shadows, frame_data, cmd_pool, device
            );
        }

        std::string_view name() const override { return "transp skinned"; }

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

    class RpMasterTranspStatic
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<2> {

    public:
        RpMasterTranspStatic(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : cosmos_(cosmos), rp_res_(rp_res), device_(device) {
            auto& desclays = rp_res_.desclays_;

            // Image references
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_[i];
                fd.trans_lut_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("atmos trans LUT:trans_lut_f#{}", i), name_s()
                );
                fd.cam_scat_vol_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("atmos cam volume:cam_vol_f#{}", i), name_s()
                );
            }

            // Desc layout
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":frame" };
                builder
                    .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_TranspFrame
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // dlight
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // slight
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env diffuse
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env specular
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env lut
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // trans lut
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // cam scat vol
                desclays.add(builder, device.logi_device());
            }

            // Ubuf
            for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                mirinae::BufferCreateInfo buf_cinfo;
                buf_cinfo.preset_ubuf(sizeof(::U_TranspFrame));

                auto& fd = frame_data_.at(i);
                fd.ubuf_.init(buf_cinfo, device.mem_alloc());
            }

            // Desc sets
            {
                auto& desclayout = rp_res.desclays_.get(name_s() + ":frame");

                desc_pool_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desclayout.size_info(),
                    device.logi_device()
                );

                const auto desc_sets = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desclayout.layout(),
                    device.logi_device()
                );

                auto& shadow = *rp_res.shadow_maps_;
                auto& dlights = shadow.dlights();
                const auto slight_count = shadow.slight_count();

                mirinae::DescWriter w;
                for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    const mirinae::FrameIndex f_idx(i);
                    auto& fd = frame_data_[i];
                    fd.desc_ = desc_sets[i];

                    // Ubuf
                    w.add_buf_info(fd.ubuf_).add_buf_write(fd.desc_, 0);
                    // Dlight shadow maps
                    for (uint32_t i_dl = 0; i_dl < dlights.count(); ++i_dl) {
                        constexpr auto LAYOUT =
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        w.add_img_info()
                            .set_img_view(dlights.at(i_dl).view(f_idx))
                            .set_layout(LAYOUT)
                            .set_sampler(device.samplers().get_shadow());
                        break;
                    }
                    w.add_sampled_img_write(fd.desc_, 1);
                    // Slight shadow maps
                    for (uint32_t i_sl = 0; i_sl < slight_count; ++i_sl) {
                        constexpr auto LAYOUT =
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        const auto view = shadow.slight_view_at(i_sl);
                        w.add_img_info()
                            .set_img_view(view)
                            .set_layout(LAYOUT)
                            .set_sampler(device.samplers().get_shadow());
                        break;
                    }
                    w.add_sampled_img_write(fd.desc_, 2);
                    // Env diffuse
                    w.add_img_info()
                        .set_img_view(rp_res.envmaps_->diffuse_at(0))
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        .set_sampler(device.samplers().get_cubemap());
                    w.add_sampled_img_write(fd.desc_, 3);
                    // Env specular
                    w.add_img_info()
                        .set_img_view(rp_res.envmaps_->specular_at(0))
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        .set_sampler(device.samplers().get_cubemap());
                    w.add_sampled_img_write(fd.desc_, 4);
                    // Env lut
                    w.add_img_info()
                        .set_img_view(rp_res.envmaps_->brdf_lut())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        .set_sampler(device.samplers().get_linear());
                    w.add_sampled_img_write(fd.desc_, 5);
                    // Transmittance LUT
                    w.add_img_info()
                        .set_img_view(fd.trans_lut_->view_.get())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        .set_sampler(device.samplers().get_cubemap());
                    w.add_sampled_img_write(fd.desc_, 6);
                    // Camera scattering volume
                    w.add_img_info()
                        .set_img_view(fd.cam_scat_vol_->view_.get())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        .set_sampler(device.samplers().get_linear());
                    w.add_sampled_img_write(fd.desc_, 7);
                }
                w.apply_all(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclays.get(name_s() + ":frame").layout())
                    .desc(desclays.get("gbuf:model").layout())
                    .desc(desclays.get("gbuf:actor").layout())
                    .build(pipe_layout_, device_);
            }

            // Render pass, pipeline, frame buffers
            this->on_resize(rp_res_.gbuf_.width(), rp_res_.gbuf_.height());

            // Misc
            {
                clear_values_[0].depthStencil = { 0, 0 };
                clear_values_[1].color = { 0, 0, 0, 1 };
            }
        }

        ~RpMasterTranspStatic() {
            for (auto& fd : frame_data_) {
                fd.fbuf_.destroy(device_.logi_device());
                fd.ubuf_.destroy(device_.mem_alloc());
            }

            desc_pool_.destroy(device_.logi_device());
            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "transp static"; }

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
                    .add_vert(":asset/spv/transp_static_vert.spv")
                    .add_frag(":asset/spv/transp_static_frag.spv");

                builder.vertex_input_state().set_static();

                builder.depth_stencil_state()
                    .depth_test_enable(true)
                    .depth_write_enable(false);

                builder.color_blend_state().add(true, 1);

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_.reset(
                    builder.build(render_pass_, pipe_layout_), device_
                );
            }

            // Frame buffers
            {
                mirinae::FbufCinfo cinfo;
                cinfo.set_rp(render_pass_)
                    .set_dim(rp_res_.gbuf_.extent())
                    .set_layers(1);

                for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    auto& fd = frame_data_[i];

                    cinfo.clear_attach()
                        .add_attach(gbufs.depth(i).image_view())
                        .add_attach(gbufs.compo(i).image_view());
                    fd.fbuf_.init(cinfo.get(), device_.logi_device());
                }
            }
        }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto task = std::make_unique<RpTask>();
            task->init(
                cosmos_.reg(),
                rp_res_.gbuf_,
                *this,
                *rp_res_.shadow_maps_,
                frame_data_,
                rp_res_.cmd_pool_,
                device_
            );
            return task;
        }

    private:
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;
        mirinae::VulkanDevice& device_;

        FrameDataArr frame_data_;
        mirinae::DescPool desc_pool_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_states_transp_static(
        RpCreateBundle& cbundle
    ) {
        return std::make_unique<::RpMasterTranspStatic>(
            cbundle.cosmos_, cbundle.rp_res_, cbundle.device_
        );
    }

}  // namespace mirinae::rp
