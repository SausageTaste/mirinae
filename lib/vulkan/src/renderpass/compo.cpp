#include "mirinae/renderpass/compo.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"


// Compo Dlight
namespace {

    class U_CompoDlightMain {

    public:
        U_CompoDlightMain& set_proj(const glm::dmat4& v) {
            proj_ = v;
            proj_inv_ = glm::inverse(v);
            return *this;
        }

        U_CompoDlightMain& set_view(const glm::dmat4& v) {
            view_ = v;
            view_inv_ = glm::inverse(v);
            return *this;
        }

        template <typename T>
        U_CompoDlightMain& set_fog_color(const glm::tvec3<T>& v) {
            fog_color_density_.x = static_cast<float>(v.x);
            fog_color_density_.y = static_cast<float>(v.y);
            fog_color_density_.z = static_cast<float>(v.z);
            return *this;
        }

        template <typename T>
        U_CompoDlightMain& set_fog_density(T v) {
            fog_color_density_.w = static_cast<float>(v);
            return *this;
        }

        template <typename T>
        U_CompoDlightMain& set_mie_anisotropy(T v) {
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


    class RpStatesCompoDlight : public mirinae::IRpStates {

    public:
        RpStatesCompoDlight(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            namespace cpnt = mirinae::cpnt;
            auto& reg = cosmos.reg();

            // Desc layout: main
            {
                mirinae::DescLayoutBuilder builder{ name() + ":main" };
                builder
                    .add_img_frag(1)    // depth
                    .add_img_frag(1)    // albedo
                    .add_img_frag(1)    // normal
                    .add_img_frag(1)    // material
                    .add_ubuf_frag(1);  // U_CompoDlight
                desclayouts.add(builder, device.logi_device());
            }

            // Desc layout: shadow map
            {
                mirinae::DescLayoutBuilder builder{ name() + ":shadow_map" };
                builder
                    .add_img_frag(1)    // shadow map
                    .add_ubuf_frag(1);  // U_CompoDlightShadowMap
                desclayouts.add(builder, device.logi_device());
            }

            // Desc sets: main
            {
                auto& desc_layout = desclayouts.get(name() + ":main");

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
                for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_ = desc_sets[i];
                    fd.ubuf_.init_ubuf<U_CompoDlightMain>(device.mem_alloc());

                    // Depth
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.depth(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 0);
                    // Albedo
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.albedo(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 1);
                    // Normal
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.normal(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 2);
                    // Material
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.material(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 3);
                    // U_CompoDlight
                    writer.add_buf_info(fd.ubuf_);
                    writer.add_buf_write(fd.desc_set_, 4);
                }
                writer.apply_all(device.logi_device());
            }

            // Desc sets: shadow map
            {
                constexpr auto FD_COUNT = mirinae::MAX_FRAMES_IN_FLIGHT;
                auto& dlights = rp_res.shadow_maps_->dlights();
                auto& desc_layout = desclayouts.get(name() + ":shadow_map");

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

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(rp_res.gbuf_.compo_format())
                    .ini_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_clear_store();

                builder.color_attach_ref().add_color_attach(0);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclayouts.get(name() + ":main").layout())
                    .desc(desclayouts.get(name() + ":shadow_map").layout())
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/compo_dlight_vert.spv")
                    .add_frag(":asset/spv/compo_dlight_frag.spv");

                builder.rasterization_state().cull_mode_back();

                builder.color_blend_state().add().set_additive_blend();

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_ = builder.build(render_pass_, pipe_layout_);
            }

            // Framebuffers
            {
                for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    mirinae::FbufCinfo fbuf_cinfo;
                    fbuf_cinfo.set_rp(render_pass_)
                        .set_dim(rp_res.gbuf_.width(), rp_res.gbuf_.height())
                        .add_attach(rp_res.gbuf_.compo(i).image_view());
                    frame_data_[i].fbuf_ = fbuf_cinfo.build(device);
                }

                fbuf_width_ = rp_res.gbuf_.width();
                fbuf_height_ = rp_res.gbuf_.height();
            }

            // Misc
            {
                clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            }

            return;
        }

        ~RpStatesCompoDlight() override {
            for (auto& fd : frame_data_) {
                fd.ubuf_.destroy(device_.mem_alloc());

                if (VK_NULL_HANDLE != fd.fbuf_) {
                    vkDestroyFramebuffer(
                        device_.logi_device(), fd.fbuf_, nullptr
                    );
                    fd.fbuf_ = VK_NULL_HANDLE;
                }

                for (auto& sh_data : fd.shadows_) {
                    sh_data.ubuf_.destroy(device_.mem_alloc());
                }
            }

            desc_pool_.destroy(device_.logi_device());
            desc_pool_sh_.destroy(device_.logi_device());
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "compo_dlight";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;
            auto& gbufs = rp_res_.gbuf_;
            auto& reg = ctxt.cosmos_->reg();
            auto& fd = frame_data_[ctxt.f_index_.get()];
            const VkExtent2D fbuf_ext{ fbuf_width_, fbuf_height_ };
            const auto& view_mat = ctxt.view_mat_;
            const auto view_inv = glm::inverse(view_mat);

            U_CompoDlightMain ubuf;
            ubuf.set_proj(ctxt.proj_mat_).set_view(view_mat);
            for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                ubuf.set_fog_color(atmos.fog_color_)
                    .set_fog_density(atmos.fog_density_)
                    .set_mie_anisotropy(atmos.mie_anisotropy_);
                break;
            }
            fd.ubuf_.set_data(ubuf, device_.mem_alloc());

            mirinae::ImageMemoryBarrier{}
                .image(gbufs.depth(ctxt.f_index_.get()).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .old_layout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_src_access(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .set_signle_mip_layer()
                .record_single(
                    ctxt.cmdbuf_,
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
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );

            color_barrier.image(gbufs.normal(ctxt.f_index_.get()).image())
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );

            color_barrier.image(gbufs.material(ctxt.f_index_.get()).image())
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );

            auto& dlights = rp_res_.shadow_maps_->dlights();
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
                        ctxt.cmdbuf_,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                    );
            }

            mirinae::RenderPassBeginInfo{}
                .rp(render_pass_)
                .fbuf(fd.fbuf_)
                .wh(fbuf_ext)
                .clear_value_count(clear_values_.size())
                .clear_values(clear_values_.data())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(pipe_layout_)
                .set(fd.desc_set_)
                .record(cmdbuf);

            for (uint32_t i = 0; i < dlights.count(); ++i) {
                auto& dlight = dlights.at(i);
                const auto e = dlight.entt();
                if (entt::null == e)
                    continue;

                auto& fd = frame_data_[ctxt.f_index_.get()];
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
                sh_data.ubuf_.set_data(ubuf_sh, device_.mem_alloc());

                mirinae::DescSetBindInfo{}
                    .layout(pipe_layout_)
                    .first_set(1)
                    .set(sh_data.desc_set_)
                    .record(cmdbuf);

                vkCmdDraw(cmdbuf, 3, 1, 0, 0);
            }

            vkCmdEndRenderPass(cmdbuf);
        }

    private:
        struct ShadowMapData {
            mirinae::Buffer ubuf_;
            VkDescriptorSet desc_set_;
        };

        struct FrameData {
            std::vector<ShadowMapData> shadows_;
            mirinae::Buffer ubuf_;
            VkDescriptorSet desc_set_;
            VkFramebuffer fbuf_;
        };

        static entt::entity select_atmos_simple(entt::registry& reg) {
            for (auto entity : reg.view<mirinae::cpnt::AtmosphereSimple>())
                return entity;

            return entt::null;
        }

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;

        std::shared_ptr<mirinae::ITexture> sky_tex_;
        mirinae::DescPool desc_pool_, desc_pool_sh_;
        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        std::array<VkClearValue, 1> clear_values_;
        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
    };

}  // namespace


// Compo Slight
namespace {

    struct U_CompoSlightMain : public U_CompoDlightMain {};


    struct U_CompoSlightPushConst {

    public:
        U_CompoSlightPushConst& set_light_mat(const glm::mat4& v) {
            light_mat = v;
            return *this;
        }

        U_CompoSlightPushConst& set_pos(const glm::dvec3& v) {
            pos_n_inner_angle.x = static_cast<float>(v.x);
            pos_n_inner_angle.y = static_cast<float>(v.y);
            pos_n_inner_angle.z = static_cast<float>(v.z);
            return *this;
        }

        U_CompoSlightPushConst& set_direc(const glm::dvec3& v) {
            dir_n_outer_angle.x = static_cast<float>(v.x);
            dir_n_outer_angle.y = static_cast<float>(v.y);
            dir_n_outer_angle.z = static_cast<float>(v.z);
            return *this;
        }

        U_CompoSlightPushConst& set_color(const glm::vec3& v) {
            color_n_max_dist.x = static_cast<float>(v.x);
            color_n_max_dist.y = static_cast<float>(v.y);
            color_n_max_dist.z = static_cast<float>(v.z);
            return *this;
        }

        U_CompoSlightPushConst& set_inner_angle(sung::TAngle<double> angle) {
            const auto v = std::cos(angle.rad());
            pos_n_inner_angle.w = static_cast<float>(v);
            return *this;
        }

        U_CompoSlightPushConst& set_outer_angle(sung::TAngle<double> angle) {
            const auto v = std::cos(angle.rad());
            dir_n_outer_angle.w = static_cast<float>(v);
            return *this;
        }

        U_CompoSlightPushConst& set_max_dist(double max_dist) {
            color_n_max_dist.w = static_cast<float>(max_dist);
            return *this;
        }

    private:
        glm::mat4 light_mat;
        glm::vec4 pos_n_inner_angle;
        glm::vec4 dir_n_outer_angle;
        glm::vec4 color_n_max_dist;
    };

    static_assert(sizeof(U_CompoSlightPushConst) < 128);


    class RpStatesCompoSlight : public mirinae::IRpStates {

    public:
        RpStatesCompoSlight(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            namespace cpnt = mirinae::cpnt;
            auto& reg = cosmos.reg();

            // Desc layout: main
            {
                mirinae::DescLayoutBuilder builder{ name() + ":main" };
                builder
                    .add_img_frag(1)    // depth
                    .add_img_frag(1)    // albedo
                    .add_img_frag(1)    // normal
                    .add_img_frag(1)    // material
                    .add_ubuf_frag(1);  // U_CompoSlight
                desclayouts.add(builder, device.logi_device());
            }

            // Desc layout: shadow map
            {
                mirinae::DescLayoutBuilder builder{ name() + ":shadow_map" };
                builder.add_img_frag(1);  // shadow map
                desclayouts.add(builder, device.logi_device());
            }

            // Desc sets: main
            {
                auto& desc_layout = desclayouts.get(name() + ":main");

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
                for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_ = desc_sets[i];
                    fd.ubuf_.init_ubuf<U_CompoSlightMain>(device.mem_alloc());

                    // Depth
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.depth(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 0);
                    // Albedo
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.albedo(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 1);
                    // Normal
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.normal(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 2);
                    // Material
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.material(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 3);
                    // U_CompoSlight
                    writer.add_buf_info(fd.ubuf_);
                    writer.add_buf_write(fd.desc_set_, 4);
                }
                writer.apply_all(device.logi_device());
            }

            // Desc sets: shadow map
            {
                const auto sh_count = rp_res.shadow_maps_->slight_count();
                auto& desc_layout = desclayouts.get(name() + ":shadow_map");

                desc_pool_sh_.init(
                    sh_count, desc_layout.size_info(), device.logi_device()
                );

                auto desc_sets = desc_pool_sh_.alloc(
                    sh_count, desc_layout.layout(), device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < sh_count; i++) {
                    auto& fd = shmap_data_.emplace_back();
                    fd.desc_set_ = desc_sets[i];

                    // Shadow map
                    writer.add_img_info()
                        .set_img_view(rp_res.shadow_maps_->slight_view_at(i))
                        .set_sampler(device.samplers().get_shadow())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 0);
                }
                writer.apply_all(device.logi_device());
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(rp_res.gbuf_.compo_format())
                    .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();

                builder.color_attach_ref().add_color_attach(0);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclayouts.get(name() + ":main").layout())
                    .desc(desclayouts.get(name() + ":shadow_map").layout())
                    .add_frag_flag()
                    .pc<U_CompoSlightPushConst>()
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/compo_slight_vert.spv")
                    .add_frag(":asset/spv/compo_slight_frag.spv");

                builder.rasterization_state().cull_mode_back();

                builder.color_blend_state().add().set_additive_blend();

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_ = builder.build(render_pass_, pipe_layout_);
            }

            // Framebuffers
            {
                for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    mirinae::FbufCinfo fbuf_cinfo;
                    fbuf_cinfo.set_rp(render_pass_)
                        .set_dim(rp_res.gbuf_.width(), rp_res.gbuf_.height())
                        .add_attach(rp_res.gbuf_.compo(i).image_view());
                    frame_data_[i].fbuf_ = fbuf_cinfo.build(device);
                }

                fbuf_width_ = rp_res.gbuf_.width();
                fbuf_height_ = rp_res.gbuf_.height();
            }

            // Misc
            {
                clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            }

            return;
        }

        ~RpStatesCompoSlight() override {
            for (auto& fd : frame_data_) {
                fd.ubuf_.destroy(device_.mem_alloc());

                if (VK_NULL_HANDLE != fd.fbuf_) {
                    vkDestroyFramebuffer(
                        device_.logi_device(), fd.fbuf_, nullptr
                    );
                    fd.fbuf_ = VK_NULL_HANDLE;
                }
            }

            desc_pool_.destroy(device_.logi_device());
            desc_pool_sh_.destroy(device_.logi_device());
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "compo_slight";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;
            auto& reg = ctxt.cosmos_->reg();
            auto& fd = frame_data_[ctxt.f_index_.get()];
            const VkExtent2D fbuf_ext{ fbuf_width_, fbuf_height_ };
            const auto view_inv = glm::inverse(ctxt.view_mat_);

            U_CompoSlightMain ubuf;
            ubuf.set_proj(ctxt.proj_mat_).set_view(ctxt.view_mat_);
            for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                ubuf.set_fog_color(atmos.fog_color_)
                    .set_fog_density(atmos.fog_density_)
                    .set_mie_anisotropy(atmos.mie_anisotropy_);
                break;
            }
            fd.ubuf_.set_data(ubuf, device_.mem_alloc());

            for (size_t i = 0; i < rp_res_.shadow_maps_->slight_count(); ++i) {
                const auto e = rp_res_.shadow_maps_->slight_entt_at(i);
                if (entt::null == e)
                    continue;

                auto shadow_img = rp_res_.shadow_maps_->slight_img_at(i);
                mirinae::ImageMemoryBarrier{}
                    .image(shadow_img)
                    .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .old_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .new_lay(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .set_src_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                    .set_dst_acc(VK_ACCESS_SHADER_READ_BIT)
                    .set_signle_mip_layer()
                    .record_single(
                        ctxt.cmdbuf_,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                    );
            }

            mirinae::RenderPassBeginInfo{}
                .rp(render_pass_)
                .fbuf(fd.fbuf_)
                .wh(fbuf_ext)
                .clear_value_count(clear_values_.size())
                .clear_values(clear_values_.data())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(pipe_layout_)
                .set(fd.desc_set_)
                .record(cmdbuf);

            for (size_t i = 0; i < rp_res_.shadow_maps_->slight_count(); ++i) {
                const auto e = rp_res_.shadow_maps_->slight_entt_at(i);
                if (entt::null == e)
                    continue;

                auto& sh_data = shmap_data_.at(i);
                auto& slight = reg.get<mirinae::cpnt::SLight>(e);
                auto& tform = reg.get<mirinae::cpnt::Transform>(e);

                mirinae::DescSetBindInfo{}
                    .layout(pipe_layout_)
                    .first_set(1)
                    .set(sh_data.desc_set_)
                    .record(cmdbuf);

                U_CompoSlightPushConst pc;
                pc.set_light_mat(slight.make_light_mat(tform) * view_inv)
                    .set_pos(ctxt.view_mat_ * glm::dvec4(tform.pos_, 1))
                    .set_direc(slight.calc_to_light_dir(ctxt.view_mat_, tform))
                    .set_color(slight.color_.scaled_color())
                    .set_inner_angle(slight.inner_angle_)
                    .set_outer_angle(slight.outer_angle_)
                    .set_max_dist(100);

                mirinae::PushConstInfo{}
                    .layout(pipe_layout_)
                    .add_stage_frag()
                    .record(cmdbuf, pc);

                vkCmdDraw(cmdbuf, 3, 1, 0, 0);
            }

            vkCmdEndRenderPass(cmdbuf);
        }

    private:
        struct FrameData {
            mirinae::Buffer ubuf_;
            VkDescriptorSet desc_set_;
            VkFramebuffer fbuf_;
        };

        struct ShadowMapData {
            VkDescriptorSet desc_set_;
        };

        static entt::entity select_atmos_simple(entt::registry& reg) {
            for (auto entity : reg.view<mirinae::cpnt::AtmosphereSimple>())
                return entity;

            return entt::null;
        }

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;
        std::vector<ShadowMapData> shmap_data_;

        std::shared_ptr<mirinae::ITexture> sky_tex_;
        mirinae::DescPool desc_pool_, desc_pool_sh_;
        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        std::array<VkClearValue, 1> clear_values_;
        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
    };

}  // namespace


// Compo Envmap
namespace {

    struct U_CompoEnvmapPushConst {
        glm::mat4 view_inv_;
        glm::mat4 proj_inv_;
        glm::vec4 fog_color_density_;
    };


    class RpStatesCompoEnvmap : public mirinae::IRpStates {

    public:
        RpStatesCompoEnvmap(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            namespace cpnt = mirinae::cpnt;
            auto& reg = cosmos.reg();

            // Desc layout: main
            {
                mirinae::DescLayoutBuilder builder{ name() + ":main" };
                builder
                    .add_img_frag(1)   // depth
                    .add_img_frag(1)   // albedo
                    .add_img_frag(1)   // normal
                    .add_img_frag(1);  // material
                desclayouts.add(builder, device.logi_device());
            }

            // Desc layout: envmaps
            {
                mirinae::DescLayoutBuilder builder{ name() + ":envmaps" };
                builder
                    .add_img_frag(1)   // u_env_diffuse
                    .add_img_frag(1)   // u_env_specular
                    .add_img_frag(1);  // u_env_lut
                desclayouts.add(builder, device.logi_device());
            }

            // Desc sets: main
            {
                auto& desc_layout = desclayouts.get(name() + ":main");

                desc_pool_main_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.size_info(),
                    device.logi_device()
                );

                auto desc_sets = desc_pool_main_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desc_layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (uint32_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_main_ = desc_sets[i];

                    // Depth
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.depth(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_main_, 0);
                    // Albedo
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.albedo(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_main_, 1);
                    // Normal
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.normal(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_main_, 2);
                    // Material
                    writer.add_img_info()
                        .set_img_view(rp_res.gbuf_.material(i).image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_main_, 3);
                }
                writer.apply_all(device.logi_device());
            }

            // Desc sets: envmaps
            {
                auto& desc_layout = desclayouts.get(name() + ":envmaps");

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

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(rp_res.gbuf_.compo_format())
                    .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();

                builder.color_attach_ref().add_color_attach(0);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(desclayouts.get(name() + ":main").layout())
                    .desc(desclayouts.get(name() + ":envmaps").layout())
                    .add_frag_flag()
                    .pc<U_CompoEnvmapPushConst>()
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/compo_envmap_vert.spv")
                    .add_frag(":asset/spv/compo_envmap_frag.spv");

                builder.rasterization_state().cull_mode_back();

                builder.color_blend_state().add().set_additive_blend();

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_ = builder.build(render_pass_, pipe_layout_);
            }

            // Framebuffers
            {
                fbuf_width_ = rp_res.gbuf_.width();
                fbuf_height_ = rp_res.gbuf_.height();

                for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    mirinae::FbufCinfo fbuf_cinfo;
                    fbuf_cinfo.set_rp(render_pass_)
                        .set_dim(fbuf_width_, fbuf_height_)
                        .add_attach(rp_res.gbuf_.compo(i).image_view());
                    frame_data_[i].fbuf_ = fbuf_cinfo.build(device);
                }
            }

            // Misc
            {
                clear_values_.at(0).color = { 0, 0, 0, 1 };
            }

            return;
        }

        ~RpStatesCompoEnvmap() override {
            for (auto& fd : frame_data_) {
                if (VK_NULL_HANDLE != fd.fbuf_) {
                    vkDestroyFramebuffer(
                        device_.logi_device(), fd.fbuf_, nullptr
                    );
                    fd.fbuf_ = VK_NULL_HANDLE;
                }
            }

            desc_pool_main_.destroy(device_.logi_device());
            desc_pool_env_.destroy(device_.logi_device());
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "compo_envmap";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;
            auto& reg = ctxt.cosmos_->reg();
            auto& fd = frame_data_[ctxt.f_index_.get()];
            const VkExtent2D fbuf_ext{ fbuf_width_, fbuf_height_ };

            mirinae::RenderPassBeginInfo{}
                .rp(render_pass_)
                .fbuf(fd.fbuf_)
                .wh(fbuf_ext)
                .clear_value_count(clear_values_.size())
                .clear_values(clear_values_.data())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(pipe_layout_)
                .first_set(0)
                .set(fd.desc_set_main_)
                .record(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(pipe_layout_)
                .first_set(1)
                .set(fd.desc_set_env_)
                .record(cmdbuf);

            U_CompoEnvmapPushConst pc;
            pc.proj_inv_ = glm::inverse(ctxt.proj_mat_);
            pc.view_inv_ = glm::inverse(ctxt.view_mat_);
            for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                pc.fog_color_density_ = glm::vec4(
                    atmos.fog_color_, atmos.fog_density_
                );
                break;
            }

            mirinae::PushConstInfo{}
                .layout(pipe_layout_)
                .add_stage_frag()
                .record(cmdbuf, pc);

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmdbuf);
        }

    private:
        struct FrameData {
            VkDescriptorSet desc_set_main_ = VK_NULL_HANDLE;
            VkDescriptorSet desc_set_env_ = VK_NULL_HANDLE;
            VkFramebuffer fbuf_ = VK_NULL_HANDLE;
        };

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;

        mirinae::DescPool desc_pool_main_, desc_pool_env_;
        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        std::array<VkClearValue, 1> clear_values_;
        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
    };

}  // namespace


// Compo Sky
namespace {

    class RpStatesCompoSky : public mirinae::IRpStates {

    public:
        RpStatesCompoSky(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device), rp_res_(rp_res) {
            namespace cpnt = mirinae::cpnt;
            auto& reg = cosmos.reg();

            // Sky texture
            {
                auto e = this->select_atmos_simple(cosmos.reg());
                auto& atmos = cosmos.reg().get<cpnt::AtmosphereSimple>(e);
                auto& tex = *rp_res.tex_man_;
                if (tex.request_blck(atmos.sky_tex_path_, false)) {
                    sky_tex_ = tex.get(atmos.sky_tex_path_);
                } else {
                    sky_tex_ = tex.missing_tex();
                }
            }

            // Descriptor layout
            {
                mirinae::DescLayoutBuilder builder{ this->name() + ":main" };
                builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
                desclayouts.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                auto& desc_layout = desclayouts.get(name() + ":main");

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
                        .set_img_view(sky_tex_->image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 0);
                }
                writer.apply_all(device.logi_device());
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(rp_res.gbuf_.depth_format())
                    .ini_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .fin_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
                    .stor_op(VK_ATTACHMENT_STORE_OP_STORE);
                builder.attach_desc()
                    .add(rp_res.gbuf_.compo_format())
                    .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .op_pair_load_store();

                builder.color_attach_ref().add_color_attach(1);

                builder.depth_attach_ref().set(0);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device.logi_device());
            }

            // Pipeline layout
            {
                auto& desc_layout = desclayouts.get(name() + ":main");

                mirinae::PipelineLayoutBuilder{}
                    .desc(desc_layout.layout())
                    .add_frag_flag()
                    .pc<mirinae::U_CompoSkyMain>()
                    .build(pipe_layout_, device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/compo_sky_vert.spv")
                    .add_frag(":asset/spv/compo_sky_frag.spv");

                builder.depth_stencil_state()
                    .depth_test_enable(true)
                    .depth_write_enable(false)
                    .depth_compare_op(VK_COMPARE_OP_GREATER_OR_EQUAL);

                builder.color_blend_state().add(false, 1);

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_ = builder.build(render_pass_, pipe_layout_);
            }

            // Framebuffers
            {
                for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                    mirinae::FbufCinfo fbuf_cinfo;
                    fbuf_cinfo.set_rp(render_pass_)
                        .set_dim(rp_res.gbuf_.width(), rp_res.gbuf_.height())
                        .add_attach(rp_res.gbuf_.depth(i).image_view())
                        .add_attach(rp_res.gbuf_.compo(i).image_view());
                    frame_data_[i].fbuf_ = fbuf_cinfo.build(device);
                }

                fbuf_width_ = rp_res.gbuf_.width();
                fbuf_height_ = rp_res.gbuf_.height();
            }

            // Misc
            {
                clear_values_.at(0).depthStencil = { 0, 0 };
                clear_values_.at(1).color = { 0.0f, 0.0f, 0.0f, 1.0f };
            }

            return;
        }

        ~RpStatesCompoSky() override {
            for (auto& fd : frame_data_) {
                if (VK_NULL_HANDLE != fd.fbuf_) {
                    vkDestroyFramebuffer(
                        device_.logi_device(), fd.fbuf_, nullptr
                    );
                    fd.fbuf_ = VK_NULL_HANDLE;
                }
            }

            sky_tex_.reset();
            desc_pool_.destroy(device_.logi_device());
            render_pass_.destroy(device_);
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        const std::string& name() const override {
            static const std::string name = "compo_sky";
            return name;
        }

        void record(const mirinae::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf_;
            auto& fd = frame_data_[ctxt.f_index_.get()];

            const VkExtent2D fbuf_ext{ fbuf_width_, fbuf_height_ };

            mirinae::ImageMemoryBarrier{}
                .image(ctxt.rp_res_->gbuf_.depth(ctxt.f_index_.get()).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .old_lay(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .new_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .set_src_acc(VK_ACCESS_SHADER_READ_BIT)
                .set_dst_acc(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
                .set_signle_mip_layer()
                .record_single(
                    ctxt.cmdbuf_,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                );

            mirinae::RenderPassBeginInfo{}
                .rp(render_pass_)
                .fbuf(fd.fbuf_)
                .wh(fbuf_ext)
                .clear_value_count(clear_values_.size())
                .clear_values(clear_values_.data())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(pipe_layout_)
                .set(fd.desc_set_)
                .record(cmdbuf);

            mirinae::U_CompoSkyMain pc;
            pc.proj_inv_ = glm::inverse(ctxt.proj_mat_);
            pc.view_inv_ = glm::inverse(ctxt.view_mat_);
            if (auto& atmos = ctxt.draw_sheet_->atmosphere_)
                pc.fog_color_density_ = glm::vec4{ atmos->fog_color_,
                                                   atmos->fog_density_ };

            mirinae::PushConstInfo{}
                .layout(pipe_layout_)
                .add_stage_frag()
                .record(cmdbuf, pc);

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);

            vkCmdEndRenderPass(cmdbuf);
        }

    private:
        struct FrameData {
            VkDescriptorSet desc_set_;
            VkFramebuffer fbuf_;
        };

        static entt::entity select_atmos_simple(entt::registry& reg) {
            for (auto entity : reg.view<mirinae::cpnt::AtmosphereSimple>())
                return entity;

            return entt::null;
        }

        mirinae::VulkanDevice& device_;
        mirinae::RpResources& rp_res_;

        std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT> frame_data_;
        std::shared_ptr<mirinae::ITexture> sky_tex_;
        mirinae::DescPool desc_pool_;
        mirinae::RenderPass render_pass_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        std::array<VkClearValue, 2> clear_values_;
        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
    };

}  // namespace


namespace mirinae::rp::compo {

    URpStates create_rps_dlight(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesCompoDlight>(
            cosmos, rp_res, desclayouts, device
        );
    }

    URpStates create_rps_slight(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesCompoSlight>(
            cosmos, rp_res, desclayouts, device
        );
    }

    URpStates create_rps_envmap(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesCompoEnvmap>(
            cosmos, rp_res, desclayouts, device
        );
    }

    URpStates create_rps_sky(
        mirinae::CosmosSimulator& cosmos,
        mirinae::RpResources& rp_res,
        mirinae::DesclayoutManager& desclayouts,
        mirinae::VulkanDevice& device
    ) {
        return std::make_unique<RpStatesCompoSky>(
            cosmos, rp_res, desclayouts, device
        );
    }

}  // namespace mirinae::rp::compo
