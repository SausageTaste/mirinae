#include "mirinae/vulkan/renpass/compo/compo.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/vulkan/base/render/cmdbuf.hpp"
#include "mirinae/vulkan/base/render/draw_set.hpp"
#include "mirinae/vulkan/base/render/mem_cinfo.hpp"
#include "mirinae/vulkan/base/renderpass/builder.hpp"


namespace {

    class U_CompoSlightMain {

    public:
        U_CompoSlightMain& set_proj(const glm::dmat4& v) {
            proj_ = v;
            proj_inv_ = glm::inverse(v);
            return *this;
        }

        U_CompoSlightMain& set_view(const glm::dmat4& v) {
            view_ = v;
            view_inv_ = glm::inverse(v);
            return *this;
        }

        template <typename T>
        U_CompoSlightMain& set_fog_color(const glm::tvec3<T>& v) {
            fog_color_density_.x = static_cast<float>(v.x);
            fog_color_density_.y = static_cast<float>(v.y);
            fog_color_density_.z = static_cast<float>(v.z);
            return *this;
        }

        template <typename T>
        U_CompoSlightMain& set_fog_density(T v) {
            fog_color_density_.w = static_cast<float>(v);
            return *this;
        }

        template <typename T>
        U_CompoSlightMain& set_mie_anisotropy(T v) {
            mie_anisotropy_ = static_cast<float>(v);
            return *this;
        }

    private:
        glm::mat4 proj_;
        glm::mat4 proj_inv_;
        glm::mat4 view_;
        glm::mat4 view_inv_;
        glm::vec4 fog_color_density_;
        float mie_anisotropy_ = 0.5f;
    };


    struct U_CompoSlightPushConst {

    public:
        U_CompoSlightPushConst& set_light_mat(const glm::mat4& v) {
            light_mat_ = v;
            return *this;
        }

        U_CompoSlightPushConst& set_pos(const glm::dvec3& v) {
            pos_n_inner_angle_.x = static_cast<float>(v.x);
            pos_n_inner_angle_.y = static_cast<float>(v.y);
            pos_n_inner_angle_.z = static_cast<float>(v.z);
            return *this;
        }

        U_CompoSlightPushConst& set_direc(const glm::dvec3& v) {
            dir_n_outer_angle_.x = static_cast<float>(v.x);
            dir_n_outer_angle_.y = static_cast<float>(v.y);
            dir_n_outer_angle_.z = static_cast<float>(v.z);
            return *this;
        }

        U_CompoSlightPushConst& set_color(const glm::vec3& v) {
            color_n_max_dist_.x = static_cast<float>(v.x);
            color_n_max_dist_.y = static_cast<float>(v.y);
            color_n_max_dist_.z = static_cast<float>(v.z);
            return *this;
        }

        U_CompoSlightPushConst& set_inner_angle(sung::TAngle<double> angle) {
            const auto v = std::cos(angle.rad());
            pos_n_inner_angle_.w = static_cast<float>(v);
            return *this;
        }

        U_CompoSlightPushConst& set_outer_angle(sung::TAngle<double> angle) {
            const auto v = std::cos(angle.rad());
            dir_n_outer_angle_.w = static_cast<float>(v);
            return *this;
        }

        U_CompoSlightPushConst& set_max_dist(double max_dist) {
            color_n_max_dist_.w = static_cast<float>(max_dist);
            return *this;
        }

    private:
        glm::mat4 light_mat_;
        glm::vec4 pos_n_inner_angle_;
        glm::vec4 dir_n_outer_angle_;
        glm::vec4 color_n_max_dist_;
    };

    static_assert(sizeof(U_CompoSlightPushConst) < 128);


    struct ShadowMapData {
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    };


    struct FrameData {
        std::vector<ShadowMapData> shadows_;
        mirinae::Buffer ubuf_;
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
            this->update_ubuf(*reg_, *ctxt_, fd, *device_);
            this->record_barriers_shadow(cmdbuf_, *shadows_, *ctxt_);
            this->record(cmdbuf_, fd, *reg_, *rp_, *shadows_, *ctxt_, gbuf_ext);
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void update_ubuf(
            const entt::registry& reg,
            const mirinae::RpCtxt& ctxt,
            ::FrameData& fd,
            mirinae::VulkanDevice& device
        ) {
            U_CompoSlightMain ubuf;
            ubuf.set_proj(ctxt.main_cam_.proj())
                .set_view(ctxt.main_cam_.view());
            for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                ubuf.set_fog_color(atmos.fog_color_)
                    .set_fog_density(atmos.fog_density_)
                    .set_mie_anisotropy(atmos.mie_anisotropy_);
                break;
            }
            fd.ubuf_.set_data(ubuf);
        }

        static void record_barriers_shadow(
            const VkCommandBuffer cmdbuf,
            const mirinae::IShadowMapBundle& shadow_maps,
            const mirinae::RpCtxt& ctxt
        ) {
            for (size_t i = 0; i < shadow_maps.slight_count(); ++i) {
                const auto e = shadow_maps.slight_entt_at(i);
                if (entt::null == e)
                    continue;

                mirinae::ImageMemoryBarrier{}
                    .image(shadow_maps.slight_img_at(i))
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
            const ::FrameData& fd,
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            const mirinae::IShadowMapBundle& shadow_maps,
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

            for (size_t i = 0; i < shadow_maps.slight_count(); ++i) {
                const auto e = shadow_maps.slight_entt_at(i);
                if (entt::null == e)
                    continue;

                auto& sh_data = fd.shadows_.at(i);
                auto& slight = reg.get<mirinae::cpnt::SLight>(e);
                auto& tform = reg.get<mirinae::cpnt::Transform>(e);

                mirinae::DescSetBindInfo{}
                    .layout(rp.pipe_layout())
                    .first_set(1)
                    .set(sh_data.desc_set_)
                    .record(cmdbuf);

                U_CompoSlightPushConst pc;
                pc.set_light_mat(slight.make_light_mat(tform) * view_inv)
                    .set_pos(view_mat * glm::dvec4(tform.pos_, 1))
                    .set_direc(slight.calc_to_light_dir(view_mat, tform))
                    .set_color(slight.color_.scaled_color())
                    .set_inner_angle(slight.inner_angle_)
                    .set_outer_angle(slight.outer_angle_)
                    .set_max_dist(100);

                mirinae::PushConstInfo{}
                    .layout(rp.pipe_layout())
                    .add_stage_frag()
                    .record(cmdbuf, pc);

                vkCmdDraw(cmdbuf, 3, 1, 0, 0);
            }

            vkCmdEndRenderPass(cmdbuf);
        }

        const mirinae::DebugLabel DEBUG_LABEL{ "Compo SLight", 1, 0.96, 0.61 };

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


// Compo Slight
namespace {

    class RpStatesCompoSlight
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<1> {

    public:
        RpStatesCompoSlight(
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
                    .add_img_frag(1)    // depth
                    .add_img_frag(1)    // albedo
                    .add_img_frag(1)    // normal
                    .add_img_frag(1)    // material
                    .add_ubuf_frag(1);  // U_CompoSlight
                desclays.add(builder, device.logi_device());
            }

            // Desc layout: shadow map
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":shadow_map" };
                builder.add_img_frag(1);  // shadow map
                desclays.add(builder, device.logi_device());
            }

            // Desc sets: main
            this->recreate_desc_sets(frame_data_, desc_pool_, device_);

            // Desc sets: shadow map
            {
                const auto sh_count = rp_res.shadow_maps_->slight_count();
                auto& desc_layout = desclays.get(name_s() + ":shadow_map");

                desc_pool_sh_.init(
                    sh_count, desc_layout.size_info(), device.logi_device()
                );

                auto desc_sets = desc_pool_sh_.alloc(
                    sh_count, desc_layout.layout(), device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < sh_count; i++) {
                    auto descset = desc_sets[i];

                    for (auto& fd : frame_data_) {
                        auto& sh = fd.shadows_.emplace_back();
                        sh.desc_set_ = descset;
                    }

                    // Shadow map
                    writer.add_img_info()
                        .set_img_view(rp_res.shadow_maps_->slight_view_at(i))
                        .set_sampler(device.samplers().get_shadow())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(descset, 0);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclays.get(name_s() + ":main").layout())
                    .desc(desclays.get(name_s() + ":shadow_map").layout())
                    .add_frag_flag()
                    .pc<U_CompoSlightPushConst>()
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

        ~RpStatesCompoSlight() override {
            for (auto& fd : frame_data_) {
                fd.ubuf_.destroy();
                fd.fbuf_.destroy(device_.logi_device());
            }

            desc_pool_.destroy(device_.logi_device());
            desc_pool_sh_.destroy(device_.logi_device());
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
            auto out = std::make_unique<task::RpTask>();
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

            mirinae::BufferCreateInfo buf_cinfo;
            buf_cinfo.preset_ubuf(sizeof(U_CompoSlightMain));

            mirinae::DescWriter writer;
            for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                const mirinae::FrameIndex f_idx(i);

                auto& fd = fdata[i];
                fd.desc_set_ = desc_sets[i];
                fd.ubuf_.init(buf_cinfo, device.mem_alloc());

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
                // U_CompoSlight
                writer.add_buf_info(fd.ubuf_);
                writer.add_buf_write(fd.desc_set_, 4);
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
                .add_vert(":asset/spv/compo_slight_vert.spv")
                .add_frag(":asset/spv/compo_slight_frag.spv");

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
        mirinae::DescPool desc_pool_, desc_pool_sh_;
    };

}  // namespace


namespace mirinae::rp::compo {

    std::unique_ptr<IRpBase> create_rps_slight(RpCreateBundle& cbundle) {
        return std::make_unique<RpStatesCompoSlight>(
            cbundle.cosmos_, cbundle.rp_res_, cbundle.device_
        );
    }

}  // namespace mirinae::rp::compo
