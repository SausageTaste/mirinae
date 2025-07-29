#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/compo.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/draw_set.hpp"
#include "mirinae/renderpass/builder.hpp"


namespace {

    class U_CompoAtmosSurfMain {

    public:
        U_CompoAtmosSurfMain& set_proj(const glm::dmat4& v) {
            proj_ = v;
            proj_inv_ = glm::inverse(v);
            return *this;
        }

        U_CompoAtmosSurfMain& set_view(const glm::dmat4& v) {
            view_ = v;
            view_inv_ = glm::inverse(v);
            return *this;
        }

        template <typename T>
        U_CompoAtmosSurfMain& set_view_pos_w(const glm::tvec3<T>& v) {
            view_pos_w_.x = static_cast<float>(v.x);
            view_pos_w_.y = static_cast<float>(v.y);
            view_pos_w_.z = static_cast<float>(v.z);
            return *this;
        }

        template <typename T>
        U_CompoAtmosSurfMain& set_fog_color(const glm::tvec3<T>& v) {
            fog_color_density_.x = static_cast<float>(v.x);
            fog_color_density_.y = static_cast<float>(v.y);
            fog_color_density_.z = static_cast<float>(v.z);
            return *this;
        }

        template <typename T>
        U_CompoAtmosSurfMain& set_fog_density(T v) {
            fog_color_density_.w = static_cast<float>(v);
            return *this;
        }

        template <typename T>
        U_CompoAtmosSurfMain& set_mie_anisotropy(T v) {
            mie_anisotropy_ = static_cast<float>(v);
            return *this;
        }

    private:
        glm::mat4 proj_;
        glm::mat4 proj_inv_;
        glm::mat4 view_;
        glm::mat4 view_inv_;
        glm::vec4 view_pos_w_;
        glm::vec4 fog_color_density_;
        float mie_anisotropy_ = 0.5f;
    };


    struct U_CompoDlightShadowMap {

    public:
        U_CompoDlightShadowMap& set_light_mat(size_t idx, const glm::mat4& m) {
            light_mats_[idx] = m;
            return *this;
        }

        template <typename T>
        U_CompoDlightShadowMap& set_cascade_depths(const T* arr) {
            cascade_depths_.x = static_cast<float>(arr[0]);
            cascade_depths_.y = static_cast<float>(arr[1]);
            cascade_depths_.z = static_cast<float>(arr[2]);
            cascade_depths_.w = static_cast<float>(arr[3]);
            return *this;
        }

        U_CompoDlightShadowMap& set_dlight_dir(const glm::dvec3& v) {
            dlight_dir_.x = static_cast<float>(v.x);
            dlight_dir_.y = static_cast<float>(v.y);
            dlight_dir_.z = static_cast<float>(v.z);
            return *this;
        }

        U_CompoDlightShadowMap& set_dlight_color(const glm::vec3& v) {
            dlight_color_.x = v.r;
            dlight_color_.y = v.g;
            dlight_color_.z = v.b;
            return *this;
        }

    private:
        glm::mat4 light_mats_[4];
        glm::vec4 cascade_depths_;
        glm::vec4 dlight_color_;
        glm::vec4 dlight_dir_;
    };


    struct ShadowMapData {
        mirinae::Buffer ubuf_;
        VkDescriptorSet desc_set_;
    };


    struct FrameData {
        std::vector<ShadowMapData> shadows_;
        mirinae::HRpImage trans_lut_;
        mirinae::HRpImage multi_scat_;
        mirinae::HRpImage sky_view_lut_;
        mirinae::HRpImage cam_scat_vol_;
        mirinae::Buffer ubuf_;
        mirinae::Fbuf fbuf_;
        VkDescriptorSet desc_set_;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;
    using Dlights = mirinae::IShadowMapBundle::IDlightShadowMapBundle;

}  // namespace


// Tasks
namespace { namespace task {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const ::Dlights& dlights,
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            ::FrameDataArr& fdata,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            cmd_pool_ = &cmd_pool;
            device_ = &device;
            dlights_ = &dlights;
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
            this->update_ubuf(*reg_, *ctxt_, fd, *device_);
            this->update_ubuf_shadow(*dlights_, *reg_, *ctxt_, fd, *device_);
            this->record_barriers(cmdbuf_, *gbufs_, *ctxt_);
            this->record_barriers_shadow(cmdbuf_, *dlights_, *ctxt_);
            this->record(cmdbuf_, *dlights_, fd, *rp_, *ctxt_, gbuf_ext);
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void update_ubuf(
            const entt::registry& reg,
            const mirinae::RpCtxt& ctxt,
            ::FrameData& fd,
            mirinae::VulkanDevice& device
        ) {
            U_CompoAtmosSurfMain ubuf;
            ubuf.set_proj(ctxt.main_cam_.proj())
                .set_view(ctxt.main_cam_.view())
                .set_view_pos_w(ctxt.main_cam_.view_pos());
            for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                ubuf.set_fog_color(atmos.fog_color_)
                    .set_fog_density(atmos.fog_density_)
                    .set_mie_anisotropy(atmos.mie_anisotropy_);
                break;
            }
            fd.ubuf_.set_data(ubuf, device.mem_alloc());
        }

        static void update_ubuf_shadow(
            const ::Dlights& dlights,
            const entt::registry& reg,
            const mirinae::RpCtxt& ctxt,
            ::FrameData& fd,
            mirinae::VulkanDevice& device
        ) {
            const auto& view_mat = ctxt.main_cam_.view();
            const auto& view_inv = ctxt.main_cam_.view_inv();

            for (uint32_t i = 0; i < dlights.count(); ++i) {
                auto& dlight = dlights.at(i);
                const auto e = dlight.entt();
                if (entt::null == e)
                    continue;

                auto& sh_data = fd.shadows_.at(i);
                const auto shadow_view = dlight.view(ctxt.f_index_);
                const auto& light = reg.get<mirinae::cpnt::DLight>(e);
                const auto& tform = reg.get<mirinae::cpnt::Transform>(e);
                const auto& cascades = light.cascades_;
                const auto& casc_arr = cascades.cascades_;

                U_CompoDlightShadowMap ubuf_sh;
                ubuf_sh.set_light_mat(0, casc_arr[0].light_mat_ * view_inv)
                    .set_light_mat(1, casc_arr[1].light_mat_ * view_inv)
                    .set_light_mat(2, casc_arr[2].light_mat_ * view_inv)
                    .set_light_mat(3, casc_arr[3].light_mat_ * view_inv)
                    .set_cascade_depths(cascades.far_depths_.data())
                    .set_dlight_dir(light.calc_to_light_dir(view_mat, tform))
                    .set_dlight_color(light.color_.scaled_color());
                sh_data.ubuf_.set_data(ubuf_sh, device.mem_alloc());
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
                .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_src_access(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );

            mirinae::ImageMemoryBarrier color_barrier{};
            color_barrier.set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_src_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .set_signle_mip_layer();

            color_barrier.image(gbufs.albedo(ctxt.f_index_.get()).image())
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );

            color_barrier.image(gbufs.normal(ctxt.f_index_.get()).image())
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );

            color_barrier.image(gbufs.material(ctxt.f_index_.get()).image())
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );
        }

        static void record_barriers_shadow(
            const VkCommandBuffer cmdbuf,
            const ::Dlights& dlights,
            const mirinae::RpCtxt& ctxt
        ) {
            for (uint32_t i = 0; i < dlights.count(); ++i) {
                auto& dlight = dlights.at(i);
                const auto e = dlight.entt();
                if (entt::null == e)
                    continue;

                mirinae::ImageMemoryBarrier{}
                    .image(dlight.img(ctxt.f_index_))
                    .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .old_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .new_lay(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .set_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_dst_acc(VK_ACCESS_SHADER_READ_BIT)
                    .set_signle_mip_layer()
                    .record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                    );
            }
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const ::Dlights& dlights,
            const ::FrameData& fd,
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

            for (uint32_t i = 0; i < dlights.count(); ++i) {
                auto& dlight = dlights.at(i);
                const auto e = dlight.entt();
                if (entt::null == e)
                    continue;

                mirinae::DescSetBindInfo{}
                    .layout(rp.pipe_layout())
                    .first_set(1)
                    .set(fd.shadows_.at(i).desc_set_)
                    .record(cmdbuf);

                vkCmdDraw(cmdbuf, 3, 1, 0, 0);
            }

            vkCmdEndRenderPass(cmdbuf);
        }

        const mirinae::DebugLabel DEBUG_LABEL{
            "Compo Atmos Surface", 1, 0.96, 0.61
        };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const ::Dlights* dlights_ = nullptr;
        const entt::registry* reg_ = nullptr;
        const mirinae::FbufImageBundle* gbufs_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        ::FrameDataArr* fdata_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        void init(
            const ::Dlights& dlights,
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            ::FrameDataArr& fdata,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(
                dlights, reg, gbufs, rp, fdata, cmd_pool, device
            );
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


// Compo Dlight
namespace {

    class RpStatesCompoDlight
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<1> {

    public:
        RpStatesCompoDlight(
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

            // Desc layout: main
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
                builder
                    .add_img_frag(1)    // depth
                    .add_img_frag(1)    // albedo
                    .add_img_frag(1)    // normal
                    .add_img_frag(1)    // material
                    .add_img_frag(1)    // trans LUT
                    .add_img_frag(1)    // multi scat
                    .add_img_frag(1)    // sky view LUT
                    .add_img_frag(1)    // cam scat vol
                    .add_ubuf_frag(1);  // U_CompoDlight
                desclays.add(builder, device.logi_device());
            }

            // Desc layout: shadow map
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":shadow_map" };
                builder
                    .add_img_frag(1)    // shadow map
                    .add_ubuf_frag(1);  // U_CompoDlightShadowMap
                desclays.add(builder, device.logi_device());
            }

            // Desc sets: main
            this->recreate_desc_sets(frame_data_, desc_pool_, device);

            // Desc sets: shadow map
            {
                constexpr auto FD_COUNT = mirinae::MAX_FRAMES_IN_FLIGHT;
                auto& dlights = rp_res.shadow_maps_->dlights();
                auto& desc_layout = desclays.get(name_s() + ":shadow_map");

                desc_pool_sh_.init(
                    dlights.count() * FD_COUNT,
                    desc_layout.size_info(),
                    device.logi_device()
                );

                auto desc_sets = desc_pool_sh_.alloc(
                    dlights.count() * FD_COUNT,
                    desc_layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (uint32_t i_fd = 0; i_fd < FD_COUNT; i_fd++) {
                    const mirinae::FrameIndex f_idx(i_fd);
                    auto& fd = frame_data_[i_fd];

                    for (uint32_t i_sh = 0; i_sh < dlights.count(); ++i_sh) {
                        auto& sh = fd.shadows_.emplace_back();
                        sh.desc_set_ = desc_sets.at(i_sh * FD_COUNT + i_fd);
                        sh.ubuf_.init_ubuf<U_CompoDlightShadowMap>(
                            device.mem_alloc()
                        );

                        // Shadow map
                        writer.add_img_info()
                            .set_img_view(dlights.at(i_sh).view(f_idx))
                            .set_sampler(device.samplers().get_shadow())
                            .set_layout(
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                            );
                        writer.add_sampled_img_write(sh.desc_set_, 0);
                        // U_CompoDlightShadowMap
                        writer.add_buf_info(sh.ubuf_);
                        writer.add_buf_write(sh.desc_set_, 1);
                    }
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclays.get(name_s() + ":main").layout())
                    .desc(desclays.get(name_s() + ":shadow_map").layout())
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

        ~RpStatesCompoDlight() override {
            for (auto& fd : frame_data_) {
                rp_res_.ren_img_.free_img(fd.trans_lut_->id(), name_s());
                rp_res_.ren_img_.free_img(fd.multi_scat_->id(), name_s());
                rp_res_.ren_img_.free_img(fd.sky_view_lut_->id(), name_s());
                rp_res_.ren_img_.free_img(fd.cam_scat_vol_->id(), name_s());
                fd.ubuf_.destroy(device_.mem_alloc());
                fd.fbuf_.destroy(device_.logi_device());

                for (auto& sh_data : fd.shadows_) {
                    sh_data.ubuf_.destroy(device_.mem_alloc());
                }
            }

            desc_pool_.destroy(device_.logi_device());
            desc_pool_sh_.destroy(device_.logi_device());
            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "compo_dlight"; }

        void on_resize(uint32_t width, uint32_t height) override {
            this->recreate_desc_sets(frame_data_, desc_pool_, device_);
            this->recreate_render_pass(render_pass_, device_);
            this->recreate_pipeline(pipeline_, device_);
            this->recreate_fbufs(frame_data_, device_);
        }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<task::RpTask>();
            out->init(
                rp_res_.shadow_maps_->dlights(),
                cosmos_.reg(),
                rp_res_.gbuf_,
                *this,
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
                auto& fd = fdata[i];
                fd.desc_set_ = desc_sets[i];
                fd.ubuf_.init_ubuf<U_CompoAtmosSurfMain>(device.mem_alloc());

                // Depth
                writer.add_img_info()
                    .set_img_view(gbufs.depth(i).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 0);
                // Albedo
                writer.add_img_info()
                    .set_img_view(gbufs.albedo(i).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 1);
                // Normal
                writer.add_img_info()
                    .set_img_view(gbufs.normal(i).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 2);
                // Material
                writer.add_img_info()
                    .set_img_view(gbufs.material(i).image_view())
                    .set_sampler(device.samplers().get_linear())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 3);
                // Trans LUT
                writer.add_img_info()
                    .set_img_view(fd.trans_lut_->view_.get())
                    .set_sampler(device.samplers().get_cubemap())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 4);
                // Multi Scat
                writer.add_img_info()
                    .set_img_view(fd.multi_scat_->view_.get())
                    .set_sampler(device.samplers().get_cubemap())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 5);
                // Sky View LUT
                writer.add_img_info()
                    .set_img_view(fd.sky_view_lut_->view_.get())
                    .set_sampler(device.samplers().get_cubemap())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 6);
                // Cam Scat Vol
                writer.add_img_info()
                    .set_img_view(fd.cam_scat_vol_->view_.get())
                    .set_sampler(device.samplers().get_cubemap())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 7);
                // U_CompoDlight
                writer.add_buf_info(fd.ubuf_);
                writer.add_buf_write(fd.desc_set_, 8);
            }
            writer.apply_all(device.logi_device());
        }

        void recreate_render_pass(
            mirinae::RenderPass& render_pass, mirinae::VulkanDevice& device
        ) const {
            mirinae::RenderPassBuilder builder;

            builder.attach_desc()
                .add(rp_res_.gbuf_.compo_format())
                .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .op_pair_clear_store();

            builder.color_attach_ref().add_color_attach(0);

            builder.subpass_dep().add().preset_single();

            render_pass.reset(builder.build(device.logi_device()), device);
        }

        void recreate_pipeline(
            mirinae::RpPipeline& pipeline, mirinae::VulkanDevice& device
        ) const {
            mirinae::PipelineBuilder builder{ device };

            builder.shader_stages()
                .add_vert(":asset/spv/compo_atmos_surface_vert.spv")
                .add_frag(":asset/spv/compo_atmos_surface_frag.spv");

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
                    .set_dim(rp_res_.gbuf_.width(), rp_res_.gbuf_.height())
                    .add_attach(rp_res_.gbuf_.compo(i).image_view());
                fdata.at(i).fbuf_.reset(
                    fbuf_cinfo.build(device), device.logi_device()
                );
            }
        }

        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        ::FrameDataArr frame_data_;
        mirinae::DescPool desc_pool_, desc_pool_sh_;
    };

}  // namespace


namespace mirinae::rp::compo {

    std::unique_ptr<IRpBase> create_rps_atmos_surface(RpCreateBundle& cbundle) {
        return std::make_unique<RpStatesCompoDlight>(
            cbundle.cosmos_, cbundle.rp_res_, cbundle.device_
        );
    }

}  // namespace mirinae::rp::compo
