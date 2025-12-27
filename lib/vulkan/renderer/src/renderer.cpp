#include "mirinae/vulkan/renderer.hpp"

#include <SDL3/SDL_scancode.h>
#include <imgui_impl_vulkan.h>
#include <dal/auxiliary/path.hpp>
#include <dal/auxiliary/util.hpp>
#include <entt/entity/registry.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/camera.hpp"
#include "mirinae/cpnt/terrain.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/math/mamath.hpp"
#include "mirinae/vulkan/base/overlay/overlay.hpp"
#include "mirinae/vulkan/base/platform_func.hpp"
#include "mirinae/vulkan/base/render/cmdbuf.hpp"
#include "mirinae/vulkan/base/render/draw_set.hpp"
#include "mirinae/vulkan/base/render/render_graph.hpp"
#include "mirinae/vulkan/base/render/renderpass.hpp"
#include "mirinae/vulkan/base/render/vkcheck.hpp"
#include "mirinae/vulkan/base/renderee/atmos.hpp"
#include "mirinae/vulkan/base/renderpass/builder.hpp"

#include "renderpasses.hpp"
#include "task/render_stage.hpp"


namespace {

    template <typename T>
    std::pair<uint32_t, uint32_t> calc_scaled_dimensions(T w, T h) {
        constexpr double SCALE_FACTOR = 1.25;
        return std::make_pair(
            static_cast<uint32_t>(SCALE_FACTOR * w),
            static_cast<uint32_t>(SCALE_FACTOR * h)
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
                        fbufs.compo(mirinae::FrameIndex(i)).image_view(),
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
            DEBUG_LABEL.record_begin(cmdbuf);

            mirinae::ImageMemoryBarrier{}
                .image(gbufs.compo(f_index).image())
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
            DEBUG_LABEL.record_end(cmdbuf);
        }

        const mirinae::DebugLabel DEBUG_LABEL{
            "Fill Screen", 0.47, 0.56, 0.61
        };

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

            rp_res_.shadow_maps_ = mirinae::create_shadow_maps_bundle(device_);
            model_man_ = mirinae::create_model_mgr(
                task_sche, rp_res_.tex_man_, rp_res_.desclays_, device_
            );

            // Render graph
            /*
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

                rg.new_pass("gbuf terrain")
                    .set_impl_f(mirinae::rp::create_rpimpl_gbuf_terrain)
                    .add_inout_atta(rg.get_img("gbuf depth"))
                    .add_inout_atta(rg.get_img("gbuf albedo"))
                    .add_inout_atta(rg.get_img("gbuf normal"))
                    .add_inout_atta(rg.get_img("gbuf material"));
            }
            */

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

                mirinae::create_gbuf_desc_layouts(rp_res_.desclays_, device_);

                mirinae::RpCreateBundle cbundle{
                    *cosmos_, rp_res_, ren_ctxt.debug_ren_, device_
                };

                rp_.init_render_passes(
                    rp_res_.gbuf_, rp_res_.desclays_, swapchain_, device_
                );
                rp_states_fillscreen_.init(
                    rp_res_.desclays_, rp_res_.gbuf_, device_
                );
                rp_states_imgui_.init(swapchain_);

                for (auto func : mirinae::get_rp_factories())
                    render_passes_.push_back(func(cbundle));
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

            // GPu side init
            {
                const auto cmdbuf = cmd_pool_.begin_single_time(device_);
                rp_res_.envmaps_->record_gpu_init(cmdbuf);
                cmd_pool_.end_single_time(cmdbuf, device_);
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
            for (auto& e : reg.view<mirinae::cpnt::AtmosphereEpic>()) {
                auto& c = reg.get<mirinae::cpnt::AtmosphereEpic>(e);
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
            tasks.push_back(
                mirinae::create_render_stage(
                    cmdbufs_,
                    *cosmos_,
                    flag_ship_,
                    framesync_,
                    *model_man_,
                    ren_ctxt,
                    rp_res_,
                    [this]() {
                        if (this->resize_swapchain()) {
                            overlay_man_.on_fbuf_resize(
                                fbuf_width_, fbuf_height_
                            );
                            return true;
                        } else {
                            return false;
                        }
                    },
                    render_passes_,
                    swapchain_,
                    device_
                )
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
                const mirinae::DebugLabel DEBUG_LABEL{
                    "Overlay", 0.47, 0.56, 0.61
                };

                auto& rp = rp_.get("overlay");
                DEBUG_LABEL.record_begin(ren_ctxt.cmdbuf_);

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
                DEBUG_LABEL.record_end(ren_ctxt.cmdbuf_);
            }

            // ImGui
            {
                const mirinae::DebugLabel DEBUG_LABEL{
                    "ImGui", 0.47, 0.56, 0.61
                };
                DEBUG_LABEL.record_begin(ren_ctxt.cmdbuf_);

                ImGui_ImplVulkan_NewFrame();
                ecinfo_.vulkan_os_->imgui_new_frame();
                ImGui::NewFrame();
                // ImGui::ShowDemoWindow();

                for (auto& w : cosmos_->imgui_) {
                    w->render();
                }

                ImGui::Render();
                rp_states_imgui_.record(ren_ctxt);
                DEBUG_LABEL.record_end(ren_ctxt.cmdbuf_);
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
                if (mirinae::is_fbuf_too_small(extent.width, extent.height))
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

                mirinae::create_gbuf_desc_layouts(rp_res_.desclays_, device_);
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

        mirinae::FrameSync framesync_;
        mirinae::FlagShip flag_ship_;
        mirinae::rg::RenderGraphDef render_graph_;
        mirinae::RpResources rp_res_;
        mirinae::HMdlMgr model_man_;
        mirinae::OverlayManager overlay_man_;
        mirinae::RenderPassPackage rp_;
        mirinae::Swapchain swapchain_;
        mirinae::RpContext ren_ctxt;
        mirinae::InputProcesserMgr input_mgrs_;

        // Command buffers
        mirinae::CmdBufList cmdbufs_;
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
