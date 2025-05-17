#include "mirinae/vulkan_pch.h"

#include "mirinae/renderer.hpp"

#include <SDL3/SDL_scancode.h>
#include <daltools/common/util.h>
#include <entt/entity/registry.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/camera.hpp"
#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/script.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/math/mamath.hpp"
#include "mirinae/overlay/overlay.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/draw_set.hpp"
#include "mirinae/render/platform_func.hpp"
#include "mirinae/render/render_graph.hpp"
#include "mirinae/render/renderpass.hpp"
#include "mirinae/renderpass/atmos/sky.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/compo.hpp"
#include "mirinae/renderpass/envmap/envmap.hpp"
#include "mirinae/renderpass/gbuf/gbuf.hpp"
#include "mirinae/renderpass/misc/misc.hpp"
#include "mirinae/renderpass/ocean/ocean.hpp"
#include "mirinae/renderpass/shadow/shadow.hpp"
#include "mirinae/renderpass/transp/transp.hpp"


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
    std::pair<uint32_t, uint32_t> calc_scaled_dimensions(T w, T h) {
        return std::make_pair(
            static_cast<uint32_t>(static_cast<double>(w) * 1.25),
            static_cast<uint32_t>(static_cast<double>(h) * 1.25)
        );
    }


    class DominantCommandProc : public mirinae::IInputProcessor {

    public:
        DominantCommandProc(mirinae::VulkanDevice& device) : device_(device) {}

        bool on_key_event(const mirinae::key::Event& e) override {
            keys_.notify(e);

            if (e.scancode_ == SDL_SCANCODE_RETURN) {
                if (keys_.is_pressed(SDL_SCANCODE_LALT)) {
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


    class FlagShip {

    public:
        bool need_resize() const { return need_resize_; }
        void set_need_resize(bool flag) { need_resize_ = flag; }

        bool dont_render() const { return dont_render_; }
        void set_dont_render(bool flag) { dont_render_ = flag; }

    private:
        bool need_resize_{ false };
        bool dont_render_{ false };
    };


    class CmdBufList {

    private:
        using FIdx = mirinae::FrameIndex;

    public:
        CmdBufList() { frame_data_.resize(mirinae::MAX_FRAMES_IN_FLIGHT); }

        void clear(FIdx f_idx) { frame_data_.at(f_idx.get()).cmdbufs_.clear(); }

        void add(VkCommandBuffer cmdbuf, FIdx f_idx) {
            frame_data_.at(f_idx.get()).cmdbufs_.push_back(cmdbuf);
        }

        const VkCommandBuffer* data(FIdx f_idx) const {
            return frame_data_.at(f_idx.get()).cmdbufs_.data();
        }

        size_t size(FIdx f_idx) const {
            return frame_data_.at(f_idx.get()).cmdbufs_.size();
        }

        std::vector<VkCommandBuffer>& vector(FIdx f_idx) {
            return frame_data_.at(f_idx.get()).cmdbufs_;
        }

    private:
        struct FrameData {
            std::vector<VkCommandBuffer> cmdbufs_;
        };

        std::vector<FrameData> frame_data_;
    };

}  // namespace


// Render pass states
namespace {

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
                        fbufs.compo(i).image_view(),
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
            const mirinae::FrameIndex f_index,
            const mirinae::ShainImageIndex i_index,
            mirinae::FbufImageBundle& gbufs,
            mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("fillscreen");

            mirinae::ImageMemoryBarrier{}
                .image(gbufs.compo(f_index.get()).image())
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_lay(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_lay(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_src_acc(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_acc(VK_ACCESS_SHADER_READ_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(i_index.get()))
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
                .set(desc_sets_.at(f_index.get()))
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

        void record(const mirinae::RpContext& ctxt) {
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


// Tasks
namespace { namespace task {

    class UpdateRenContext : public mirinae::DependingTask {

    public:
        void init(
            mirinae::RpContext& rp_ctxt,
            ::FrameSync& framesync,
            ::FlagShip& flag_ship,
            const mirinae::Scene& scene,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        ) {
            ren_ctxt_ = &rp_ctxt;
            framesync_ = &framesync;
            flag_ship_ = &flag_ship;
            scene_ = &scene;
            swapchain_ = &swapchain;
            device_ = &device;
        }

        void prepare() {}

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            this->update(
                *flag_ship_,
                *framesync_,
                *scene_,
                *ren_ctxt_,
                *swapchain_,
                *device_
            );
        }

        static void update(
            ::FlagShip& flag_ship,
            ::FrameSync& framesync,
            const mirinae::Scene& scene,
            mirinae::RpContext& ren_ctxt,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        ) {
            namespace cpnt = mirinae::cpnt;

            if (flag_ship.need_resize()) {
                flag_ship.set_dont_render(true);
                return;
            }
            if (::is_fbuf_too_small(swapchain.width(), swapchain.height())) {
                flag_ship.set_need_resize(true);
                flag_ship.set_dont_render(true);
                return;
            }
            const auto i_idx = try_acquire_image(framesync, swapchain, device);
            if (!i_idx) {
                flag_ship.set_need_resize(true);
                flag_ship.set_dont_render(true);
                return;
            }

            flag_ship.set_need_resize(false);
            flag_ship.set_dont_render(false);

            const auto e_cam = scene.main_camera_;
            const auto cam = scene.reg_->try_get<cpnt::StandardCamera>(e_cam);
            if (!cam) {
                SPDLOG_WARN("No camera found in scene.");
                flag_ship.set_dont_render(true);
                return;
            }

            ren_ctxt.proj_mat_ = cam->proj_.make_proj_mat(
                swapchain.width(), swapchain.height()
            );

            if (auto tform = scene.reg_->try_get<cpnt::Transform>(e_cam)) {
                ren_ctxt.view_mat_ = tform->make_view_mat();
                ren_ctxt.view_pos_ = tform->pos_;
                ren_ctxt.main_cam_.update(
                    *cam, *tform, swapchain.width(), swapchain.height()
                );
            } else {
                ren_ctxt.view_mat_ = glm::dmat4(1);
                ren_ctxt.view_pos_ = glm::dvec3(0);
            }

            ren_ctxt.f_index_ = framesync.get_frame_index();
            ren_ctxt.i_index_ = i_idx.value();
            ren_ctxt.dt_ = scene.clock().dt();

            ren_ctxt.view_frustum_.update(
                ren_ctxt.proj_mat_, ren_ctxt.view_mat_
            );
        }

        static std::optional<mirinae::ShainImageIndex> try_acquire_image(
            ::FrameSync& framesync,
            mirinae::Swapchain& swapchain,
            mirinae::VulkanDevice& device
        ) {
            framesync.get_cur_in_flight_fence().wait(device.logi_device());

            const auto i_idx = swapchain.acquire_next_image(
                framesync.get_cur_img_ava_semaph().get(), device.logi_device()
            );
            if (!i_idx)
                return std::nullopt;

            framesync.get_cur_in_flight_fence().reset(device.logi_device());
            return i_idx.value();
        }

        ::FlagShip* flag_ship_ = nullptr;
        ::FrameSync* framesync_ = nullptr;
        mirinae::RpContext* ren_ctxt_ = nullptr;
        mirinae::Swapchain* swapchain_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
        const mirinae::Scene* scene_ = nullptr;
    };


    std::mutex g_model_mtx;
    std::mutex g_actor_mtx;

    class InitStaticModel : public mirinae::DependingTask {

    public:
        void init(
            entt::registry& reg,
            mirinae::VulkanDevice& device,
            mirinae::IModelManager& model_mgr,
            mirinae::RpCtxt& rp_ctxt,
            mirinae::RpResources& rp_res
        ) {
            reg_ = &reg;
            device_ = &device;
            model_mgr_ = &model_mgr;
            rp_ctxt_ = &rp_ctxt;
            rp_res_ = &rp_res;
        }

        void prepare() {
            this->set_size(reg_->view<mirinae::cpnt::MdlActorStatic>().size());
        }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            namespace cpnt = mirinae::cpnt;

            auto& reg = *reg_;
            auto view = reg.view<cpnt::MdlActorStatic>();
            auto begin = view.begin() + range.start;
            auto end = view.begin() + range.end;

            for (auto it = begin; it != end; ++it) {
                const auto e = *it;
                auto& mactor = reg.get<cpnt::MdlActorStatic>(e);

                if (!this->load_model(mactor, *model_mgr_))
                    continue;

                if (!this->create_actor(mactor, rp_res_->desclays_, *device_))
                    continue;

                this->update_ubuf(e, mactor, reg, *rp_ctxt_, *device_);
            }

            return;
        }

        static bool load_model(
            mirinae::cpnt::MdlActorStatic& mactor,
            mirinae::IModelManager& model_mgr
        ) {
            if (mactor.model_)
                return true;

            std::lock_guard<std::mutex> lock(g_model_mtx);

            const auto& mdl_path = mactor.model_path_;
            const auto res = model_mgr.request_static(mdl_path);
            if (dal::ReqResult::loading == res) {
                return false;
            } else if (dal::ReqResult::ready != res) {
                const auto path_str = mdl_path.u8string();
                SPDLOG_WARN("Failed to get model: {}", path_str);
                mactor.model_path_ = "Sung/rickroll.dun/rickroll.dmd";
                return false;
            }

            mactor.model_ = model_mgr.get_static(mdl_path);
            if (!mactor.model_) {
                const auto path_str = mdl_path.u8string();
                SPDLOG_WARN("Failed to get model: {}", path_str);
                return false;
            }

            return true;
        }

        static bool create_actor(
            mirinae::cpnt::MdlActorStatic& mactor,
            mirinae::DesclayoutManager& desclayout,
            mirinae::VulkanDevice& device
        ) {
            if (mactor.actor_)
                return true;

            std::lock_guard<std::mutex> lock(g_actor_mtx);

            auto a = std::make_shared<mirinae::RenderActor>(device);
            a->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout);
            mactor.actor_ = a;
            return true;
        }

        static bool update_ubuf(
            const entt::entity e,
            mirinae::cpnt::MdlActorStatic& mactor,
            const entt::registry& reg,
            const mirinae::RpCtxt& rp_ctxt,
            mirinae::VulkanDevice& device
        ) {
            auto actor = mactor.get_actor<mirinae::RenderActor>();

            glm::dmat4 model_mat(1);
            if (auto tform = reg.try_get<mirinae::cpnt::Transform>(e))
                model_mat = tform->make_model_mat();
            const auto vm = rp_ctxt.main_cam_.view() * model_mat;
            const auto pvm = rp_ctxt.main_cam_.proj() * vm;

            mirinae::U_GbufActor udata;
            udata.model = model_mat;
            udata.view_model = vm;
            udata.pvm = pvm;

            actor->udpate_ubuf(
                rp_ctxt.f_index_.get(), udata, device.mem_alloc()
            );

            return true;
        }

        entt::registry* reg_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
        mirinae::IModelManager* model_mgr_ = nullptr;
        mirinae::RpCtxt* rp_ctxt_ = nullptr;
        mirinae::RpResources* rp_res_ = nullptr;
    };


    class InitSkinnedModel : public mirinae::DependingTask {

    public:
        void init(
            mirinae::Scene& scene,
            mirinae::VulkanDevice& device,
            mirinae::IModelManager& model_mgr,
            mirinae::RpCtxt& rp_ctxt,
            mirinae::RpResources& rp_res
        ) {
            scene_ = &scene;
            device_ = &device;
            model_mgr_ = &model_mgr;
            rp_ctxt_ = &rp_ctxt;
            rp_res_ = &rp_res;
        }

        void prepare() {
            this->set_size(
                scene_->reg_->view<mirinae::cpnt::MdlActorSkinned>().size()
            );
        }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            namespace cpnt = mirinae::cpnt;

            auto& reg = *scene_->reg_;
            auto view = reg.view<cpnt::MdlActorSkinned>();
            auto begin = view.begin() + range.start;
            auto end = view.begin() + range.end;

            for (auto it = begin; it != end; ++it) {
                const auto e = *it;
                auto& mactor = reg.get<cpnt::MdlActorSkinned>(e);

                if (!this->load_model(mactor, *model_mgr_))
                    continue;

                if (!this->create_actor(mactor, rp_res_->desclays_, *device_))
                    continue;

                this->update_ubuf(e, mactor, *scene_, *rp_ctxt_, *device_);
            }
        }

        static bool load_model(
            mirinae::cpnt::MdlActorSkinned& mactor,
            mirinae::IModelManager& model_mgr
        ) {
            if (mactor.model_)
                return true;

            std::lock_guard<std::mutex> lock(g_model_mtx);

            const auto mdl_path = mactor.model_path_;
            const auto res = model_mgr.request_skinned(mdl_path);
            if (dal::ReqResult::loading == res) {
                return false;
            } else if (dal::ReqResult::ready != res) {
                const auto path_str = mdl_path.u8string();
                SPDLOG_WARN("Failed to get model: {}", path_str);
                return false;
            }

            auto model = model_mgr.get_skinned(mdl_path);
            if (!model) {
                const auto path_str = mdl_path.u8string();
                SPDLOG_WARN("Failed to get model: {}", path_str);
                return false;
            }

            mactor.model_ = model;
            mactor.anim_state_.set_skel_anim(model->skel_anim_);

            return true;
        }

        static bool create_actor(
            mirinae::cpnt::MdlActorSkinned& mactor,
            mirinae::DesclayoutManager& desclayout,
            mirinae::VulkanDevice& device
        ) {
            if (mactor.actor_)
                return true;

            std::lock_guard<std::mutex> lock(g_actor_mtx);

            auto a = std::make_shared<mirinae::RenderActorSkinned>(device);
            a->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout);
            mactor.actor_ = a;
            return true;
        }

        static bool update_ubuf(
            const entt::entity e,
            mirinae::cpnt::MdlActorSkinned& mactor,
            const mirinae::Scene& scene,
            const mirinae::RpCtxt& rp_ctxt,
            mirinae::VulkanDevice& device
        ) {
            auto& reg = *scene.reg_;
            auto actor = mactor.get_actor<mirinae::RenderActorSkinned>();

            glm::dmat4 model_mat(1);
            if (auto tform = reg.try_get<mirinae::cpnt::Transform>(e))
                model_mat = tform->make_model_mat();
            const auto vm = rp_ctxt.main_cam_.view() * model_mat;
            const auto pvm = rp_ctxt.main_cam_.proj() * vm;

            mirinae::U_GbufActorSkinned udata;
            udata.view_model = vm;
            udata.pvm = pvm;

            mactor.anim_state_.sample_anim(
                udata.joint_transforms_, mirinae::MAX_JOINTS, scene.clock()
            );

            actor->udpate_ubuf(
                rp_ctxt.f_index_.get(), udata, device.mem_alloc()
            );

            return true;
        }

        mirinae::Scene* scene_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
        mirinae::IModelManager* model_mgr_ = nullptr;
        mirinae::RpCtxt* rp_ctxt_ = nullptr;
        mirinae::RpResources* rp_res_ = nullptr;
    };


    class UpdateDlight : public mirinae::DependingTask {

    public:
        void init(
            mirinae::CosmosSimulator& cosmos, mirinae::Swapchain& swhain
        ) {
            cosmos_ = &cosmos;
            swhain_ = &swhain;
        }

        void prepare() {
            this->set_size(cosmos_->reg().view<mirinae::cpnt::DLight>().size());
        }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            namespace cpnt = mirinae::cpnt;

            auto& reg = cosmos_->reg();

            const auto e_cam = cosmos_->scene().main_camera_;
            auto cam = reg.try_get<cpnt::StandardCamera>(e_cam);
            auto cam_view = reg.try_get<cpnt::Transform>(e_cam);

            if (cam == nullptr || cam_view == nullptr) {
                SPDLOG_WARN("Not a camera: {}", static_cast<uint32_t>(e_cam));
                return;
            }

            const auto view_inv = glm::inverse(cam_view->make_view_mat());

            auto view = reg.view<cpnt::DLight>();
            auto begin = view.begin() + range.start;
            auto end = view.begin() + range.end;
            for (auto it = begin; it != end; ++it) {
                const auto e = *it;
                auto& light = reg.get<cpnt::DLight>(e);

                auto tfrom = reg.try_get<cpnt::Transform>(e);
                if (!tfrom) {
                    SPDLOG_WARN(
                        "DLight without transform: {}",
                        static_cast<uint32_t>(e_cam)
                    );
                    continue;
                }

                tfrom->pos_ = cam_view->pos_;
                light.cascades_.update(
                    swhain_->calc_ratio(), view_inv, cam->proj_, light, *tfrom
                );
            }
        }

        mirinae::CosmosSimulator* cosmos_ = nullptr;
        mirinae::Swapchain* swhain_;
    };


    class RenderPasses : public mirinae::DependingTask {

    public:
        RenderPasses() {}

        void init(
            ::FlagShip& flag_ship,
            ::CmdBufList& cmdbuf_list,
            std::vector<std::unique_ptr<mirinae::IRpBase>>& passes,
            mirinae::RpResources& rp_res,
            mirinae::RpCtxt& rp_ctxt,
            mirinae::VulkanDevice& device,
            std::function<bool()> resize_func
        ) {
            resize_func_ = resize_func;
            flag_ship_ = &flag_ship;
            cmdbuf_list_ = &cmdbuf_list;
            rp_res_ = &rp_res;
            rp_ctxt_ = &rp_ctxt;
            device_ = &device;

            for (auto& rp : passes) {
                passes_.push_back(rp->create_task());
            }
        }

        void prepare() {}

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            if (flag_ship_->need_resize()) {
                if (resize_func_) {
                    resize_func_();
                    flag_ship_->set_need_resize(false);
                }
            }

            if (flag_ship_->dont_render()) {
                return;
            }

            rp_res_->cmd_pool_.reset_pool(rp_ctxt_->f_index_, *device_);
            cmdbuf_list_->clear(rp_ctxt_->f_index_);

            for (auto& rp : passes_) rp->prepare(*rp_ctxt_);
            for (auto& rp : passes_) this->try_run(rp->update_task());
            for (auto& rp : passes_) this->try_wait(rp->update_fence());
            for (auto& rp : passes_) this->try_run(rp->record_task());
            for (auto& rp : passes_) this->try_wait(rp->record_fence());
            for (auto& rp : passes_)
                rp->collect_cmdbuf(cmdbuf_list_->vector(rp_ctxt_->f_index_));
        }

        static void try_run(enki::ITaskSet* task) {
            if (task) {
                dal::tasker().AddTaskSetToPipe(task);
            }
        }

        static void try_wait(enki::ITaskSet* task) {
            if (task) {
                dal::tasker().WaitforTask(task);
            }
        }

        ::FlagShip* flag_ship_ = nullptr;
        ::CmdBufList* cmdbuf_list_ = nullptr;
        mirinae::RpResources* rp_res_ = nullptr;
        mirinae::RpCtxt* rp_ctxt_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
        std::function<bool()> resize_func_;

        std::vector<std::unique_ptr<mirinae::IRpTask>> passes_;
    };


    class RenderStage : public mirinae::StageTask {

    public:
        RenderStage() : mirinae::StageTask("vulan renderer") {
            update_ren_ctxt_.succeed(this);
            init_static_.succeed(&update_ren_ctxt_);
            init_skinned_.succeed(&update_ren_ctxt_);
            update_dlight_.succeed(&update_ren_ctxt_);
            render_passes_.succeed(
                &init_static_, &init_skinned_, &update_dlight_
            );
            fence_.succeed(&render_passes_);
        }

    private:
        enki::ITaskSet* get_fence() override { return &fence_; }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            update_ren_ctxt_.prepare();
            init_static_.prepare();
            init_skinned_.prepare();
            update_dlight_.prepare();
            render_passes_.prepare();
        }

    public:
        UpdateRenContext update_ren_ctxt_;
        InitStaticModel init_static_;
        InitSkinnedModel init_skinned_;
        UpdateDlight update_dlight_;
        RenderPasses render_passes_;

    private:
        mirinae::FenceTask fence_;
    };

}}  // namespace ::task


// Engine
namespace {

    class RendererVulkan : public mirinae::IRenderer {

    public:
        RendererVulkan(
            mirinae::EngineCreateInfo& cinfo,
            sung::HTaskSche task_sche,
            std::shared_ptr<mirinae::CosmosSimulator> cosmos,
            int init_width,
            int init_height
        )
            : device_(cinfo)
            , cosmos_(cosmos)
            , ecinfo_(cinfo)
            , rp_res_(task_sche, device_)
            , overlay_man_(
                  init_width,
                  init_height,
                  rp_res_.desclays_,
                  *rp_res_.tex_man_,
                  device_
              )
            , rp_states_imgui_(device_)
            , fbuf_width_(init_width)
            , fbuf_height_(init_height) {
            framesync_.init(device_.logi_device());

            rp_res_.shadow_maps_ = mirinae::rp::create_shadow_maps_bundle(
                device_
            );
            model_man_ = mirinae::create_model_mgr(
                task_sche, rp_res_.tex_man_, rp_res_.desclays_, device_
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
                swapchain_.init(device_);

                const auto [gbuf_width, gbuf_height] = ::calc_scaled_dimensions(
                    swapchain_.width(), swapchain_.height()
                );
                rp_res_.gbuf_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    gbuf_width,
                    gbuf_height,
                    *rp_res_.tex_man_,
                    device_
                );

                mirinae::rp::gbuf::create_desc_layouts(
                    rp_res_.desclays_, device_
                );

                mirinae::RpCreateBundle cbundle{ *cosmos_, rp_res_, device_ };

                render_passes_.push_back(
                    mirinae::rp::create_rp_atmos_trans_lut(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_atmos_multi_scat(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_sky_view_lut(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_atmos_cam_vol(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_ocean_h0(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_ocean_hkt(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_ocean_butterfly(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_ocean_post_ift(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_envmap(cbundle)
                );

                rp_.init_render_passes(
                    rp_res_.gbuf_, rp_res_.desclays_, swapchain_, device_
                );
                rp_states_fillscreen_.init(
                    rp_res_.desclays_, rp_res_.gbuf_, device_
                );
                rp_states_imgui_.init(swapchain_);

                render_passes_.push_back(
                    mirinae::rp::create_rp_shadow_static(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_shadow_skinned(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_shadow_static_trs(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_shadow_skinned_trs(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_shadow_terrain(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::gbuf::create_rp_gbuf_static(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::gbuf::create_rp_gbuf_skinned(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::gbuf::create_rp_gbuf_terrain(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::compo::create_rps_dlight(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::compo::create_rps_slight(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::compo::create_rps_envmap(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::compo::create_rps_sky(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_ocean_tess(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_states_transp_static(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_states_transp_skinned(cbundle)
                );

                render_passes_.push_back(
                    mirinae::rp::create_rp_debug(cbundle, ren_ctxt.debug_ren_)
                );
            }

            cmd_pool_.init(
                device_.graphics_queue_family_index().value(),
                device_.logi_device()
            );
            basic_cmdbufs_.resize(mirinae::MAX_FRAMES_IN_FLIGHT);
            cmd_pool_.alloc(
                basic_cmdbufs_.data(), basic_cmdbufs_.size(), device_
            );

            {
                input_mgrs_.add(std::make_unique<DominantCommandProc>(device_));
                input_mgrs_.add(&overlay_man_);
            }

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
                rp_states_imgui_.destroy();
                rp_states_fillscreen_.destroy(device_);

                rp_.destroy();
                render_passes_.clear();
            }

            swapchain_.destroy(device_.logi_device());
            cmd_pool_.destroy(device_.logi_device());
            framesync_.destroy(device_.logi_device());
        }

        void register_tasks(mirinae::TaskGraph& tasks) override {
            auto stage = tasks.emplace_back<::task::RenderStage>();

            stage->update_ren_ctxt_.init(
                ren_ctxt,
                framesync_,
                flag_ship_,
                cosmos_->scene(),
                swapchain_,
                device_
            );

            stage->init_static_.init(
                cosmos_->reg(), device_, *model_man_, ren_ctxt, rp_res_
            );

            stage->init_skinned_.init(
                cosmos_->scene(), device_, *model_man_, ren_ctxt, rp_res_
            );

            stage->update_dlight_.init(*cosmos_, swapchain_);

            stage->render_passes_.init(
                flag_ship_,
                cmdbufs_,
                render_passes_,
                rp_res_,
                ren_ctxt,
                device_,
                [this]() {
                    if (this->resize_swapchain()) {
                        overlay_man_.on_fbuf_resize(fbuf_width_, fbuf_height_);
                        return true;
                    } else {
                        return false;
                    }
                }
            );
        }

        void do_frame() override {
            if (flag_ship_.dont_render())
                return;

            const auto f_idx = ren_ctxt.f_index_;
            auto& cam = cosmos_->reg().get<mirinae::cpnt::StandardCamera>(
                cosmos_->scene().main_camera_
            );

            // Update widgets
            mirinae::WidgetRenderUniData widget_ren_data;
            widget_ren_data.win_dim_ = overlay_man_.win_dim();
            widget_ren_data.frame_index_ = framesync_.get_frame_index().get();
            widget_ren_data.cmd_buf_ = VK_NULL_HANDLE;
            widget_ren_data.pipe_layout_ = VK_NULL_HANDLE;
            overlay_man_.widgets().tick(widget_ren_data);

            namespace cpnt = mirinae::cpnt;

            // Begin recording
            ren_ctxt.cmdbuf_ = basic_cmdbufs_.at(f_idx.get());
            cmdbufs_.add(ren_ctxt.cmdbuf_, ren_ctxt.f_index_);
            mirinae::begin_cmdbuf(ren_ctxt.cmdbuf_);

            rp_states_fillscreen_.record(
                ren_ctxt.cmdbuf_,
                swapchain_.extent(),
                cam,
                ren_ctxt.f_index_,
                ren_ctxt.i_index_,
                rp_res_.gbuf_,
                rp_
            );

            // Shader: Overlay
            {
                auto& rp = rp_.get("overlay");

                mirinae::RenderPassBeginInfo{}
                    .rp(rp.renderpass())
                    .fbuf(rp.fbuf_at(ren_ctxt.i_index_.get()))
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
                ecinfo_.vulkan_os_->imgui_new_frame();
                ImGui::NewFrame();
                // ImGui::ShowDemoWindow();

                for (auto& w : cosmos_->imgui_) {
                    w->render();
                }

                ImGui::Render();
                rp_states_imgui_.record(ren_ctxt);
            }

            mirinae::end_cmdbuf(ren_ctxt.cmdbuf_);
            ren_ctxt.debug_ren_.clear();

            // Submit and present
            {
                const VkSemaphore signal_semaph =
                    framesync_.get_cur_render_fin_semaph().get();

                mirinae::SubmitInfo{}
                    .add_wait_semaph_color_attach_out(
                        framesync_.get_cur_img_ava_semaph().get()
                    )
                    .add_signal_semaph(signal_semaph)
                    .add_cmdbuf(cmdbufs_.data(f_idx), cmdbufs_.size(f_idx))
                    .queue_submit_single(
                        device_.graphics_queue(),
                        framesync_.get_cur_in_flight_fence().get()
                    );

                mirinae::PresentInfo{}
                    .add_wait_semaph(signal_semaph)
                    .add_swapchain(swapchain_.get())
                    .add_image_index(ren_ctxt.i_index_.get())
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

        mirinae::IDebugRen& debug_ren() override { return ren_ctxt.debug_ren_; }

    private:
        bool resize_swapchain() {
            device_.wait_idle();

            // Destroy
            {
                // rp_states_imgui_.destroy();
                rp_states_fillscreen_.destroy(device_);

                rp_.destroy();
            }

            // Create
            {
                if (!swapchain_.init(device_))
                    return false;
                if (!swapchain_.is_ready())
                    return false;

                const auto extent = swapchain_.extent();
                if (::is_fbuf_too_small(extent.width, extent.height))
                    return false;

                const auto [gbuf_width, gbuf_height] = ::calc_scaled_dimensions(
                    swapchain_.width(), swapchain_.height()
                );

                rp_res_.gbuf_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    gbuf_width,
                    gbuf_height,
                    *rp_res_.tex_man_,
                    device_
                );

                mirinae::rp::gbuf::create_desc_layouts(
                    rp_res_.desclays_, device_
                );
                rp_.init_render_passes(
                    rp_res_.gbuf_, rp_res_.desclays_, swapchain_, device_
                );
                rp_states_fillscreen_.init(
                    rp_res_.desclays_, rp_res_.gbuf_, device_
                );
            }

            // Optimized resize
            {
                rp_states_imgui_.on_swchain_resize(swapchain_);

                const auto width = swapchain_.width();
                const auto height = swapchain_.height();
                for (auto& pass : render_passes_) {
                    pass->on_resize(width, height);
                }
            }

            return true;
        }

        // This must be the first member variable
        mirinae::VulkanDevice device_;

        // External dependencies
        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;
        mirinae::EngineCreateInfo& ecinfo_;

        ::FrameSync framesync_;
        ::FlagShip flag_ship_;
        mirinae::rg::RenderGraphDef render_graph_;
        mirinae::RpResources rp_res_;
        mirinae::HMdlMgr model_man_;
        mirinae::OverlayManager overlay_man_;
        mirinae::RenderPassPackage rp_;
        mirinae::Swapchain swapchain_;
        mirinae::RpContext ren_ctxt;
        mirinae::InputProcesserMgr input_mgrs_;

        // Command buffers
        ::CmdBufList cmdbufs_;
        mirinae::CommandPool cmd_pool_;
        std::vector<VkCommandBuffer> basic_cmdbufs_;

        // Render passes
        ::RpStatesFillscreen rp_states_fillscreen_;
        ::RpStatesImgui rp_states_imgui_;
        std::vector<std::unique_ptr<mirinae::IRpBase>> render_passes_;

        // PODs
        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
        bool fbuf_resized_ = false;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IRenderer> create_vk_renderer(
        mirinae::EngineCreateInfo& cinfo,
        sung::HTaskSche task_sche,
        std::shared_ptr<mirinae::CosmosSimulator> cosmos
    ) {
        const auto w = cinfo.init_width_;
        const auto h = cinfo.init_height_;
        return std::make_unique<::RendererVulkan>(
            cinfo, task_sche, cosmos, w, h
        );
    }

}  // namespace mirinae
