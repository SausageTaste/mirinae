#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/transp/transp.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/draw_set.hpp"
#include "mirinae/renderpass/builder.hpp"


namespace {

    class U_TranspFrame {

    public:
        template <typename T>
        U_TranspFrame& set_proj(const glm::tmat4x4<T>& m) {
            proj_ = m;
            proj_inv_ = glm::inverse(m);
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_view(const glm::tmat4x4<T>& m) {
            view_ = m;
            view_inv_ = glm::inverse(m);
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_dlight_mat(size_t index, const glm::tmat4x4<T>& m) {
            dlight_mats_[index] = m;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_dlight_dir(const glm::tvec3<T>& x) {
            dlight_dir_.x = x.x;
            dlight_dir_.y = x.y;
            dlight_dir_.z = x.z;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_dlight_color(T r, T g, T b) {
            dlight_color_.x = r;
            dlight_color_.y = g;
            dlight_color_.z = b;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_dlight_color(const glm::tvec3<T>& dlight_color) {
            dlight_color_.x = dlight_color.r;
            dlight_color_.y = dlight_color.g;
            dlight_color_.z = dlight_color.b;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_dlight_cascade_depths(
            const std::array<T, 4>& depths
        ) {
            for (size_t i = 0; i < depths.size(); ++i)
                dlight_cascade_depths_[i] = depths[i];

            return *this;
        }

        template <typename T>
        U_TranspFrame& set_slight_mat(const glm::tmat4x4<T>& m) {
            slight_mat_ = m;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_slight_pos(const glm::tvec3<T>& pos) {
            slight_pos_n_inner_angle.x = pos.x;
            slight_pos_n_inner_angle.y = pos.y;
            slight_pos_n_inner_angle.z = pos.z;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_slight_dir(const glm::tvec3<T>& x) {
            slight_dir_n_outer_angle.x = x.x;
            slight_dir_n_outer_angle.y = x.y;
            slight_dir_n_outer_angle.z = x.z;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_slight_color(const glm::tvec3<T>& v) {
            slight_color_n_max_dist.x = v.r;
            slight_color_n_max_dist.y = v.g;
            slight_color_n_max_dist.z = v.b;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_slight_inner_angle(sung::TAngle<T> angle) {
            const auto v = std::cos(angle.rad());
            slight_pos_n_inner_angle.w = static_cast<float>(v);
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_slight_outer_angle(sung::TAngle<T> angle) {
            const auto v = std::cos(angle.rad());
            slight_dir_n_outer_angle.w = static_cast<float>(v);
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_slight_max_dist(T max_dist) {
            slight_color_n_max_dist.w = max_dist;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_vpl_pos(size_t i, const glm::tvec3<T>& pos) {
            vpl_pos_n_radius[i].x = pos.x;
            vpl_pos_n_radius[i].y = pos.y;
            vpl_pos_n_radius[i].z = pos.z;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_vpl_color(size_t i, const glm::tvec3<T>& v) {
            vpl_color_n_intensity[i].x = v.r;
            vpl_color_n_intensity[i].y = v.g;
            vpl_color_n_intensity[i].z = v.b;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_fog_color(const glm::tvec3<T>& v) {
            fog_color_density_.x = v.r;
            fog_color_density_.y = v.g;
            fog_color_density_.z = v.b;
            return *this;
        }

        template <typename T>
        U_TranspFrame& set_fog_density(T density) {
            fog_color_density_.w = density;
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

        // Spotlight
        glm::mat4 slight_mat_;
        glm::vec4 slight_pos_n_inner_angle;
        glm::vec4 slight_dir_n_outer_angle;
        glm::vec4 slight_color_n_max_dist;

        // Volumetric Point Light
        std::array<glm::vec4, 8> vpl_pos_n_radius;
        std::array<glm::vec4, 8> vpl_color_n_intensity;

        glm::vec4 fog_color_density_;
    };


    struct FrameData {
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

            mirinae::begin_cmdbuf(cmdbuf_);
            this->update_ubuf(*reg_, *ctxt_, fd, *device_);
            this->record(cmdbuf_, fd, draw_set_, *rp_, *ctxt_, fbuf_ext);
            mirinae::end_cmdbuf(cmdbuf_);
        }

        static void update_ubuf(
            const entt::registry& reg,
            const mirinae::RpCtxt& ctxt,
            ::FrameData& fd,
            mirinae::VulkanDevice& device
        ) {
            namespace cpnt = mirinae::cpnt;

            auto& proj_mat = ctxt.main_cam_.proj();
            auto& view_mat = ctxt.main_cam_.view();

            ::U_TranspFrame ubuf_data;
            ubuf_data.set_proj(proj_mat).set_view(view_mat);

            for (auto e : reg.view<cpnt::DLight, cpnt::Transform>()) {
                const auto& l = reg.get<cpnt::DLight>(e);
                const auto& t = reg.get<cpnt::Transform>(e);
                const auto& cascade = l.cascades_;
                const auto& cascades = cascade.cascades_;

                for (size_t i = 0; i < cascades.size(); ++i)
                    ubuf_data.set_dlight_mat(i, cascades.at(i).light_mat_);

                ubuf_data.set_dlight_dir(l.calc_to_light_dir(view_mat, t))
                    .set_dlight_color(l.color_.scaled_color())
                    .set_dlight_cascade_depths(cascade.far_depths_);
                break;
            }

            for (auto e : reg.view<cpnt::SLight, cpnt::Transform>()) {
                const auto& l = reg.get<cpnt::SLight>(e);
                const auto& t = reg.get<cpnt::Transform>(e);
                ubuf_data.set_slight_mat(l.make_light_mat(t))
                    .set_slight_pos(l.calc_view_space_pos(view_mat, t))
                    .set_slight_dir(l.calc_to_light_dir(view_mat, t))
                    .set_slight_color(l.color_.scaled_color())
                    .set_slight_inner_angle(l.inner_angle_)
                    .set_slight_outer_angle(l.outer_angle_)
                    .set_slight_max_dist(l.max_distance_);
                break;
            }

            size_t i = 0;
            for (auto e : reg.view<cpnt::VPLight, cpnt::Transform>()) {
                if (i >= 8)
                    break;

                const auto& l = reg.get<cpnt::VPLight>(e);
                const auto& t = reg.get<cpnt::Transform>(e);
                ubuf_data.set_vpl_color(i, l.color_.scaled_color())
                    .set_vpl_pos(i, t.pos_);

                ++i;
            }

            for (auto e : reg.view<cpnt::AtmosphereSimple>()) {
                const auto& atm = reg.get<cpnt::AtmosphereSimple>(e);
                ubuf_data.set_fog_color(atm.fog_color_)
                    .set_fog_density(atm.fog_density_);
                break;
            }

            fd.ubuf_.set_data(ubuf_data, device.mem_alloc());
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

        mirinae::FenceTask fence_;
        mirinae::DrawSetStatic draw_set_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const entt::registry* reg_ = nullptr;
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
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            ::FrameDataArr& frame_data,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(reg, gbufs, rp, frame_data, cmd_pool, device);
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

            // Desc layout
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":frame" };
                builder
                    .add_ubuf(VK_SHADER_STAGE_FRAGMENT_BIT, 1)  // U_TranspFrame
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // dlight
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // slight
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env diffuse
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1)   // env specular
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);  // env lut
                desclays.add(builder, device.logi_device());
            }

            // Ubuf
            for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                auto& fd = frame_data_.at(i);
                fd.ubuf_.init_ubuf<::U_TranspFrame>(device.mem_alloc());
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

                const auto sam_nea = device.samplers().get_nearest();
                const auto sam_lin = device.samplers().get_linear();
                const auto sam_cube = device.samplers().get_cubemap();
                auto& shadows = *rp_res.shadow_maps_;
                auto& dlights = shadows.dlights();
                auto& envmaps = *rp_res.envmaps_;

                mirinae::DescWriteInfoBuilder builder;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    const mirinae::FrameIndex f_idx(i);
                    auto& fd = frame_data_.at(i);
                    fd.desc_ = desc_sets[i];

                    builder.set_descset(fd.desc_)
                        .add_ubuf(fd.ubuf_)
                        .add_img_sampler(dlights.at(0).view(f_idx), sam_nea)
                        .add_img_sampler(shadows.slight_view_at(0), sam_nea)
                        .add_img_sampler(envmaps.diffuse_at(0), sam_cube)
                        .add_img_sampler(envmaps.specular_at(0), sam_cube)
                        .add_img_sampler(envmaps.brdf_lut(), sam_lin);
                }
                builder.apply_all(device.logi_device());
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
                    .add_vert(":asset/spv/transp_vert.spv")
                    .add_frag(":asset/spv/transp_frag.spv");

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
