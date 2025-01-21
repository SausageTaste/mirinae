#include "mirinae/renderer.hpp"

#include <daltools/common/util.h>
#include <sung/basic/time.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lightweight/script.hpp"
#include "mirinae/math/mamath.hpp"
#include "mirinae/overlay/overlay.hpp"
#include "mirinae/render/cmdbuf.hpp"
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
        RpMasters() {
            gbuf_basic_ = mirinae::rp::gbuf::create_rpm_basic();
            gbuf_terrain_ = mirinae::rp::gbuf::create_rpm_terrain();
            envmap_ = mirinae::rp::envmap::create_rp_master();
        }

        void create_std_rp(
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::FbufImageBundle& fbuf_bundle,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        ) {
            this->destroy_std_rp();

            rp_states_.push_back(
                mirinae::rp::ocean::create_rp_states_ocean_tilde_h(
                    rp_res, desclayouts, device
                )
            );

            rp_states_.push_back(
                mirinae::rp::ocean::create_rp_states_ocean_tilde_hkt(
                    rp_res, desclayouts, device
                )
            );

            rp_states_.push_back(
                mirinae::rp::ocean::create_rp_states_ocean_butterfly(
                    rp_res, desclayouts, device
                )
            );

            rp_states_.push_back(
                mirinae::rp::ocean::create_rp_states_ocean_finalize(
                    rp_res, desclayouts, device
                )
            );

            rp_states_.push_back(
                mirinae::rp::ocean::create_rp_states_ocean_tess(
                    swapchain.views_count(),
                    fbuf_bundle,
                    rp_res,
                    desclayouts,
                    device
                )
            );
            ocean_tess_ = rp_states_.back().get();
        }

        void destroy_std_rp() {
            rp_states_.clear();
            ocean_tess_ = nullptr;
        }

        mirinae::rp::gbuf::IRpMasterBasic& gbuf_basic() { return *gbuf_basic_; }
        mirinae::rp::gbuf::IRpMasterTerrain& gbuf_terrain() {
            return *gbuf_terrain_;
        }
        mirinae::rp::envmap::IRpMaster& envmap() { return *envmap_; }
        mirinae::rp::shadow::RpMaster& shadow() { return shadow_; }
        mirinae::rp::compo::RpMasterBasic& compo_basic() {
            return compo_basic_;
        }
        mirinae::rp::compo::RpMasterSky& compo_sky() { return compo_sky_; }

        mirinae::rp::ocean::IRpStates& ocean_tess() { return *ocean_tess_; }

        void record_computes(const mirinae::rp::ocean::RpContext& ctxt) {
            for (auto& rp : rp_states_) {
                if (rp.get() == ocean_tess_)
                    continue;
                rp->record(ctxt);
            }
        }

    private:
        std::unique_ptr<mirinae::rp::gbuf::IRpMasterBasic> gbuf_basic_;
        std::unique_ptr<mirinae::rp::gbuf::IRpMasterTerrain> gbuf_terrain_;
        std::unique_ptr<mirinae::rp::envmap::IRpMaster> envmap_;
        mirinae::rp::shadow::RpMaster shadow_;
        mirinae::rp::compo::RpMasterBasic compo_basic_;
        mirinae::rp::compo::RpMasterSky compo_sky_;

        std::vector<mirinae::rp::ocean::URpStates> rp_states_;
        mirinae::rp::ocean::IRpStates* ocean_tess_ = nullptr;
    };


    mirinae::DrawSheet make_draw_sheet(mirinae::Scene& scene) {
        using CTrans = mirinae::cpnt::Transform;
        using CStaticModelActor = mirinae::cpnt::StaticActorVk;
        using CSkinnedModelActor = mirinae::cpnt::SkinnedActorVk;

        mirinae::DrawSheet sheet;

        for (auto enttid : scene.reg_.view<CTrans, CStaticModelActor>()) {
            auto& pair = scene.reg_.get<CStaticModelActor>(enttid);
            auto& trans = scene.reg_.get<CTrans>(enttid);

            auto& dst = sheet.get_static_pair(*pair.model_);
            auto& actor = dst.actors_.emplace_back();
            actor.actor_ = pair.actor_.get();
            actor.model_mat_ = trans.make_model_mat();
        }

        for (auto& enttid : scene.reg_.view<CTrans, CSkinnedModelActor>()) {
            auto& pair = scene.reg_.get<CSkinnedModelActor>(enttid);
            auto& trans = scene.reg_.get<CTrans>(enttid);

            auto& dst = sheet.get_skinn_pair(*pair.model_);
            auto& actor = dst.actors_.emplace_back();
            actor.actor_ = pair.actor_.get();
            actor.model_mat_ = trans.make_model_mat();
        }

        sheet.ocean_ = nullptr;
        for (auto& eid : scene.reg_.view<mirinae::cpnt::Ocean>()) {
            sheet.ocean_ = &scene.reg_.get<mirinae::cpnt::Ocean>(eid);
            // Only one ocean is allowed
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
            mirinae::DesclayoutManager& desclayouts,
            VkImageView dlight_shadowmap,
            VkImageView slight_shadowmap,
            VkImageView env_diffuse,
            VkImageView env_specular,
            VkImageView env_lut,
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

            mirinae::DescWriteInfoBuilder builder;
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& ubuf = ubufs_.emplace_back();
                ubuf.init_ubuf(
                    sizeof(mirinae::U_TranspFrame), device.mem_alloc()
                );

                builder.set_descset(desc_sets_.at(i))
                    .add_ubuf(ubuf)
                    .add_img_sampler(dlight_shadowmap, sam_nea)
                    .add_img_sampler(slight_shadowmap, sam_nea)
                    .add_img_sampler(env_diffuse, sam_cube)
                    .add_img_sampler(env_specular, sam_cube)
                    .add_img_sampler(env_lut, sam_lin);
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

            for (auto& pair : draw_sheet.static_pairs_) {
                for (auto& unit : pair.model_->render_units_alpha_) {
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

            for (auto& pair : draw_sheet.skinned_pairs_) {
                for (auto& unit : pair.model_->runits_alpha_) {
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
            const glm::vec3& p0,
            const glm::vec3& p1,
            const glm::vec3& p2,
            const glm::vec4& color,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            mirinae::U_DebugMeshPushConst pc;
            pc.vertices_[0] = glm::vec4(p0, 1);
            pc.vertices_[1] = glm::vec4(p1, 1);
            pc.vertices_[2] = glm::vec4(p2, 1);
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
            VkCommandBuffer cmdbuf,
            VkExtent2D shain_exd,
            mirinae::FrameIndex frame_index,
            mirinae::ShainImageIndex image_index,
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

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);

            vkCmdEndRenderPass(cmdbuf);
        }

        mirinae::DescPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
    };

}  // namespace


// Engine
namespace {

    class RendererVulkan : public mirinae::IRenderer {

    public:
        RendererVulkan(
            mirinae::EngineCreateInfo&& cinfo,
            sung::HTaskSche task_sche,
            std::shared_ptr<mirinae::ScriptEngine> script,
            std::shared_ptr<mirinae::CosmosSimulator> cosmos,
            int init_width,
            int init_height
        )
            : device_(std::move(cinfo))
            , script_(script)
            , cosmos_(cosmos)
            , rp_res_(device_)
            , desclayout_(device_)
            , tex_man_(mirinae::create_tex_mgr(task_sche, device_))
            , model_man_(mirinae::create_model_mgr(
                  task_sche, tex_man_, desclayout_, device_
              ))
            , overlay_man_(
                  init_width, init_height, desclayout_, *tex_man_, device_
              )
            , fbuf_width_(init_width)
            , fbuf_height_(init_height) {
            // This must be the first member variable right after vtable pointer
            static_assert(offsetof(RendererVulkan, device_) == sizeof(void*));

            framesync_.init(device_.logi_device());

            rpm_.shadow().pool().add(4096, 4096, device_);
            rpm_.shadow().pool().add(256, 256, device_);

            this->create_swapchain_and_relatives(fbuf_width_, fbuf_height_);

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

            fps_timer_.set_fps_cap(120);
        }

        ~RendererVulkan() {
            device_.wait_idle();

            auto& reg = cosmos_->reg();
            for (auto enttid : reg.view<mirinae::cpnt::StaticActorVk>())
                reg.remove<mirinae::cpnt::StaticActorVk>(enttid);
            for (auto& enttid : reg.view<mirinae::cpnt::SkinnedActorVk>())
                reg.remove<mirinae::cpnt::SkinnedActorVk>(enttid);

            cmd_pool_.destroy(device_.logi_device());
            this->destroy_swapchain_and_relatives();
            framesync_.destroy(device_.logi_device());
        }

        void do_frame() override {
            const auto t = cosmos_->ftime().tp_;
            const auto delta_time = cosmos_->ftime().dt_;

            auto& cam = cosmos_->reg().get<mirinae::cpnt::StandardCamera>(
                cosmos_->scene().main_camera_
            );

            this->update_unloaded_models();

            const auto image_index_opt = this->try_acquire_image();
            if (!image_index_opt) {
                return;
            }
            const auto image_index = image_index_opt.value();

            const auto aspect_ratio = (double)swapchain_.width() /
                                      (double)swapchain_.height();
            const auto proj_mat = cam.proj_.make_proj_mat(
                swapchain_.width(), swapchain_.height()
            );
            const auto proj_inv = glm::inverse(proj_mat);
            const auto view_mat = cam.view_.make_view_mat();
            const auto view_inv = glm::inverse(view_mat);

            // Update widgets
            mirinae::WidgetRenderUniData widget_ren_data;
            widget_ren_data.win_dim_ = overlay_man_.win_dim();
            widget_ren_data.frame_index_ = framesync_.get_frame_index().get();
            widget_ren_data.cmd_buf_ = VK_NULL_HANDLE;
            widget_ren_data.pipe_layout_ = VK_NULL_HANDLE;
            overlay_man_.widgets().tick(widget_ren_data);

            mirinae::rp::ocean::RpContext ren_ctxt;
            ren_ctxt.f_index_ = framesync_.get_frame_index();
            ren_ctxt.i_index_ = image_index;
            ren_ctxt.proj_mat_ = proj_mat;
            ren_ctxt.view_mat_ = view_mat;
            ren_ctxt.cmdbuf_ = cmd_buf_.at(framesync_.get_frame_index().get());
            ren_ctxt.draw_sheet_ = std::make_shared<mirinae::DrawSheet>(
                make_draw_sheet(cosmos_->scene())
            );

            for (auto& l : cosmos_->reg().view<mirinae::cpnt::DLight>()) {
                auto& dlight = cosmos_->reg().get<mirinae::cpnt::DLight>(l);
                dlight.transform_.pos_ = cam.view_.pos_;

                rpm_.shadow().cascade().update(
                    swapchain_.calc_ratio(), view_inv, cam.proj_, dlight
                );
                rpm_.shadow().pool().at(0).mat_ =
                    rpm_.shadow().cascade().cascades_.front().light_mat_;

                break;
            }

            for (auto& l : cosmos_->reg().view<mirinae::cpnt::SLight>()) {
                auto& slight = cosmos_->reg().get<mirinae::cpnt::SLight>(l);
                slight.transform_.pos_ = cam.view_.pos_ +
                                         glm::dvec3{ 0, -0.1, 0 };
                slight.transform_.rot_ = cam.view_.rot_;
                slight.transform_.rotate(
                    sung::TAngle<double>::from_deg(std::atan(0.1 / 5.0)),
                    cam.view_.make_right_dir()
                );

                rpm_.shadow().pool().at(1).mat_ = slight.make_light_mat();
                break;
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

                std::array<VkClearValue, 3> clear_values;
                clear_values[0].depthStencil = { 1.f, 0 };
                clear_values[1].color = { 0.f, 0.f, 0.f, 1.f };
                clear_values[2].color = { 0.f, 0.f, 0.f, 1.f };
            }

            rpm_.record_computes(ren_ctxt);

            rpm_.envmap().record(
                ren_ctxt.cmdbuf_,
                *ren_ctxt.draw_sheet_,
                framesync_.get_frame_index(),
                *cosmos_,
                image_index,
                rp_
            );

            rpm_.shadow().record(
                ren_ctxt.cmdbuf_,
                *ren_ctxt.draw_sheet_,
                framesync_.get_frame_index(),
                rp_
            );

            rpm_.gbuf_basic().record(
                ren_ctxt.cmdbuf_,
                fbuf_images_.extent(),
                *ren_ctxt.draw_sheet_,
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rpm_.gbuf_terrain().record(
                ren_ctxt.cmdbuf_,
                proj_mat,
                view_mat,
                fbuf_images_.extent(),
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rpm_.compo_basic().record(
                ren_ctxt.cmdbuf_,
                fbuf_images_.extent(),
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rpm_.compo_sky().record(
                ren_ctxt.cmdbuf_,
                proj_inv,
                view_inv,
                fbuf_images_.extent(),
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rpm_.ocean_tess().record(ren_ctxt);

            rp_states_transp_.record_static(
                ren_ctxt.cmdbuf_,
                fbuf_images_.extent(),
                *ren_ctxt.draw_sheet_,
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rp_states_transp_.record_skinned(
                ren_ctxt.cmdbuf_,
                fbuf_images_.extent(),
                *ren_ctxt.draw_sheet_,
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rp_states_debug_mesh_.begin_record(
                ren_ctxt.cmdbuf_, fbuf_images_.extent(), image_index, rp_
            );
            rp_states_debug_mesh_.end_record(
                ren_ctxt.cmdbuf_, fbuf_images_.extent(), image_index, rp_
            );

            rp_states_fillscreen_.record(
                ren_ctxt.cmdbuf_,
                swapchain_.extent(),
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
        void create_swapchain_and_relatives(
            uint32_t fbuf_width, uint32_t fbuf_height
        ) {
            device_.wait_idle();
            swapchain_.init(fbuf_width, fbuf_height, device_);

            const auto [gbuf_width, gbuf_height] = ::calc_scaled_dimensions(
                swapchain_.width(), swapchain_.height(), 0.9
            );
            fbuf_images_.init(gbuf_width, gbuf_height, *tex_man_, device_);

            mirinae::rp::gbuf::create_rp(
                rp_,
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );
            mirinae::rp::envmap::create_rp(rp_, desclayout_, device_);
            mirinae::rp::shadow::create_rp(
                rp_, fbuf_images_.depth().format(), desclayout_, device_
            );
            mirinae::rp::compo::create_rp(
                rp_,
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );
            rp_.init_render_passes(
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );

            rpm_.shadow().pool().recreate_fbufs(rp_.get("shadowmap"), device_);

            rpm_.envmap().init(rp_, *tex_man_, desclayout_, device_);
            rpm_.gbuf_basic().init();
            rpm_.gbuf_terrain().init(*tex_man_, desclayout_, device_);
            rpm_.compo_basic().init(
                desclayout_,
                fbuf_images_,
                rpm_.shadow().pool().get_img_view_at(0),
                rpm_.shadow().pool().get_img_view_at(1),
                rpm_.envmap().diffuse_view(0),
                rpm_.envmap().specular_view(0),
                rpm_.envmap().brdf_lut_view(),
                device_
            );
            rpm_.compo_sky().init(
                rpm_.envmap().sky_tex_view(),
                rp_,
                desclayout_,
                fbuf_images_,
                swapchain_,
                device_
            );
            rp_states_transp_.init(
                desclayout_,
                rpm_.shadow().pool().get_img_view_at(0),
                rpm_.shadow().pool().get_img_view_at(1),
                rpm_.envmap().diffuse_view(0),
                rpm_.envmap().specular_view(0),
                rpm_.envmap().brdf_lut_view(),
                device_
            );
            rp_states_debug_mesh_.init(device_);
            rp_states_fillscreen_.init(desclayout_, fbuf_images_, device_);

            rpm_.create_std_rp(
                rp_res_, desclayout_, fbuf_images_, swapchain_, device_
            );
        }

        void destroy_swapchain_and_relatives() {
            device_.wait_idle();

            rpm_.shadow().pool().destroy_fbufs(device_);

            rpm_.destroy_std_rp();
            rp_states_fillscreen_.destroy(device_);
            rp_states_debug_mesh_.destroy(device_);
            rp_states_transp_.destroy(device_);
            rpm_.compo_sky().destroy(device_);
            rpm_.compo_basic().destroy(device_);
            rpm_.gbuf_terrain().destroy(device_);
            rpm_.gbuf_basic().destroy(device_);
            rpm_.envmap().destroy(device_);

            rp_.destroy();
            swapchain_.destroy(device_.logi_device());
        }

        std::optional<mirinae::ShainImageIndex> try_acquire_image() {
            framesync_.get_cur_in_flight_fence().wait(device_.logi_device());

            if (fbuf_resized_) {
                if (::is_fbuf_too_small(fbuf_width_, fbuf_height_)) {
                    fbuf_resized_ = true;
                } else {
                    fbuf_resized_ = false;
                    this->destroy_swapchain_and_relatives();
                    this->create_swapchain_and_relatives(
                        fbuf_width_, fbuf_height_
                    );
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
                    this->destroy_swapchain_and_relatives();
                    this->create_swapchain_and_relatives(
                        fbuf_width_, fbuf_height_
                    );
                }
                return std::nullopt;
            }

            framesync_.get_cur_in_flight_fence().reset(device_.logi_device());
            return image_index_opt.value();
        }

        void update_unloaded_models() {
            namespace cpnt = mirinae::cpnt;
            using SrcSkinn = cpnt::SkinnedModelActor;
            using mirinae::RenderActorSkinned;

            auto& scene = cosmos_->scene();
            auto& reg = cosmos_->reg();

            for (auto e : reg.view<cpnt::StaticModelActor>()) {
                auto mavk = reg.try_get<cpnt::StaticActorVk>(e);
                if (mavk)
                    continue;

                const auto& moac = reg.get<cpnt::StaticModelActor>(e);
                const auto res_result = model_man_->request_static(
                    moac.model_path_
                );
                if (dal::ReqResult::loading == res_result)
                    continue;
                else if (dal::ReqResult::ready != res_result) {
                    SPDLOG_WARN(
                        "Failed to get model: {}", moac.model_path_.u8string()
                    );
                    reg.destroy(e);
                    continue;
                }

                auto model = model_man_->get_static(moac.model_path_);
                if (!model)
                    continue;

                mavk = &reg.emplace<cpnt::StaticActorVk>(e);
                mavk->model_ = model;
                mavk->actor_ = std::make_shared<mirinae::RenderActor>(device_);
                mavk->actor_->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout_);
                return;
            }

            for (auto e : reg.view<cpnt::SkinnedModelActor>()) {
                auto mavk = reg.try_get<cpnt::SkinnedActorVk>(e);
                if (mavk)
                    continue;

                auto& moac = reg.get<cpnt::SkinnedModelActor>(e);
                const auto res_result = model_man_->request_skinned(
                    moac.model_path_
                );
                if (dal::ReqResult::loading == res_result)
                    continue;
                else if (dal::ReqResult::ready != res_result) {
                    SPDLOG_WARN(
                        "Failed to get model: {}", moac.model_path_.u8string()
                    );
                    reg.destroy(e);
                    continue;
                }

                auto model = model_man_->get_skinned(moac.model_path_);
                if (!model)
                    continue;

                mavk = &reg.emplace<cpnt::SkinnedActorVk>(e);
                mavk->model_ = model;
                mavk->actor_ = std::make_shared<mirinae::RenderActorSkinned>(
                    device_
                );
                mavk->actor_->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout_);
                moac.anim_state_.set_skel_anim(model->skel_anim_);
                return;
            }

            scene.entt_without_model_.clear();
        }

        void update_ubufs(
            const glm::dmat4& proj_mat, const glm::dmat4& view_mat
        ) {
            namespace cpnt = mirinae::cpnt;
            const auto t = cosmos_->ftime().tp_;

            auto& scene = cosmos_->scene();
            auto& reg = cosmos_->reg();

            // Update ubuf: U_GbufActor
            reg.view<cpnt::Transform, cpnt::StaticActorVk>().each(
                [&](auto enttid, auto& transform, auto& ren_pair) {
                    const auto model_mat = transform.make_model_mat();

                    mirinae::U_GbufActor ubuf_data;
                    ubuf_data.model = model_mat;
                    ubuf_data.view_model = view_mat * model_mat;
                    ubuf_data.pvm = proj_mat * view_mat * model_mat;

                    ren_pair.actor_->udpate_ubuf(
                        framesync_.get_frame_index().get(),
                        ubuf_data,
                        device_.mem_alloc()
                    );
                }
            );

            // Update ubuf: U_GbufActorSkinned
            reg.view<
                   cpnt::Transform,
                   cpnt::SkinnedActorVk,
                   cpnt::SkinnedModelActor>()
                .each([&](auto enttid,
                          auto& transform,
                          auto& ren_pair,
                          auto& mactor) {
                    const auto model_m = transform.make_model_mat();
                    mactor.anim_state_.update_tick(cosmos_->ftime());

                    mirinae::U_GbufActorSkinned ubuf_data;
                    mactor.anim_state_.sample_anim(
                        ubuf_data.joint_transforms_,
                        mirinae::MAX_JOINTS,
                        cosmos_->ftime()
                    );
                    ubuf_data.view_model = view_mat * model_m;
                    ubuf_data.pvm = proj_mat * view_mat * model_m;

                    ren_pair.actor_->udpate_ubuf(
                        framesync_.get_frame_index().get(),
                        ubuf_data,
                        device_.mem_alloc()
                    );
                });

            // Update ubuf: U_CompoMain
            {
                mirinae::U_CompoMain ubuf_data;
                ubuf_data.set_proj(proj_mat).set_view(view_mat);

                for (auto e : cosmos_->reg().view<cpnt::DLight>()) {
                    const auto& light = cosmos_->reg().get<cpnt::DLight>(e);
                    const auto& cascade = rpm_.shadow().cascade();
                    const auto& cascades = cascade.cascades_;

                    for (size_t i = 0; i < cascades.size(); ++i)
                        ubuf_data.set_dlight_mat(i, cascades.at(i).light_mat_);

                    ubuf_data.set_dlight_dir(light.calc_to_light_dir(view_mat))
                        .set_dlight_color(light.color_)
                        .set_dlight_cascade_depths(cascade.far_depths_);
                    break;
                }

                for (auto e : cosmos_->reg().view<cpnt::SLight>()) {
                    const auto& l = cosmos_->reg().get<cpnt::SLight>(e);
                    ubuf_data.set_slight_mat(l.make_light_mat())
                        .set_slight_pos(l.calc_view_space_pos(view_mat))
                        .set_slight_dir(l.calc_to_light_dir(view_mat))
                        .set_slight_color(l.color_)
                        .set_slight_inner_angle(l.inner_angle_)
                        .set_slight_outer_angle(l.outer_angle_)
                        .set_slight_max_dist(l.max_distance_);
                    break;
                }

                rpm_.compo_basic()
                    .ubufs_.at(framesync_.get_frame_index().get())
                    .set_data(ubuf_data, device_.mem_alloc());
                rp_states_transp_.ubufs_.at(framesync_.get_frame_index().get())
                    .set_data(ubuf_data, device_.mem_alloc());
            }
        }

        // This must be the first member variable
        mirinae::VulkanDevice device_;
        std::shared_ptr<mirinae::ScriptEngine> script_;
        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;

        mirinae::RpResources rp_res_;
        mirinae::DesclayoutManager desclayout_;
        mirinae::HTexMgr tex_man_;
        mirinae::HMdlMgr model_man_;
        mirinae::FbufImageBundle fbuf_images_;
        mirinae::OverlayManager overlay_man_;
        mirinae::RenderPassPackage rp_;
        ::RpMasters rpm_;
        ::RpStatesTransp rp_states_transp_;
        ::RpStatesDebugMesh rp_states_debug_mesh_;
        ::RpStatesFillscreen rp_states_fillscreen_;
        mirinae::Swapchain swapchain_;
        ::FrameSync framesync_;
        mirinae::CommandPool cmd_pool_;
        std::vector<VkCommandBuffer> cmd_buf_;
        mirinae::InputProcesserMgr input_mgrs_;
        dal::TimerThatCaps fps_timer_;
        std::shared_ptr<mirinae::ITextData> dev_console_output_;

        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
        bool fbuf_resized_ = false;
        bool flashlight_on_ = false;
        bool quit_ = false;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IRenderer> create_vk_renderer(
        mirinae::EngineCreateInfo&& cinfo,
        sung::HTaskSche task_sche,
        std::shared_ptr<mirinae::ScriptEngine> script,
        std::shared_ptr<mirinae::CosmosSimulator> cosmos
    ) {
        const auto w = cinfo.init_width_;
        const auto h = cinfo.init_height_;
        return std::make_unique<::RendererVulkan>(
            std::move(cinfo), task_sche, script, cosmos, w, h
        );
    }

}  // namespace mirinae
