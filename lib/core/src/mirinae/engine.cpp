#include "mirinae/engine.hpp"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>

#include <daltools/util.h>
#include <sung/general/time.hpp>

#include "mirinae/overlay/overlay.hpp"
#include "mirinae/render/renderpass.hpp"
#include "mirinae/scene/scene.hpp"
#include "mirinae/util/glm_fmt.hpp"
#include "mirinae/util/mamath.hpp"
#include "mirinae/util/script.hpp"


namespace {

    using FrameIndex = mirinae::StrongType<int, struct FrameIndexStrongTypeTag>;


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

        FrameIndex get_frame_index() const { return cur_frame_; }
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
        FrameIndex cur_frame_{ 0 };
    };

}  // namespace


namespace {

    struct DrawSheet {
        struct StaticRenderPairs {
            struct Actor {
                mirinae::RenderActor* actor_;
                glm::dmat4 model_mat_;
            };

            mirinae::RenderModel* model_;
            std::vector<Actor> actors_;
        };

        struct SkinnedRenderPairs {
            struct Actor {
                mirinae::RenderActorSkinned* actor_;
                glm::dmat4 model_mat_;
            };

            mirinae::RenderModelSkinned* model_;
            std::vector<Actor> actors_;
        };

        StaticRenderPairs& get_static_pair(mirinae::RenderModel& model) {
            for (auto& x : static_pairs_) {
                if (x.model_ == &model)
                    return x;
            }

            auto& output = static_pairs_.emplace_back();
            output.model_ = &model;
            return output;
        }

        SkinnedRenderPairs& get_skinn_pair(mirinae::RenderModelSkinned& model) {
            for (auto& x : skinned_pairs_) {
                if (x.model_ == &model)
                    return x;
            }

            auto& output = skinned_pairs_.emplace_back();
            output.model_ = &model;
            return output;
        }

        std::vector<StaticRenderPairs> static_pairs_;
        std::vector<SkinnedRenderPairs> skinned_pairs_;
    };

    DrawSheet make_draw_sheet(mirinae::Scene& scene) {
        using CTrans = mirinae::cpnt::Transform;
        using CStaticModelActor = mirinae::cpnt::StaticActorVk;
        using CSkinnedModelActor = mirinae::cpnt::SkinnedActorVk;

        DrawSheet sheet;

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

        return sheet;
    }

}  // namespace


// Render pass states
namespace {

    class RpStatesCompo {

    public:
        void init(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::FbufImageBundle& fbufs,
            VkImageView dlight_shadowmap,
            VkSampler texture_sampler,
            VkSampler shadow_map_sampler,
            mirinae::VulkanDevice& device
        ) {
            desc_pool_.init(10, device.logi_device());
            desc_sets_ = desc_pool_.alloc(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayouts.get("compo:main"),
                device.logi_device()
            );

            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& ubuf = ubufs_.emplace_back();
                ubuf.init_ubuf(
                    sizeof(mirinae::U_CompoMain), device.mem_alloc()
                );

                mirinae::DescWriteInfoBuilder builder;
                builder
                    .add_combinded_image_sampler(
                        fbufs.depth().image_view(),
                        texture_sampler,
                        desc_sets_.at(i)
                    )
                    .add_combinded_image_sampler(
                        fbufs.albedo().image_view(),
                        texture_sampler,
                        desc_sets_.at(i)
                    )
                    .add_combinded_image_sampler(
                        fbufs.normal().image_view(),
                        texture_sampler,
                        desc_sets_.at(i)
                    )
                    .add_combinded_image_sampler(
                        fbufs.material().image_view(),
                        texture_sampler,
                        desc_sets_.at(i)
                    )
                    .add_uniform_buffer(ubuf, desc_sets_.at(i))
                    .add_combinded_image_sampler(
                        dlight_shadowmap, shadow_map_sampler, desc_sets_.at(i)
                    )
                    .apply_all(device.logi_device());
            }
        }

        void destroy(mirinae::VulkanDevice& device) {
            desc_pool_.destroy(device.logi_device());

            for (auto& ubuf : ubufs_) ubuf.destroy(device.mem_alloc());
            ubufs_.clear();
        }

        mirinae::DescriptorPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<mirinae::Buffer> ubufs_;
    };


    class RpStatesTransp {

    public:
        void init(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        ) {
            desc_pool_.init(10, device.logi_device());
            desc_sets_ = desc_pool_.alloc(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayouts.get("transp:frame"),
                device.logi_device()
            );

            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& ubuf = ubufs_.emplace_back();
                ubuf.init_ubuf(
                    sizeof(mirinae::U_TranspFrame), device.mem_alloc()
                );

                mirinae::DescWriteInfoBuilder builder;
                builder.add_uniform_buffer(ubuf, desc_sets_.at(i))
                    .apply_all(device.logi_device());
            }
        }

        void destroy(mirinae::VulkanDevice& device) {
            desc_pool_.destroy(device.logi_device());

            for (auto& ubuf : ubufs_) ubuf.destroy(device.mem_alloc());
            ubufs_.clear();
        }

        mirinae::DescriptorPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<mirinae::Buffer> ubufs_;
    };


    class RpStatesFillscreen {

    public:
        void init(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::FbufImageBundle& fbufs,
            VkSampler texture_sampler,
            mirinae::VulkanDevice& device
        ) {
            desc_pool_.init(10, device.logi_device());
            desc_sets_ = desc_pool_.alloc(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayouts.get("fillscreen:main"),
                device.logi_device()
            );

            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                mirinae::DescWriteInfoBuilder builder;
                builder
                    .add_combinded_image_sampler(
                        fbufs.compo().image_view(),
                        texture_sampler,
                        desc_sets_.at(i)
                    )
                    .apply_all(device.logi_device());
            }
        }

        void destroy(mirinae::VulkanDevice& device) {
            desc_pool_.destroy(device.logi_device());
        }

        mirinae::DescriptorPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
    };

}  // namespace


// Engine
namespace {

    class EngineGlfw : public mirinae::IEngine {

    public:
        EngineGlfw(
            mirinae::EngineCreateInfo&& cinfo, int init_width, int init_height
        )
            : device_(std::move(cinfo))
            , scene_(script_)
            , tex_man_(device_)
            , model_man_(device_)
            , desclayout_(device_)
            , texture_sampler_(device_)
            , overlay_man_(
                  init_width, init_height, desclayout_, tex_man_, device_
              )
            , shadow_map_sampler_(device_)
            , fbuf_width_(init_width)
            , fbuf_height_(init_height) {
            // This must be the first member variable right after vtable pointer
            static_assert(offsetof(EngineGlfw, device_) == sizeof(void*));

            framesync_.init(device_.logi_device());

            {
                mirinae::SamplerBuilder sampler_builder;
                texture_sampler_.reset(sampler_builder.build(device_));
            }

            {
                mirinae::SamplerBuilder sampler_builder;
                sampler_builder.mag_filter_nearest();
                sampler_builder.min_filter_nearest();
                shadow_map_sampler_.reset(sampler_builder.build(device_));
            }

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
                input_mgrs_.add(&camera_controller_);
            }

            // Widget: Dev console
            {
                dev_console_output_ = mirinae::create_text_blocks();
                script_.replace_output_buf(dev_console_output_);

                auto w = mirinae::create_dev_console(
                    overlay_man_.sampler(),
                    overlay_man_.text_render_data(),
                    desclayout_,
                    tex_man_,
                    script_,
                    device_
                );
                w->replace_output_buf(dev_console_output_);
                w->hide(true);
                overlay_man_.widgets().add_widget(std::move(w));
            }

            // Main Camera
            {
                const auto entt = scene_.reg_.create();
                scene_.main_camera_ = entt;

                auto& d = scene_.reg_.emplace<mirinae::cpnt::StandardCamera>(
                    entt
                );

                d.view_.pos_ = glm::dvec3{ 0.14983922321477,
                                           0.66663010560478,
                                           -1.1615585516897 };
                d.view_.rot_ = { 0.5263130886922,
                                 0.022307853585388,
                                 0.84923568828777,
                                 -0.035994972955897 };
            }

            // DLight
            {
                const auto entt = scene_.reg_.create();
                auto& d = scene_.reg_.emplace<mirinae::cpnt::DLight>(entt);
                d.color_ = glm::vec3{ 5, 5, 5 };
            }

            // Script
            {
                const auto contents = device_.filesys().read_file_to_vector(
                    "asset/script/startup.lua"
                );
                if (contents) {
                    const std::string str{ contents->begin(), contents->end() };
                    script_.exec(str.c_str());
                }
            }

            fps_timer_.set_fps_cap(120);
        }

        ~EngineGlfw() {
            device_.wait_idle();

            cmd_pool_.destroy(device_.logi_device());
            this->destroy_swapchain_and_relatives();
            framesync_.destroy(device_.logi_device());
        }

        void do_frame() override {
            scene_.update_time();
            const auto t = scene_.get_time().tp_;
            const auto delta_time = scene_.get_time().dt_;

            auto& cam = scene_.reg_.get<mirinae::cpnt::StandardCamera>(
                scene_.main_camera_
            );
            camera_controller_.apply(cam.view_, delta_time);

            this->update_unloaded_models();

            const auto image_index_opt = this->try_acquire_image();
            if (!image_index_opt) {
                return;
            }
            const auto image_index = image_index_opt.value();

            const auto proj_mat = cam.proj_.make_proj_mat(
                swapchain_.width(), swapchain_.height()
            );
            const auto view_mat = cam.view_.make_view_mat();
            this->update_ubufs(proj_mat, view_mat);

            // Update widgets
            mirinae::WidgetRenderUniData widget_ren_data;
            widget_ren_data.win_dim_ = overlay_man_.win_dim();
            widget_ren_data.frame_index_ = framesync_.get_frame_index().get();
            widget_ren_data.cmd_buf_ = VK_NULL_HANDLE;
            widget_ren_data.pipe_layout_ = VK_NULL_HANDLE;
            overlay_man_.widgets().tick(widget_ren_data);

            auto draw_sheet = ::make_draw_sheet(scene_);
            auto cur_cmd_buf = cmd_buf_.at(framesync_.get_frame_index().get());

            glm::dmat4 dlight_light_mat{ 1 };
            for (auto& l : scene_.reg_.view<mirinae::cpnt::DLight>()) {
                auto& dlight = scene_.reg_.get<mirinae::cpnt::DLight>(l);
                const auto view_dir = cam.view_.make_forward_dir();

                dlight.transform_.pos_ = cam.view_.pos_;
                dlight.transform_.reset_rotation();
                dlight.transform_.rotate(
                    sung::TAngle<double>::from_deg(-80), { 1, 0, 0 }
                );
                dlight.transform_.rotate(
                    sung::TAngle<double>::from_deg(t), { 0, 1, 0 }
                );
                /*
                dlight.transform_.rotate(
                    sung::TAngle<double>::from_rad(
                        SUNG_PI * -0.5 - std::atan2(view_dir.z, view_dir.x)
                    ),
                    { 0, 1, 0 }
                );
                */

                dlight_light_mat = dlight.make_light_mat();
                break;
            }

            // Begin recording
            {
                vkResetCommandBuffer(cur_cmd_buf, 0);

                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = 0;
                beginInfo.pInheritanceInfo = nullptr;

                if (vkBeginCommandBuffer(cur_cmd_buf, &beginInfo) !=
                    VK_SUCCESS) {
                    throw std::runtime_error(
                        "failed to begin recording command buffer!"
                    );
                }

                std::array<VkClearValue, 3> clear_values;
                clear_values[0].depthStencil = { 1.f, 0 };
                clear_values[1].color = { 0.f, 0.f, 0.f, 1.f };
                clear_values[2].color = { 0.f, 0.f, 0.f, 1.f };
            }

            // Shader: Shadowmap
            {
                auto& rp = *rp_.shadowmap_;

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = shadow_map_fbuf_;
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = shadow_map_->extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = shadow_map_->width();
                viewport.height = shadow_map_->height();
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = shadow_map_->extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                for (auto& pair : draw_sheet.static_pairs_) {
                    for (auto& unit : pair.model_->render_units_) {
                        auto unit_desc = unit.get_desc_set(
                            framesync_.get_frame_index().get()
                        );
                        unit.record_bind_vert_buf(cur_cmd_buf);

                        for (auto& actor : pair.actors_) {
                            auto actor_desc = actor.actor_->get_desc_set(
                                framesync_.get_frame_index().get()
                            );
                            vkCmdBindDescriptorSets(
                                cur_cmd_buf,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                rp.pipeline_layout(),
                                0,
                                1,
                                &actor_desc,
                                0,
                                nullptr
                            );

                            mirinae::U_ShadowPushConst push_const;
                            push_const.pvm_ = dlight_light_mat *
                                              actor.model_mat_;

                            vkCmdPushConstants(
                                cur_cmd_buf,
                                rp.pipeline_layout(),
                                VK_SHADER_STAGE_VERTEX_BIT,
                                0,
                                sizeof(push_const),
                                &push_const
                            );

                            vkCmdDrawIndexed(
                                cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                            );
                        }
                    }
                }

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            // Shader: Shadowmap Skin
            {
                auto& rp = *rp_.shadowmap_skin_;

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = shadow_map_fbuf_;
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = shadow_map_->extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = shadow_map_->width();
                viewport.height = shadow_map_->height();
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = shadow_map_->extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                for (auto& pair : draw_sheet.skinned_pairs_) {
                    for (auto& unit : pair.model_->runits_) {
                        auto unit_desc = unit.get_desc_set(
                            framesync_.get_frame_index().get()
                        );
                        unit.record_bind_vert_buf(cur_cmd_buf);

                        for (auto& actor : pair.actors_) {
                            auto actor_desc = actor.actor_->get_desc_set(
                                framesync_.get_frame_index().get()
                            );
                            vkCmdBindDescriptorSets(
                                cur_cmd_buf,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                rp.pipeline_layout(),
                                0,
                                1,
                                &actor_desc,
                                0,
                                nullptr
                            );

                            mirinae::U_ShadowPushConst push_const;
                            push_const.pvm_ = dlight_light_mat *
                                              actor.model_mat_;

                            vkCmdPushConstants(
                                cur_cmd_buf,
                                rp.pipeline_layout(),
                                VK_SHADER_STAGE_VERTEX_BIT,
                                0,
                                sizeof(push_const),
                                &push_const
                            );

                            vkCmdDrawIndexed(
                                cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                            );
                        }
                    }
                }

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            // Shader: Gbuf
            {
                auto& rp = *rp_.gbuf_;

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = fbuf_images_.extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<float>(fbuf_images_.width());
                viewport.height = static_cast<float>(fbuf_images_.height());
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = fbuf_images_.extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                for (auto& pair : draw_sheet.static_pairs_) {
                    for (auto& unit : pair.model_->render_units_) {
                        auto unit_desc = unit.get_desc_set(
                            framesync_.get_frame_index().get()
                        );
                        vkCmdBindDescriptorSets(
                            cur_cmd_buf,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rp.pipeline_layout(),
                            0,
                            1,
                            &unit_desc,
                            0,
                            nullptr
                        );
                        unit.record_bind_vert_buf(cur_cmd_buf);

                        for (auto& actor : pair.actors_) {
                            auto actor_desc = actor.actor_->get_desc_set(
                                framesync_.get_frame_index().get()
                            );
                            vkCmdBindDescriptorSets(
                                cur_cmd_buf,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                rp.pipeline_layout(),
                                1,
                                1,
                                &actor_desc,
                                0,
                                nullptr
                            );

                            vkCmdDrawIndexed(
                                cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                            );
                        }
                    }
                }

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            // Shader: Gbuf skin
            {
                auto& rp = *rp_.gbuf_skin_;

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = fbuf_images_.extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<float>(fbuf_images_.width());
                viewport.height = static_cast<float>(fbuf_images_.height());
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = fbuf_images_.extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                for (auto& pair : draw_sheet.skinned_pairs_) {
                    for (auto& unit : pair.model_->runits_) {
                        auto unit_desc = unit.get_desc_set(
                            framesync_.get_frame_index().get()
                        );
                        vkCmdBindDescriptorSets(
                            cur_cmd_buf,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rp.pipeline_layout(),
                            0,
                            1,
                            &unit_desc,
                            0,
                            nullptr
                        );
                        unit.record_bind_vert_buf(cur_cmd_buf);

                        for (auto& actor : pair.actors_) {
                            auto actor_desc = actor.actor_->get_desc_set(
                                framesync_.get_frame_index().get()
                            );
                            vkCmdBindDescriptorSets(
                                cur_cmd_buf,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                rp.pipeline_layout(),
                                1,
                                1,
                                &actor_desc,
                                0,
                                nullptr
                            );

                            vkCmdDrawIndexed(
                                cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                            );
                        }
                    }
                }

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            // Shader: Compo
            {
                auto& rp = *rp_.compo_;
                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = fbuf_images_.extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<float>(fbuf_images_.width());
                viewport.height = static_cast<float>(fbuf_images_.height());
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = fbuf_images_.extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                auto desc_main = rp_states_compo_.desc_sets_.at(
                    framesync_.get_frame_index().get()
                );
                vkCmdBindDescriptorSets(
                    cur_cmd_buf,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    rp.pipeline_layout(),
                    0,
                    1,
                    &desc_main,
                    0,
                    nullptr
                );

                vkCmdDraw(cur_cmd_buf, 3, 1, 0, 0);

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            // Shader: Transp
            {
                auto& rp = *rp_.transp_;

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = fbuf_images_.extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<float>(fbuf_images_.width());
                viewport.height = static_cast<float>(fbuf_images_.height());
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = fbuf_images_.extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                auto desc_frame = rp_states_transp_.desc_sets_.at(
                    framesync_.get_frame_index().get()
                );
                vkCmdBindDescriptorSets(
                    cur_cmd_buf,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    rp.pipeline_layout(),
                    0,
                    1,
                    &desc_frame,
                    0,
                    nullptr
                );

                for (auto& pair : draw_sheet.static_pairs_) {
                    for (auto& unit : pair.model_->render_units_alpha_) {
                        auto unit_desc = unit.get_desc_set(
                            framesync_.get_frame_index().get()
                        );
                        vkCmdBindDescriptorSets(
                            cur_cmd_buf,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rp.pipeline_layout(),
                            1,
                            1,
                            &unit_desc,
                            0,
                            nullptr
                        );
                        unit.record_bind_vert_buf(cur_cmd_buf);

                        for (auto& actor : pair.actors_) {
                            auto actor_desc = actor.actor_->get_desc_set(
                                framesync_.get_frame_index().get()
                            );
                            vkCmdBindDescriptorSets(
                                cur_cmd_buf,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                rp.pipeline_layout(),
                                2,
                                1,
                                &actor_desc,
                                0,
                                nullptr
                            );

                            vkCmdDrawIndexed(
                                cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                            );
                        }
                    }
                }

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            // Shader: Transp skin
            {
                auto& rp = *rp_.transp_skin_;

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = fbuf_images_.extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<float>(fbuf_images_.width());
                viewport.height = static_cast<float>(fbuf_images_.height());
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = fbuf_images_.extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                auto desc_frame = rp_states_transp_.desc_sets_.at(
                    framesync_.get_frame_index().get()
                );
                vkCmdBindDescriptorSets(
                    cur_cmd_buf,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    rp.pipeline_layout(),
                    0,
                    1,
                    &desc_frame,
                    0,
                    nullptr
                );

                for (auto& pair : draw_sheet.skinned_pairs_) {
                    for (auto& unit : pair.model_->runits_alpha_) {
                        auto unit_desc = unit.get_desc_set(
                            framesync_.get_frame_index().get()
                        );
                        vkCmdBindDescriptorSets(
                            cur_cmd_buf,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rp.pipeline_layout(),
                            1,
                            1,
                            &unit_desc,
                            0,
                            nullptr
                        );
                        unit.record_bind_vert_buf(cur_cmd_buf);

                        for (auto& actor : pair.actors_) {
                            auto actor_desc = actor.actor_->get_desc_set(
                                framesync_.get_frame_index().get()
                            );
                            vkCmdBindDescriptorSets(
                                cur_cmd_buf,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                rp.pipeline_layout(),
                                2,
                                1,
                                &actor_desc,
                                0,
                                nullptr
                            );

                            vkCmdDrawIndexed(
                                cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                            );
                        }
                    }
                }

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            // Shader: Fillscreen
            {
                auto& rp = *rp_.fillscreen_;
                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = swapchain_.extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<float>(swapchain_.width());
                viewport.height = static_cast<float>(swapchain_.height());
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = swapchain_.extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                auto desc_main = rp_states_fillscreen_.desc_sets_.at(
                    framesync_.get_frame_index().get()
                );
                vkCmdBindDescriptorSets(
                    cur_cmd_buf,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    rp.pipeline_layout(),
                    0,
                    1,
                    &desc_main,
                    0,
                    nullptr
                );

                vkCmdDraw(cur_cmd_buf, 3, 1, 0, 0);

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            // Shader: Overlay
            {
                auto& rp = *rp_.overlay_;
                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = swapchain_.extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<float>(swapchain_.width());
                viewport.height = static_cast<float>(swapchain_.height());
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = swapchain_.extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                widget_ren_data.cmd_buf_ = cur_cmd_buf;
                widget_ren_data.pipe_layout_ = rp.pipeline_layout();
                overlay_man_.record_render(widget_ren_data);

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            if (vkEndCommandBuffer(cur_cmd_buf) != VK_SUCCESS) {
                throw std::runtime_error("failed to record a command buffer!");
            }

            // Submit and present
            {
                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

                VkSemaphore waitSemaphores[] = {
                    framesync_.get_cur_img_ava_semaph().get(),
                };
                VkPipelineStageFlags waitStages[] = {
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                };
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = waitSemaphores;
                submitInfo.pWaitDstStageMask = waitStages;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &cur_cmd_buf;

                VkSemaphore signalSemaphores[] = {
                    framesync_.get_cur_render_fin_semaph().get(),
                };
                submitInfo.signalSemaphoreCount = 1;
                submitInfo.pSignalSemaphores = signalSemaphores;

                const auto result = vkQueueSubmit(
                    device_.graphics_queue(),
                    1,
                    &submitInfo,
                    framesync_.get_cur_in_flight_fence().get()
                );
                if (VK_SUCCESS != result) {
                    throw std::runtime_error(
                        "failed to submit draw command buffer!"
                    );
                }

                std::array<uint32_t, 1> swapchain_indices{ image_index.get() };
                std::array<VkSwapchainKHR, 1> swapchains{ swapchain_.get() };

                VkPresentInfoKHR presentInfo{};
                presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pWaitSemaphores = signalSemaphores;
                presentInfo.swapchainCount = static_cast<uint32_t>(
                    swapchains.size()
                );
                presentInfo.pSwapchains = swapchains.data();
                presentInfo.pImageIndices = swapchain_indices.data();
                presentInfo.pResults = nullptr;

                vkQueuePresentKHR(device_.present_queue(), &presentInfo);
            }

            framesync_.increase_frame_index();
        }

        bool is_ongoing() override { return !quit_; }

        void notify_window_resize(uint32_t width, uint32_t height) override {
            fbuf_width_ = width;
            fbuf_height_ = height;
            fbuf_resized_ = true;
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            if (input_mgrs_.on_key_event(e))
                return true;

            return true;
        }

        bool on_text_event(char32_t c) override {
            if (input_mgrs_.on_text_event(c))
                return true;

            return true;
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            camera_controller_.osio_ = &device_.osio();

            if (input_mgrs_.on_mouse_event(e))
                return true;

            auto cam = scene_.reg_.try_get<mirinae::cpnt::StandardCamera>(
                scene_.main_camera_
            );
            if (cam) {
                constexpr auto FACTOR = 1.05;
                if (e.action_ == mirinae::mouse::ActionType::mwheel_up)
                    cam->proj_.multiply_fov(1.0 / FACTOR);
                else if (e.action_ == mirinae::mouse::ActionType::mwheel_down)
                    cam->proj_.multiply_fov(FACTOR);
            }

            return true;
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
            fbuf_images_.init(gbuf_width, gbuf_height, tex_man_);

            rp_.init(
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );

            {
                if (!shadow_map_) {
                    shadow_map_ = tex_man_.create_depth(1024 * 4, 1024 * 4);
                    overlay_man_.create_image_view(shadow_map_->image_view());
                }

                const std::vector<VkImageView> attachments{
                    shadow_map_->image_view()
                };

                VkFramebufferCreateInfo framebufferInfo{};
                framebufferInfo.sType =
                    VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = rp_.shadowmap_->renderpass();
                framebufferInfo.attachmentCount = static_cast<uint32_t>(
                    attachments.size()
                );
                framebufferInfo.pAttachments = attachments.data();
                framebufferInfo.width = shadow_map_->width();
                framebufferInfo.height = shadow_map_->height();
                framebufferInfo.layers = 1;

                const auto result = vkCreateFramebuffer(
                    device_.logi_device(),
                    &framebufferInfo,
                    nullptr,
                    &shadow_map_fbuf_
                );
                if (VK_SUCCESS != result) {
                    throw std::runtime_error("failed to create framebuffer!");
                }
            }

            rp_states_compo_.init(
                desclayout_,
                fbuf_images_,
                shadow_map_->image_view(),
                texture_sampler_.get(),
                shadow_map_sampler_.get(),
                device_
            );
            rp_states_transp_.init(desclayout_, device_);
            rp_states_fillscreen_.init(
                desclayout_, fbuf_images_, texture_sampler_.get(), device_
            );
        }

        void destroy_swapchain_and_relatives() {
            device_.wait_idle();

            vkDestroyFramebuffer(
                device_.logi_device(), shadow_map_fbuf_, nullptr
            );

            rp_states_fillscreen_.destroy(device_);
            rp_states_transp_.destroy(device_);
            rp_states_compo_.destroy(device_);

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

            auto& reg = scene_.reg_;

            for (auto eid : scene_.entt_without_model_) {
                if (const auto src = reg.try_get<cpnt::StaticModelActor>(eid)) {
                    auto model = model_man_.request_static(
                        src->model_path_, desclayout_, tex_man_
                    );
                    if (!model) {
                        spdlog::warn(
                            "Failed to load model: {}",
                            src->model_path_.u8string()
                        );
                        continue;
                    }

                    auto& d = reg.emplace<cpnt::StaticActorVk>(eid);
                    d.model_ = model;
                    d.actor_ = std::make_shared<mirinae::RenderActor>(device_);
                    d.actor_->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout_);
                } else if (const auto src = reg.try_get<SrcSkinn>(eid)) {
                    auto model = model_man_.request_skinned(
                        src->model_path_, desclayout_, tex_man_
                    );
                    if (!model) {
                        spdlog::warn(
                            "Failed to load model: {}",
                            src->model_path_.u8string()
                        );
                        continue;
                    }

                    auto& d = reg.emplace<cpnt::SkinnedActorVk>(eid);
                    d.model_ = model;
                    d.actor_ = std::make_shared<RenderActorSkinned>(device_);
                    d.actor_->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout_);
                    src->anim_state_.set_skel_anim(d.model_->skel_anim_);
                }
            }

            scene_.entt_without_model_.clear();
        }

        void update_ubufs(
            const glm::dmat4& proj_mat, const glm::dmat4& view_mat
        ) {
            namespace cpnt = mirinae::cpnt;
            const auto t = scene_.get_time().tp_;

            // Update ubuf: U_GbufActor
            scene_.reg_.view<cpnt::Transform, cpnt::StaticActorVk>().each(
                [&](auto enttid, auto& transform, auto& ren_pair) {
                    const auto model_mat = transform.make_model_mat();

                    mirinae::U_GbufActor ubuf_data;
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
            scene_.reg_
                .view<
                    cpnt::Transform,
                    cpnt::SkinnedActorVk,
                    cpnt::SkinnedModelActor>()
                .each([&](auto enttid,
                          auto& transform,
                          auto& ren_pair,
                          auto& mactor) {
                    const auto model_m = transform.make_model_mat();
                    mactor.anim_state_.update_tick(scene_.get_time());

                    mirinae::U_GbufActorSkinned ubuf_data;
                    mactor.anim_state_.sample_anim(
                        ubuf_data.joint_transforms_,
                        mirinae::MAX_JOINTS,
                        scene_.get_time()
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
                ubuf_data.set_proj_inv(glm::inverse(proj_mat));
                ubuf_data.set_view_inv(glm::inverse(view_mat));

                for (auto e : scene_.reg_.view<cpnt::DLight>()) {
                    const auto& light = scene_.reg_.get<cpnt::DLight>(e);
                    ubuf_data.set_dlight_mat(light.make_light_mat());
                    ubuf_data.set_dlight_dir(light.calc_to_light_dir(view_mat));
                    ubuf_data.set_dlight_color(light.color_);
                    break;
                }

                ubuf_data.set_slight_pos(glm::dvec3{ 0, 0, 0 });
                ubuf_data.set_slight_dir(glm::dvec3{ 0, 0, -1 });
                ubuf_data.set_slight_color(flashlight_on_ ? 5.f : 0.f);
                ubuf_data.set_slight_inner_angle(mirinae::Angle::from_deg(10));
                ubuf_data.set_slight_outer_angle(mirinae::Angle::from_deg(25));
                ubuf_data.set_slight_max_dist(10);

                rp_states_compo_.ubufs_.at(framesync_.get_frame_index().get())
                    .set_data(
                        &ubuf_data, sizeof(ubuf_data), device_.mem_alloc()
                    );

                rp_states_transp_.ubufs_.at(framesync_.get_frame_index().get())
                    .set_data(
                        &ubuf_data, sizeof(ubuf_data), device_.mem_alloc()
                    );
            }
        }

        // This must be the first member variable
        mirinae::VulkanDevice device_;
        mirinae::ScriptEngine script_;

        mirinae::Scene scene_;
        mirinae::TextureManager tex_man_;
        mirinae::ModelManager model_man_;
        mirinae::DesclayoutManager desclayout_;
        mirinae::FbufImageBundle fbuf_images_;
        mirinae::OverlayManager overlay_man_;
        mirinae::RenderPassPackage rp_;
        ::RpStatesCompo rp_states_compo_;
        ::RpStatesTransp rp_states_transp_;
        ::RpStatesFillscreen rp_states_fillscreen_;
        mirinae::Swapchain swapchain_;
        ::FrameSync framesync_;
        mirinae::CommandPool cmd_pool_;
        std::vector<VkCommandBuffer> cmd_buf_;
        mirinae::Sampler texture_sampler_;
        mirinae::syst::NoclipController camera_controller_;
        mirinae::InputProcesserMgr input_mgrs_;
        dal::TimerThatCaps fps_timer_;
        std::shared_ptr<mirinae::ITextData> dev_console_output_;

        std::unique_ptr<mirinae::ITexture> shadow_map_;
        VkFramebuffer shadow_map_fbuf_;
        mirinae::Sampler shadow_map_sampler_;

        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
        bool fbuf_resized_ = false;
        bool flashlight_on_ = false;
        bool quit_ = false;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IEngine> create_engine(
        mirinae::EngineCreateInfo&& create_info
    ) {
        const auto init_width = create_info.init_width_;
        const auto init_height = create_info.init_height_;
        return std::make_unique<EngineGlfw>(
            std::move(create_info), init_width, init_height
        );
    }

}  // namespace mirinae
