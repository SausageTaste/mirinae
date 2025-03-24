#include "mirinae/renderer.hpp"

#include <daltools/common/util.h>
#include <entt/entity/registry.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/camera.hpp"
#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lightweight/script.hpp"
#include "mirinae/math/mamath.hpp"
#include "mirinae/overlay/overlay.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/render_graph.hpp"
#include "mirinae/render/renderpass.hpp"
#include "mirinae/render/renderpass/builder.hpp"
#include "mirinae/render/renderpass/compo.hpp"
#include "mirinae/render/renderpass/envmap.hpp"
#include "mirinae/render/renderpass/gbuf.hpp"
#include "mirinae/render/renderpass/ocean.hpp"
#include "mirinae/render/renderpass/shadow.hpp"


namespace {

    bool is_fbuf_too_small(uint32_t width, uint32_t height) {
        if (width < 5)
            return true;
        if (height < 5)
            return true;
        else
            return false;
    }

    template <typename T>
    std::pair<T, T> calc_scaled_dimensions(T w, T h, double factor) {
        return std::make_pair(
            static_cast<T>(static_cast<double>(w) * factor),
            static_cast<T>(static_cast<double>(h) * factor)
        );
    }


    class DominantCommandProc : public mirinae::IInputProcessor {

    public:
        DominantCommandProc(mirinae::VulkanDevice& device) : device_(device) {}

        bool on_key_event(const mirinae::key::Event& e) override {
            keys_.notify(e);

            if (e.key == mirinae::key::KeyCode::enter) {
                if (keys_.is_pressed(mirinae::key::KeyCode::lalt)) {
                    if (e.action_type == mirinae::key::ActionType::up) {
                        device_.osio().toggle_fullscreen();
                    }
                    return true;
                }
            }

            return false;
        }

    private:
        mirinae::key::EventAnalyzer keys_;
        mirinae::VulkanDevice& device_;
    };


    class FrameSync {

    public:
        void init(VkDevice logi_device) {
            this->destroy(logi_device);

            for (auto& x : img_available_semaphores_) x.init(logi_device);
            for (auto& x : render_finished_semaphores_) x.init(logi_device);
            for (auto& x : in_flight_fences_) x.init(true, logi_device);
        }

        void destroy(VkDevice logi_device) {
            for (auto& x : img_available_semaphores_) x.destroy(logi_device);
            for (auto& x : render_finished_semaphores_) x.destroy(logi_device);
            for (auto& x : in_flight_fences_) x.destroy(logi_device);
        }

        mirinae::Semaphore& get_cur_img_ava_semaph() {
            return img_available_semaphores_.at(cur_frame_.get());
        }
        mirinae::Semaphore& get_cur_render_fin_semaph() {
            return render_finished_semaphores_.at(cur_frame_.get());
        }
        mirinae::Fence& get_cur_in_flight_fence() {
            return in_flight_fences_.at(cur_frame_.get());
        }

        mirinae::FrameIndex get_frame_index() const { return cur_frame_; }
        void increase_frame_index() {
            cur_frame_ = (cur_frame_ + 1) % mirinae::MAX_FRAMES_IN_FLIGHT;
        }

    private:
        std::array<mirinae::Semaphore, mirinae::MAX_FRAMES_IN_FLIGHT>
            img_available_semaphores_;
        std::array<mirinae::Semaphore, mirinae::MAX_FRAMES_IN_FLIGHT>
            render_finished_semaphores_;
        std::array<mirinae::Fence, mirinae::MAX_FRAMES_IN_FLIGHT>
            in_flight_fences_;
        mirinae::FrameIndex cur_frame_{ 0 };
    };


    class RpMasters {

    public:
        void create_std_rp(
            mirinae::CosmosSimulator& cosmos,
            mirinae::rg::RenderGraphDef& rg_def,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        ) {
            this->destroy_std_rp();

            rp_pre_.push_back(
                mirinae::rp::ocean::create_rp_states_ocean_tilde_h(
                    rp_res, desclayouts, device
                )
            );

            rp_pre_.push_back(
                mirinae::rp::ocean::create_rp_states_ocean_tilde_hkt(
                    rp_res, desclayouts, device
                )
            );

            /*/
            rp_pre_.push_back(
                mirinae::rp::ocean::create_rp_states_ocean_naive_ift(
                    rp_res, desclayouts, device
                )
            );
            /*/
            rp_pre_.push_back(
                mirinae::rp::ocean::create_rp_states_ocean_butterfly(
                    rp_res, desclayouts, device
                )
            );
            //*/

            rp_pre_.push_back(
                mirinae::rp::ocean::create_rp_states_ocean_finalize(
                    rp_res, desclayouts, device
                )
            );

            rp_pre_.push_back(
                mirinae::rp::create_rp_states_shadow_static(
                    rp_res, desclayouts, device
                )
            );

            rp_pre_.push_back(
                mirinae::rp::create_rp_states_shadow_skinned(
                    rp_res, desclayouts, device
                )
            );

            rp_pre_.push_back(
                mirinae::rp::envmap::create_rp_states_envmap(
                    cosmos, rp_res, desclayouts, device
                )
            );

            rg_ = rg_def.build(swapchain, device);

            rp_post_.push_back(
                mirinae::rp::gbuf::create_rp_states_gbuf(
                    rp_res, desclayouts, swapchain, device
                )
            );

            rp_post_.push_back(
                mirinae::rp::gbuf::create_rp_states_gbuf_terrain(
                    rp_res, desclayouts, swapchain, device
                )
            );

            rp_post_.push_back(
                mirinae::rp::compo::create_rps_dlight(
                    cosmos, rp_res, desclayouts, device
                )
            );

            rp_post_.push_back(
                mirinae::rp::compo::create_rps_slight(
                    cosmos, rp_res, desclayouts, device
                )
            );

            rp_post_.push_back(
                mirinae::rp::compo::create_rps_envmap(
                    cosmos, rp_res, desclayouts, device
                )
            );

            rp_post_.push_back(
                mirinae::rp::compo::create_rps_sky(
                    cosmos, rp_res, desclayouts, device
                )
            );

            rp_post_.push_back(
                mirinae::rp::ocean::create_rp_states_ocean_tess(
                    swapchain.views_count(), cosmos, rp_res, desclayouts, device
                )
            );
        }

        void destroy_std_rp() {
            rp_pre_.clear();
            rp_post_.clear();
        }

        void record_computes(mirinae::RpContext& ctxt) {
            for (auto& rp : rp_pre_) {
                rp->record(ctxt);
            }
            for (auto& rp : rp_post_) {
                rp->record(ctxt);
            }
        }

    private:
        std::vector<mirinae::URpStates> rp_pre_, rp_post_;
        std::unique_ptr<mirinae::rg::IRenderGraph> rg_;
    };


    mirinae::DrawSheet make_draw_sheet(mirinae::Scene& scene) {
        namespace cpnt = mirinae::cpnt;

        mirinae::DrawSheet sheet;
        auto& reg = *scene.reg_;

        for (const auto e : reg.view<cpnt::MdlActorStatic>()) {
            auto& mactor = reg.get<cpnt::MdlActorStatic>(e);
            if (!mactor.model_)
                continue;
            auto renmdl = mactor.get_model<mirinae::RenderModel>();
            if (!renmdl)
                continue;
            auto actor = mactor.get_actor<mirinae::RenderActor>();
            if (!actor)
                continue;

            glm::dmat4 model_mat(1);
            if (auto tfrom = reg.try_get<cpnt::Transform>(e))
                model_mat = tfrom->make_model_mat();

            const auto unit_count = renmdl->render_units_.size();
            for (size_t i = 0; i < unit_count; ++i) {
                if (!mactor.visibility_.get(i))
                    continue;

                auto& unit = renmdl->render_units_[i];
                auto& dst = sheet.get_static(unit);
                auto& dst_actor = dst.actors_.emplace_back();
                dst_actor.actor_ = actor;
                dst_actor.model_mat_ = model_mat;
            }

            const auto unit_trs_count = renmdl->render_units_alpha_.size();
            for (size_t i = 0; i < unit_trs_count; ++i) {
                if (!mactor.visibility_.get(i + unit_count))
                    continue;

                auto& unit = renmdl->render_units_alpha_[i];
                auto& dst = sheet.get_static_trs(unit);
                auto& dst_actor = dst.actors_.emplace_back();
                dst_actor.actor_ = actor;
                dst_actor.model_mat_ = model_mat;
            }
        }

        for (const auto e : reg.view<cpnt::MdlActorSkinned>()) {
            auto& mactor = reg.get<cpnt::MdlActorSkinned>(e);
            if (!mactor.model_)
                continue;
            auto renmdl = mactor.get_model<mirinae::RenderModelSkinned>();
            if (!renmdl)
                continue;
            auto actor = mactor.get_actor<mirinae::RenderActorSkinned>();
            if (!actor)
                continue;

            glm::dmat4 model_mat(1);
            if (auto tfrom = reg.try_get<cpnt::Transform>(e))
                model_mat = tfrom->make_model_mat();

            const auto unit_count = renmdl->runits_.size();
            for (size_t i = 0; i < unit_count; ++i) {
                if (!mactor.visibility_.get(i))
                    continue;

                auto& unit = renmdl->runits_[i];
                auto& dst = sheet.get_skinned(unit);
                auto& dst_actor = dst.actors_.emplace_back();
                dst_actor.actor_ = actor;
                dst_actor.model_mat_ = model_mat;
            }

            const auto unit_trs_count = renmdl->runits_alpha_.size();
            for (size_t i = 0; i < unit_trs_count; ++i) {
                if (!mactor.visibility_.get(i + unit_count))
                    continue;

                auto& unit = renmdl->runits_alpha_[i];
                auto& dst = sheet.get_skinned_trs(unit);
                auto& dst_actor = dst.actors_.emplace_back();
                dst_actor.actor_ = actor;
                dst_actor.model_mat_ = model_mat;
            }
        }

        sheet.ocean_ = nullptr;
        for (auto e : scene.reg_->view<cpnt::Ocean>()) {
            // Only one ocean is allowed
            sheet.ocean_ = &scene.reg_->get<cpnt::Ocean>(e);
            break;
        }

        sheet.atmosphere_ = nullptr;
        for (auto e : reg.view<cpnt::AtmosphereSimple>()) {
            // Only one atmosphere is allowed
            sheet.atmosphere_ = &scene.reg_->get<cpnt::AtmosphereSimple>(e);
            break;
        }

        return sheet;
    }

}  // namespace


// Render pass states
namespace {

    class RpStatesTransp {

    public:
        void init(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        ) {
            auto& desclayout = desclayouts.get("transp:frame");
            desc_pool_.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.size_info(),
                device.logi_device()
            );
            desc_sets_ = desc_pool_.alloc(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.layout(),
                device.logi_device()
            );

            const auto sam_nea = device.samplers().get_nearest();
            const auto sam_lin = device.samplers().get_linear();
            const auto sam_cube = device.samplers().get_cubemap();
            auto& shadow_maps = *rp_res.shadow_maps_;

            mirinae::DescWriteInfoBuilder builder;
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& ubuf = ubufs_.emplace_back();
                ubuf.init_ubuf(
                    sizeof(mirinae::U_TranspFrame), device.mem_alloc()
                );

                builder.set_descset(desc_sets_.at(i))
                    .add_ubuf(ubuf)
                    .add_img_sampler(shadow_maps.dlight_view_at(0), sam_nea)
                    .add_img_sampler(shadow_maps.slight_view_at(0), sam_nea)
                    .add_img_sampler(rp_res.envmaps_->diffuse_at(0), sam_cube)
                    .add_img_sampler(rp_res.envmaps_->specular_at(0), sam_cube)
                    .add_img_sampler(rp_res.envmaps_->brdf_lut(), sam_lin);
            }
            builder.apply_all(device.logi_device());
        }

        void destroy(mirinae::VulkanDevice& device) {
            desc_pool_.destroy(device.logi_device());

            for (auto& ubuf : ubufs_) ubuf.destroy(device.mem_alloc());
            ubufs_.clear();
        }

        void record_static(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_ext,
            const mirinae::DrawSheet& draw_sheet,
            const mirinae::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("transp");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
                .wh(fbuf_ext)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cur_cmd_buf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cur_cmd_buf);

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };
            descset_info.first_set(0)
                .set(desc_sets_.at(frame_index.get()))
                .record(cur_cmd_buf);

            for (auto& pair : draw_sheet.static_trs_) {
                auto& unit = *pair.unit_;
                descset_info.first_set(1)
                    .set(unit.get_desc_set(frame_index.get()))
                    .record(cur_cmd_buf);
                unit.record_bind_vert_buf(cur_cmd_buf);

                for (auto& actor : pair.actors_) {
                    descset_info.first_set(2)
                        .set(actor.actor_->get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    vkCmdDrawIndexed(
                        cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                    );
                }
            }

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        void record_skinned(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_ext,
            const mirinae::DrawSheet& draw_sheet,
            const mirinae::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("transp_skin");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
                .wh(fbuf_ext)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cur_cmd_buf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cur_cmd_buf);

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };
            descset_info.first_set(0)
                .set(desc_sets_.at(frame_index.get()))
                .record(cur_cmd_buf);

            for (auto& pair : draw_sheet.skinned_trs_) {
                auto& unit = *pair.unit_;
                descset_info.first_set(1)
                    .set(unit.get_desc_set(frame_index.get()))
                    .record(cur_cmd_buf);

                unit.record_bind_vert_buf(cur_cmd_buf);

                for (auto& actor : pair.actors_) {
                    descset_info.first_set(2)
                        .set(actor.actor_->get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    vkCmdDrawIndexed(
                        cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                    );
                }
            }

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        mirinae::DescPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<mirinae::Buffer> ubufs_;
    };


    class RpStatesDebugMesh {

    public:
        void init(mirinae::VulkanDevice& device) {}

        void destroy(mirinae::VulkanDevice& device) {}

        void begin_record(
            const VkCommandBuffer cmdbuf,
            const VkExtent2D& fbuf_ext,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("debug_mesh");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
                .wh(fbuf_ext)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cmdbuf);
        }

        void draw(
            const VkCommandBuffer cmdbuf,
            const glm::vec4& p0,
            const glm::vec4& p1,
            const glm::vec4& p2,
            const glm::vec4& color,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            mirinae::U_DebugMeshPushConst pc;
            pc.vertices_[0] = p0;
            pc.vertices_[1] = p1;
            pc.vertices_[2] = p2;
            pc.color_ = color;

            mirinae::PushConstInfo{}
                .layout(rp_pkg.get("debug_mesh").pipeline_layout())
                .add_stage_vert()
                .add_stage_frag()
                .record(cmdbuf, pc);

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);
        }

        void end_record(
            const VkCommandBuffer cmdbuf,
            const VkExtent2D& fbuf_ext,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            vkCmdEndRenderPass(cmdbuf);
        }
    };


    class RpStatesFillscreen {

    public:
        void init(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::FbufImageBundle& fbufs,
            mirinae::VulkanDevice& device
        ) {
            auto& desclayout = desclayouts.get("fillscreen:main");
            desc_pool_.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.size_info(),
                device.logi_device()
            );
            desc_sets_ = desc_pool_.alloc(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.layout(),
                device.logi_device()
            );

            mirinae::DescWriteInfoBuilder builder;
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                builder.set_descset(desc_sets_.at(i))
                    .add_img_sampler(
                        fbufs.compo().image_view(),
                        device.samplers().get_linear()
                    );
            }
            builder.apply_all(device.logi_device());
        }

        void destroy(mirinae::VulkanDevice& device) {
            desc_pool_.destroy(device.logi_device());
        }

        void record(
            const VkCommandBuffer cmdbuf,
            const VkExtent2D shain_exd,
            const mirinae::cpnt::StandardCamera& camera,
            const mirinae::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("fillscreen");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
                .wh(shain_exd)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ shain_exd }.record_single(cmdbuf);
            mirinae::Rect2D{ shain_exd }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(rp.pipeline_layout())
                .set(desc_sets_.at(frame_index.get()))
                .record(cmdbuf);

            mirinae::U_FillScreenPushConst pc;
            pc.exposure_ = camera.exposure_;
            pc.gamma_ = camera.gamma_;

            mirinae::PushConstInfo{}
                .layout(rp.pipeline_layout())
                .add_stage_frag()
                .record(cmdbuf, pc);

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);

            vkCmdEndRenderPass(cmdbuf);
        }

        mirinae::DescPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
    };


    class RpStatesImgui {

    public:
        RpStatesImgui(mirinae::VulkanDevice& device) : device_(device) {}

        ~RpStatesImgui() { this->destroy(); }

        void init(mirinae::Swapchain& swchain) {
            // Descriptor pool
            {
                static const std::vector<VkDescriptorPoolSize> pool_sizes{
                    { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
                };

                desc_pool_.init(
                    1000,
                    pool_sizes.size(),
                    pool_sizes.data(),
                    device_.logi_device()
                );
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(swchain.format())
                    .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
                    .op_pair_load_store();

                builder.color_attach_ref().add_color_attach(0);

                builder.subpass_dep().add().preset_single();

                render_pass_ = builder.build(device_.logi_device());
            }

            // Framebuffer
            {
                fbuf_width_ = swchain.width();
                fbuf_height_ = swchain.height();

                VkFramebufferCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                info.renderPass = render_pass_;
                info.attachmentCount = 1;
                info.pAttachments = VK_NULL_HANDLE;
                info.width = fbuf_width_;
                info.height = fbuf_height_;
                info.layers = 1;

                for (uint32_t i = 0; i < swchain.views_count(); i++) {
                    auto view = swchain.view_at(i);
                    info.pAttachments = &view;
                    VkFramebuffer fbuf = VK_NULL_HANDLE;
                    VK_CHECK(vkCreateFramebuffer(
                        device_.logi_device(), &info, VK_NULL_HANDLE, &fbuf
                    ));
                    fbufs_.push_back(fbuf);
                }
            }

            return;
        }

        void destroy() {
            auto device = device_.logi_device();

            desc_pool_.destroy(device);

            if (VK_NULL_HANDLE != render_pass_) {
                vkDestroyRenderPass(device, render_pass_, nullptr);
                render_pass_ = VK_NULL_HANDLE;
            }

            for (auto fbuf : fbufs_)
                vkDestroyFramebuffer(device, fbuf, nullptr);
            fbufs_.clear();
        }

        void on_swchain_resize(mirinae::Swapchain& swchain) {
            if (swchain.width() == fbuf_width_ &&
                swchain.height() == fbuf_height_)
                return;

            // Destroy
            {
                auto device = device_.logi_device();
                for (auto fbuf : fbufs_)
                    vkDestroyFramebuffer(device, fbuf, nullptr);
                fbufs_.clear();
            }

            // Create Framebuffers
            {
                fbuf_width_ = swchain.width();
                fbuf_height_ = swchain.height();

                VkFramebufferCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                info.renderPass = render_pass_;
                info.attachmentCount = 1;
                info.pAttachments = VK_NULL_HANDLE;
                info.width = fbuf_width_;
                info.height = fbuf_height_;
                info.layers = 1;

                for (uint32_t i = 0; i < swchain.views_count(); i++) {
                    auto view = swchain.view_at(i);
                    info.pAttachments = &view;
                    VkFramebuffer fbuf = VK_NULL_HANDLE;
                    VK_CHECK(vkCreateFramebuffer(
                        device_.logi_device(), &info, VK_NULL_HANDLE, &fbuf
                    ));
                    fbufs_.push_back(fbuf);
                }
            }

            ImGui_ImplVulkan_SetMinImageCount(swchain.views_count());
        }

        void record(mirinae::RpContext& ctxt) {
            const auto cmdbuf = ctxt.cmdbuf_;
            const auto fbuf = fbufs_.at(ctxt.i_index_.get());
            const VkClearValue clear_value = { 0.0f, 0.0f, 0.0f, 1.0f };

            {
                VkRenderPassBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                info.renderPass = render_pass_;
                info.framebuffer = fbuf;
                info.renderArea.extent.width = fbuf_width_;
                info.renderArea.extent.height = fbuf_height_;
                info.clearValueCount = 1;
                info.pClearValues = &clear_value;
                vkCmdBeginRenderPass(cmdbuf, &info, VK_SUBPASS_CONTENTS_INLINE);
            }

            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdbuf);
            vkCmdEndRenderPass(cmdbuf);
        }

        void fill_imgui_info(ImGui_ImplVulkan_InitInfo& info) {
            info.RenderPass = render_pass_;
            info.DescriptorPool = desc_pool_.get();
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::DescPool desc_pool_;
        VkRenderPass render_pass_ = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> fbufs_;
        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
    };

}  // namespace


// Engine
namespace {

    class RendererVulkan : public mirinae::IRenderer {

    public:
        RendererVulkan(
            mirinae::EngineCreateInfo& cinfo,
            sung::HTaskSche task_sche,
            std::shared_ptr<mirinae::ScriptEngine> script,
            std::shared_ptr<mirinae::CosmosSimulator> cosmos,
            int init_width,
            int init_height
        )
            : device_(cinfo)
            , script_(script)
            , cosmos_(cosmos)
            , rp_res_(task_sche, device_)
            , desclayout_(device_)
            , model_man_(
                  mirinae::create_model_mgr(
                      task_sche, rp_res_.tex_man_, desclayout_, device_
                  )
              )
            , overlay_man_(
                  init_width,
                  init_height,
                  desclayout_,
                  *rp_res_.tex_man_,
                  device_
              )
            , rp_states_imgui_(device_)
            , imgui_new_frame_(cinfo.imgui_new_frame_)
            , fbuf_width_(init_width)
            , fbuf_height_(init_height) {
            framesync_.init(device_.logi_device());

            rp_res_.shadow_maps_ = mirinae::rp::create_shadow_maps_bundle(
                device_
            );

            // Render graph
            {
                const auto depth_format = device_.img_formats().depth_map();
                const auto compo_format = device_.img_formats().rgb_hdr();

                auto& rg = render_graph_;

                rg.new_img("gbuf depth")
                    .set_format(depth_format)
                    .set_type(mirinae::rg::ImageType::depth)
                    .set_count_type(mirinae::rg::ImageCountType::per_frame)
                    .set_size_rel_swhain(0.9);
                rg.new_img("gbuf albedo")
                    .set_format(VK_FORMAT_R8G8B8A8_UNORM)
                    .set_type(mirinae::rg::ImageType::color)
                    .set_count_type(mirinae::rg::ImageCountType::per_frame)
                    .set_size_rel_swhain(0.9);
                rg.new_img("gbuf normal")
                    .set_format(VK_FORMAT_R8G8B8A8_UNORM)
                    .set_type(mirinae::rg::ImageType::color)
                    .set_count_type(mirinae::rg::ImageCountType::per_frame)
                    .set_size_rel_swhain(0.9);
                rg.new_img("gbuf material")
                    .set_format(VK_FORMAT_R8G8B8A8_UNORM)
                    .set_type(mirinae::rg::ImageType::color)
                    .set_count_type(mirinae::rg::ImageCountType::per_frame)
                    .set_size_rel_swhain(0.9);
                rg.new_img("compo")
                    .set_format(compo_format)
                    .set_type(mirinae::rg::ImageType::color)
                    .set_count_type(mirinae::rg::ImageCountType::per_frame)
                    .set_size_rel_swhain(0.9);

                rg.new_pass("gbuf static")
                    .set_impl_f(mirinae::rp::create_rpimpl_gbuf_static)
                    .add_out_atta(rg.get_img("gbuf depth"))
                    .add_out_atta(rg.get_img("gbuf albedo"))
                    .add_out_atta(rg.get_img("gbuf normal"))
                    .add_out_atta(rg.get_img("gbuf material"));

                rg.new_pass("gbuf skinned")
                    .set_impl_f(mirinae::rp::create_rpimpl_gbuf_skinned)
                    .add_inout_atta(rg.get_img("gbuf depth"))
                    .add_inout_atta(rg.get_img("gbuf albedo"))
                    .add_inout_atta(rg.get_img("gbuf normal"))
                    .add_inout_atta(rg.get_img("gbuf material"));

                rg.new_pass("gbuf terrain")
                    .set_impl_f(mirinae::rp::create_rpimpl_gbuf_terrain)
                    .add_inout_atta(rg.get_img("gbuf depth"))
                    .add_inout_atta(rg.get_img("gbuf albedo"))
                    .add_inout_atta(rg.get_img("gbuf normal"))
                    .add_inout_atta(rg.get_img("gbuf material"));
            }

            // Create swapchain and its relatives
            {
                swapchain_.init(fbuf_width_, fbuf_height_, device_);

                const auto [gbuf_width, gbuf_height] = ::calc_scaled_dimensions(
                    swapchain_.width(), swapchain_.height(), 0.9
                );
                rp_res_.gbuf_.init(
                    gbuf_width, gbuf_height, *rp_res_.tex_man_, device_
                );

                mirinae::rp::gbuf::create_desc_layouts(desclayout_, device_);
                rpm_.create_std_rp(
                    *cosmos_,
                    render_graph_,
                    rp_res_,
                    desclayout_,
                    swapchain_,
                    device_
                );
                rp_.init_render_passes(
                    rp_res_.gbuf_, desclayout_, swapchain_, device_
                );
                rp_states_transp_.init(rp_res_, desclayout_, device_);
                rp_states_debug_mesh_.init(device_);
                rp_states_fillscreen_.init(desclayout_, rp_res_.gbuf_, device_);
                rp_states_imgui_.init(swapchain_);
            }

            const auto render_graph = render_graph_.build(swapchain_, device_);

            cmd_pool_.init(
                device_.graphics_queue_family_index().value(),
                device_.logi_device()
            );
            for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i)
                cmd_buf_.push_back(cmd_pool_.alloc(device_.logi_device()));

            {
                input_mgrs_.add(std::make_unique<DominantCommandProc>(device_));
                input_mgrs_.add(&overlay_man_);
            }

            // Widget: Dev console
            /*
            {
                dev_console_output_ = mirinae::create_text_blocks();
                script->replace_output_buf(dev_console_output_);

                auto w = mirinae::create_dev_console(
                    overlay_man_.text_render_data(),
                    desclayout_,
                    *tex_man_,
                    *script,
                    device_
                );
                w->replace_output_buf(dev_console_output_);
                w->hide(true);
                overlay_man_.widgets().add_widget(std::move(w));
            }
            */

            // ImGui
            {
                rp_states_imgui_.init(swapchain_);

                auto& io = ImGui::GetIO();
                io.DisplaySize.x *= cinfo.ui_scale_;
                io.DisplaySize.y *= cinfo.ui_scale_;
                io.FontGlobalScale *= cinfo.ui_scale_;

                ImGui_ImplVulkan_InitInfo init_info = {};
                device_.fill_imgui_info(init_info);
                rp_states_imgui_.fill_imgui_info(init_info);
                init_info.MinImageCount = swapchain_.views_count();
                init_info.ImageCount = swapchain_.views_count();

                if (!ImGui_ImplVulkan_Init(&init_info))
                    MIRINAE_ABORT("Failed to initialize ImGui Vulkan backend");

                if (!ImGui_ImplVulkan_CreateFontsTexture())
                    MIRINAE_ABORT("Failed to create ImGui fonts texture");
            }

            fps_timer_.set_fps_cap(120);
        }

        ~RendererVulkan() {
            device_.wait_idle();

            ImGui_ImplVulkan_Shutdown();

            auto& reg = cosmos_->reg();
            for (auto e : reg.view<mirinae::cpnt::MdlActorStatic>()) {
                auto& mactor = reg.get<mirinae::cpnt::MdlActorStatic>(e);
                mactor.model_.reset();
                mactor.actor_.reset();
            }
            for (auto& e : reg.view<mirinae::cpnt::MdlActorSkinned>()) {
                auto& mactor = reg.get<mirinae::cpnt::MdlActorSkinned>(e);
                mactor.model_.reset();
                mactor.actor_.reset();
            }
            for (auto& e : reg.view<mirinae::cpnt::Terrain>()) {
                auto& c = reg.get<mirinae::cpnt::Terrain>(e);
                c.ren_unit_.reset();
            }

            // Destroy swapchain's relatives
            {
                rpm_.destroy_std_rp();
                rp_states_imgui_.destroy();
                rp_states_fillscreen_.destroy(device_);
                rp_states_debug_mesh_.destroy(device_);
                rp_states_transp_.destroy(device_);

                rp_.destroy();
            }

            swapchain_.destroy(device_.logi_device());
            cmd_pool_.destroy(device_.logi_device());
            framesync_.destroy(device_.logi_device());
        }

        void do_frame() override {
            auto& scene = cosmos_->scene();
            auto& clock = scene.clock();
            const auto t = clock.t();
            const auto delta_time = clock.dt();

            auto& cam = cosmos_->reg().get<mirinae::cpnt::StandardCamera>(
                cosmos_->scene().main_camera_
            );
            auto& cam_view = cosmos_->reg().get<mirinae::cpnt::Transform>(
                cosmos_->scene().main_camera_
            );

            this->update_unloaded_models();

            const auto image_index_opt = this->try_acquire_image();
            if (!image_index_opt) {
                return;
            }
            const auto image_index = image_index_opt.value();

            const auto proj_mat = cam.proj_.make_proj_mat(
                swapchain_.width(), swapchain_.height()
            );
            const auto proj_inv = glm::inverse(proj_mat);
            const auto view_mat = cam_view.make_view_mat();
            const auto view_inv = glm::inverse(view_mat);

            // Update widgets
            mirinae::WidgetRenderUniData widget_ren_data;
            widget_ren_data.win_dim_ = overlay_man_.win_dim();
            widget_ren_data.frame_index_ = framesync_.get_frame_index().get();
            widget_ren_data.cmd_buf_ = VK_NULL_HANDLE;
            widget_ren_data.pipe_layout_ = VK_NULL_HANDLE;
            overlay_man_.widgets().tick(widget_ren_data);

            ren_ctxt.view_frustum_.update(proj_mat, view_mat);
            ren_ctxt.debug_ren_.clear();
            ren_ctxt.f_index_ = framesync_.get_frame_index();
            ren_ctxt.i_index_ = image_index;
            ren_ctxt.proj_mat_ = proj_mat;
            ren_ctxt.view_mat_ = view_mat;
            ren_ctxt.view_pos_ = cam_view.pos_;
            ren_ctxt.cosmos_ = cosmos_;
            ren_ctxt.cmdbuf_ = cmd_buf_.at(framesync_.get_frame_index().get());
            ren_ctxt.draw_sheet_ = std::make_shared<mirinae::DrawSheet>(
                make_draw_sheet(cosmos_->scene())
            );

            namespace cpnt = mirinae::cpnt;
            auto& reg = cosmos_->reg();
            for (auto& l : reg.view<cpnt::DLight, cpnt::Transform>()) {
                auto& light = reg.get<cpnt::DLight>(l);
                auto& tfrom = reg.get<cpnt::Transform>(l);

                tfrom.pos_ = cam_view.pos_;
                light.cascades_.update(
                    swapchain_.calc_ratio(), view_inv, cam.proj_, light, tfrom
                );
            }

            this->update_ubufs(proj_mat, view_mat);

            // Begin recording
            {
                VK_CHECK(vkResetCommandBuffer(ren_ctxt.cmdbuf_, 0));

                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = 0;
                beginInfo.pInheritanceInfo = nullptr;
                VK_CHECK(vkBeginCommandBuffer(ren_ctxt.cmdbuf_, &beginInfo));
            }

            rpm_.record_computes(ren_ctxt);

            rp_states_transp_.record_static(
                ren_ctxt.cmdbuf_,
                rp_res_.gbuf_.extent(),
                *ren_ctxt.draw_sheet_,
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rp_states_transp_.record_skinned(
                ren_ctxt.cmdbuf_,
                rp_res_.gbuf_.extent(),
                *ren_ctxt.draw_sheet_,
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rp_states_debug_mesh_.begin_record(
                ren_ctxt.cmdbuf_, rp_res_.gbuf_.extent(), image_index, rp_
            );
            for (auto& tri : ren_ctxt.debug_ren_.tri_) {
                rp_states_debug_mesh_.draw(
                    ren_ctxt.cmdbuf_,
                    tri.vertices_[0],
                    tri.vertices_[1],
                    tri.vertices_[2],
                    tri.color_,
                    rp_
                );
            }
            rp_states_debug_mesh_.end_record(
                ren_ctxt.cmdbuf_, rp_res_.gbuf_.extent(), image_index, rp_
            );
            rp_states_fillscreen_.record(
                ren_ctxt.cmdbuf_,
                swapchain_.extent(),
                cam,
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            // Shader: Overlay
            {
                auto& rp = rp_.get("overlay");

                mirinae::RenderPassBeginInfo{}
                    .rp(rp.renderpass())
                    .fbuf(rp.fbuf_at(image_index.get()))
                    .wh(swapchain_.extent())
                    .clear_value_count(rp.clear_value_count())
                    .clear_values(rp.clear_values())
                    .record_begin(ren_ctxt.cmdbuf_);

                vkCmdBindPipeline(
                    ren_ctxt.cmdbuf_,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    rp.pipeline()
                );

                mirinae::Viewport{}
                    .set_wh(swapchain_.width(), swapchain_.height())
                    .record_single(ren_ctxt.cmdbuf_);
                mirinae::Rect2D{}
                    .set_wh(swapchain_.width(), swapchain_.height())
                    .record_scissor(ren_ctxt.cmdbuf_);

                widget_ren_data.cmd_buf_ = ren_ctxt.cmdbuf_;
                widget_ren_data.pipe_layout_ = rp.pipeline_layout();
                overlay_man_.record_render(widget_ren_data);

                vkCmdEndRenderPass(ren_ctxt.cmdbuf_);
            }

            // ImGui
            {
                ImGui_ImplVulkan_NewFrame();
                imgui_new_frame_();
                ImGui::NewFrame();
                // ImGui::ShowDemoWindow();

                for (auto& w : cosmos_->imgui_) {
                    w->render();
                }

                ImGui::Render();
                rp_states_imgui_.record(ren_ctxt);
            }

            VK_CHECK(vkEndCommandBuffer(ren_ctxt.cmdbuf_));

            // Submit and present
            {
                const VkSemaphore signal_semaph =
                    framesync_.get_cur_render_fin_semaph().get();

                mirinae::SubmitInfo{}
                    .add_wait_semaph_color_attach_out(
                        framesync_.get_cur_img_ava_semaph().get()
                    )
                    .add_signal_semaph(signal_semaph)
                    .add_cmdbuf(ren_ctxt.cmdbuf_)
                    .queue_submit_single(
                        device_.graphics_queue(),
                        framesync_.get_cur_in_flight_fence().get()
                    );

                mirinae::PresentInfo{}
                    .add_wait_semaph(signal_semaph)
                    .add_swapchain(swapchain_.get())
                    .add_image_index(image_index.get())
                    .queue_present(device_.present_queue());
            }

            framesync_.increase_frame_index();
        }

        void notify_window_resize(uint32_t width, uint32_t height) override {
            fbuf_width_ = width;
            fbuf_height_ = height;
            fbuf_resized_ = true;
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            if (input_mgrs_.on_key_event(e))
                return true;

            return false;
        }

        bool on_text_event(char32_t c) override {
            if (input_mgrs_.on_text_event(c))
                return true;

            return false;
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            if (input_mgrs_.on_mouse_event(e))
                return true;

            return false;
        }

    private:
        void resize_swapchain(uint32_t width, uint32_t height) {
            device_.wait_idle();
            swapchain_.init(width, height, device_);

            const auto [gbuf_width, gbuf_height] = ::calc_scaled_dimensions(
                swapchain_.width(), swapchain_.height(), 0.9
            );

            // Destroy
            {
                rpm_.destroy_std_rp();
                // rp_states_imgui_.destroy();
                rp_states_fillscreen_.destroy(device_);
                rp_states_debug_mesh_.destroy(device_);
                rp_states_transp_.destroy(device_);

                rp_.destroy();
            }

            // Create
            {
                swapchain_.init(fbuf_width_, fbuf_height_, device_);

                const auto [gbuf_width, gbuf_height] = ::calc_scaled_dimensions(
                    swapchain_.width(), swapchain_.height(), 0.9
                );
                rp_res_.gbuf_.init(
                    gbuf_width, gbuf_height, *rp_res_.tex_man_, device_
                );

                mirinae::rp::gbuf::create_desc_layouts(desclayout_, device_);
                rpm_.create_std_rp(
                    *cosmos_,
                    render_graph_,
                    rp_res_,
                    desclayout_,
                    swapchain_,
                    device_
                );
                rp_.init_render_passes(
                    rp_res_.gbuf_, desclayout_, swapchain_, device_
                );
                rp_states_transp_.init(rp_res_, desclayout_, device_);
                rp_states_debug_mesh_.init(device_);
                rp_states_fillscreen_.init(desclayout_, rp_res_.gbuf_, device_);
            }

            // Optimized resize
            {
                rp_states_imgui_.on_swchain_resize(swapchain_);
            }
        }

        std::optional<mirinae::ShainImageIndex> try_acquire_image() {
            framesync_.get_cur_in_flight_fence().wait(device_.logi_device());

            if (fbuf_resized_) {
                if (::is_fbuf_too_small(fbuf_width_, fbuf_height_)) {
                    fbuf_resized_ = true;
                } else {
                    fbuf_resized_ = false;
                    this->resize_swapchain(fbuf_width_, fbuf_height_);
                    overlay_man_.on_fbuf_resize(fbuf_width_, fbuf_height_);
                }
                return std::nullopt;
            }

            const auto image_index_opt = swapchain_.acquire_next_image(
                framesync_.get_cur_img_ava_semaph().get(), device_.logi_device()
            );
            if (!image_index_opt) {
                if (::is_fbuf_too_small(fbuf_width_, fbuf_height_)) {
                    fbuf_resized_ = true;
                } else {
                    fbuf_resized_ = false;
                    this->resize_swapchain(fbuf_width_, fbuf_height_);
                }
                return std::nullopt;
            }

            framesync_.get_cur_in_flight_fence().reset(device_.logi_device());
            return image_index_opt.value();
        }

        void update_unloaded_models() {
            namespace cpnt = mirinae::cpnt;

            auto& scene = cosmos_->scene();
            auto& reg = cosmos_->reg();

            for (auto e : reg.view<cpnt::MdlActorStatic>()) {
                auto& mactor = reg.get<cpnt::MdlActorStatic>(e);

                if (!mactor.model_) {
                    const auto mdl_path = mactor.model_path_;
                    const auto res = model_man_->request_static(mdl_path);
                    if (dal::ReqResult::loading == res) {
                        continue;
                    } else if (dal::ReqResult::ready != res) {
                        const auto path_str = mdl_path.u8string();
                        SPDLOG_WARN("Failed to get model: {}", path_str);
                        reg.destroy(e);
                        continue;
                    }

                    mactor.model_ = model_man_->get_static(mdl_path);
                    if (!mactor.model_) {
                        const auto path_str = mdl_path.u8string();
                        SPDLOG_WARN("Failed to get model: {}", path_str);
                        reg.destroy(e);
                        continue;
                    }
                }

                if (!mactor.actor_) {
                    auto a = std::make_shared<mirinae::RenderActor>(device_);
                    a->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout_);
                    mactor.actor_ = a;
                }
            }

            for (auto e : reg.view<cpnt::MdlActorSkinned>()) {
                auto& mactor = reg.get<cpnt::MdlActorSkinned>(e);

                if (!mactor.model_) {
                    const auto mdl_path = mactor.model_path_;
                    const auto res = model_man_->request_skinned(mdl_path);
                    if (dal::ReqResult::loading == res) {
                        continue;
                    } else if (dal::ReqResult::ready != res) {
                        const auto path_str = mdl_path.u8string();
                        SPDLOG_WARN("Failed to get model: {}", path_str);
                        reg.destroy(e);
                        continue;
                    }

                    auto model = model_man_->get_skinned(mdl_path);
                    if (!model) {
                        const auto path_str = mdl_path.u8string();
                        SPDLOG_WARN("Failed to get model: {}", path_str);
                        reg.destroy(e);
                        continue;
                    }

                    mactor.model_ = model;
                    mactor.anim_state_.set_skel_anim(model->skel_anim_);
                }

                if (!mactor.actor_) {
                    auto a = std::make_shared<mirinae::RenderActorSkinned>(
                        device_
                    );
                    a->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout_);
                    mactor.actor_ = a;
                }
            }

            scene.entt_without_model_.clear();
        }

        void update_ubufs(
            const glm::dmat4& proj_mat, const glm::dmat4& view_mat
        ) {
            namespace cpnt = mirinae::cpnt;
            auto& clock = cosmos_->scene().clock();

            auto& scene = cosmos_->scene();
            auto& reg = cosmos_->reg();

            // Update ubuf: U_GbufActor
            for (const auto e : reg.view<cpnt::MdlActorStatic>()) {
                auto& mactor = reg.get<cpnt::MdlActorStatic>(e);
                auto actor = mactor.get_actor<mirinae::RenderActor>();
                if (!actor)
                    continue;

                glm::dmat4 model_mat(1);
                if (auto tform = reg.try_get<cpnt::Transform>(e))
                    model_mat = tform->make_model_mat();
                const auto vm = view_mat * model_mat;
                const auto pvm = proj_mat * vm;

                mirinae::U_GbufActor udata;
                udata.model = model_mat;
                udata.view_model = vm;
                udata.pvm = pvm;

                actor->udpate_ubuf(
                    framesync_.get_frame_index().get(),
                    udata,
                    device_.mem_alloc()
                );
            }

            // Update ubuf: U_GbufActorSkinned
            for (const auto e : reg.view<cpnt::MdlActorSkinned>()) {
                auto& mactor = reg.get<cpnt::MdlActorSkinned>(e);
                auto actor = mactor.get_actor<mirinae::RenderActorSkinned>();
                if (!actor)
                    continue;

                mactor.anim_state_.update_tick(clock);

                glm::dmat4 model_mat(1);
                if (auto tform = reg.try_get<cpnt::Transform>(e))
                    model_mat = tform->make_model_mat();
                const auto vm = view_mat * model_mat;
                const auto pvm = proj_mat * vm;

                mirinae::U_GbufActorSkinned udata;
                udata.view_model = vm;
                udata.pvm = pvm;

                mactor.anim_state_.sample_anim(
                    udata.joint_transforms_, mirinae::MAX_JOINTS, clock
                );

                actor->udpate_ubuf(
                    framesync_.get_frame_index().get(),
                    udata,
                    device_.mem_alloc()
                );
            }

            // Update ubuf: U_CompoMain
            {
                mirinae::U_CompoMain ubuf_data;
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

                for (auto e : cosmos_->reg().view<cpnt::AtmosphereSimple>()) {
                    const auto& atm =
                        cosmos_->reg().get<cpnt::AtmosphereSimple>(e);
                    ubuf_data.set_fog_color(atm.fog_color_)
                        .set_fog_density(atm.fog_density_);
                    break;
                }

                rp_states_transp_.ubufs_.at(framesync_.get_frame_index().get())
                    .set_data(ubuf_data, device_.mem_alloc());
            }
        }

        // This must be the first member variable
        mirinae::VulkanDevice device_;
        std::shared_ptr<mirinae::ScriptEngine> script_;
        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;

        mirinae::rg::RenderGraphDef render_graph_;
        mirinae::RpResources rp_res_;
        mirinae::DesclayoutManager desclayout_;
        mirinae::HMdlMgr model_man_;
        mirinae::OverlayManager overlay_man_;
        mirinae::RenderPassPackage rp_;
        ::RpMasters rpm_;
        ::RpStatesTransp rp_states_transp_;
        ::RpStatesDebugMesh rp_states_debug_mesh_;
        ::RpStatesFillscreen rp_states_fillscreen_;
        ::RpStatesImgui rp_states_imgui_;
        mirinae::Swapchain swapchain_;
        ::FrameSync framesync_;
        mirinae::RpContext ren_ctxt;
        mirinae::CommandPool cmd_pool_;
        std::vector<VkCommandBuffer> cmd_buf_;
        mirinae::InputProcesserMgr input_mgrs_;
        dal::TimerThatCaps fps_timer_;
        std::shared_ptr<mirinae::ITextData> dev_console_output_;
        std::function<void()> imgui_new_frame_;

        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
        bool fbuf_resized_ = false;
        bool flashlight_on_ = false;
        bool quit_ = false;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IRenderer> create_vk_renderer(
        mirinae::EngineCreateInfo& cinfo,
        sung::HTaskSche task_sche,
        std::shared_ptr<mirinae::ScriptEngine> script,
        std::shared_ptr<mirinae::CosmosSimulator> cosmos
    ) {
        const auto w = cinfo.init_width_;
        const auto h = cinfo.init_height_;
        return std::make_unique<::RendererVulkan>(
            cinfo, task_sche, script, cosmos, w, h
        );
    }

}  // namespace mirinae
