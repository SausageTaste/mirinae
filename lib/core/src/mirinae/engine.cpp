#include "mirinae/engine.hpp"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>

#include <daltools/util.h>
#include <sung/general/time.hpp>

#include <mirinae/render/overlay.hpp>
#include <mirinae/render/renderpass.hpp>
#include <mirinae/util/mamath.hpp>
#include "mirinae/util/script.hpp"
#include "mirinae/util/skin_anim.hpp"


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


    struct DrawSheet {
        struct RenderPairs {
            std::shared_ptr<mirinae::RenderModel> model_;
            std::vector<std::shared_ptr<mirinae::RenderActor>> actors_;
        };

        struct SkinnedRenderPairs {
            std::shared_ptr<mirinae::RenderModelSkinned> model_;
            std::vector<std::shared_ptr<mirinae::RenderActorSkinned>> actors_;
        };

        std::vector<RenderPairs> ren_pairs_;
        std::vector<SkinnedRenderPairs> skinned_pairs_;
    };


    class RpStatesComposition {

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
                desclayouts.get("composition:main"),
                device.logi_device()
            );

            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& ubuf = ubufs_.emplace_back();
                ubuf.init_ubuf(
                    sizeof(mirinae::U_CompositionMain), device.mem_alloc()
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
                        fbufs.composition().image_view(),
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


    class EngineGlfw : public mirinae::IEngine {

    public:
        EngineGlfw(
            mirinae::EngineCreateInfo&& cinfo, int init_width, int init_height
        )
            : device_(std::move(cinfo))
            , tex_man_(device_)
            , model_man_(device_)
            , desclayout_(device_)
            , texture_sampler_(device_)
            , overlay_man_(
                  init_width, init_height, desclayout_, tex_man_, device_
              )
            , fbuf_width_(init_width)
            , fbuf_height_(init_height) {
            // This must be the first member variable right after vtable pointer
            static_assert(offsetof(EngineGlfw, device_) == sizeof(void*));

            framesync_.init(device_.logi_device());

            mirinae::SamplerBuilder sampler_builder;
            texture_sampler_.reset(sampler_builder.build(device_));

            this->create_swapchain_and_relatives(fbuf_width_, fbuf_height_);

            cmd_pool_.init(
                device_.graphics_queue_family_index().value(),
                device_.logi_device()
            );
            for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i)
                cmd_buf_.push_back(cmd_pool_.alloc(device_.logi_device()));

            const glm::dvec3 world_shift = { 10000000000000, 1000000000000, 0 };
            camera_view_.pos_ = world_shift;

            {
                const std::vector<mirinae::respath_t> mesh_paths{
                    "asset/models/sponza/sponza.dmd",
                };
                const std::vector<float> model_scales{
                    0.01f,
                };

                for (size_t i = 0; i < mesh_paths.size(); ++i) {
                    const auto& model_path = mesh_paths.at(i);
                    auto model = model_man_.request_static(
                        model_path, desclayout_, tex_man_
                    );
                    if (!model) {
                        spdlog::warn(
                            "Failed to load model: {}", model_path.u8string()
                        );
                        continue;
                    }

                    auto& ren_pair = draw_sheet_.ren_pairs_.emplace_back();
                    ren_pair.model_ = model;

                    auto& actor = ren_pair.actors_.emplace_back(
                        std::make_shared<mirinae::RenderActor>(device_)
                    );
                    actor->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout_);
                    actor->transform_.pos_ = glm::dvec3(i * 3, 0, 0) +
                                             world_shift;
                    actor->transform_.scale_ = glm::dvec3(model_scales[i]);
                }
            }

            {
                const std::vector<mirinae::respath_t> mesh_paths{
                    "Sung/artist/artist_subset.dmd",
                    "ThinMatrix/Character Running.dmd",
                };
                const std::vector<float> model_scales{
                    1.4f / 1.01229f,
                    1.8f / 8.6787f,
                };

                for (size_t i = 0; i < mesh_paths.size(); ++i) {
                    const auto& model_path = mesh_paths.at(i);
                    auto model = model_man_.request_skinned(
                        model_path, desclayout_, tex_man_
                    );
                    if (!model) {
                        spdlog::warn(
                            "Failed to load model: {}", model_path.u8string()
                        );
                        continue;
                    }

                    auto& ren_pair = draw_sheet_.skinned_pairs_.emplace_back();
                    ren_pair.model_ = model;

                    auto& actor = ren_pair.actors_.emplace_back(
                        std::make_shared<mirinae::RenderActorSkinned>(device_)
                    );
                    actor->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout_);
                    actor->transform_.pos_ = glm::dvec3(i * 3, 0, 0) +
                                             world_shift;
                    actor->transform_.scale_ = glm::dvec3(1.f);
                }
            }

            overlay_man_.add_widget_test();

            camera_view_.pos_ = glm::dvec3{
                -7.7109375, 3.4878, 0.8025601655244827
            } + world_shift;
            camera_view_.rot_ = {
                0.8120552484612948, 0, -0.5835805629443673, 0
            };
        }

        ~EngineGlfw() {
            device_.wait_idle();

            cmd_pool_.destroy(device_.logi_device());
            this->destroy_swapchain_and_relatives();
            framesync_.destroy(device_.logi_device());
        }

        void do_frame() override {
            const auto delta_time = fps_timer_.check_get_elapsed();
            camera_controller_.apply(
                camera_view_, static_cast<float>(delta_time)
            );

            const auto image_index_opt = this->try_acquire_image();
            if (!image_index_opt) {
                return;
            }
            const auto image_index = image_index_opt.value();

            // Update uniform
            {
                const auto proj_mat = camera_proj_.make_proj_mat(
                    swapchain_.width(), swapchain_.height()
                );
                const auto view_mat = camera_view_.make_view_mat();

                for (auto& pair : draw_sheet_.ren_pairs_) {
                    for (auto& actor : pair.actors_) {
                        mirinae::U_GbufActor ubuf_data;
                        const auto model_mat = actor->transform_.make_model_mat(
                        );
                        ubuf_data.view_model = view_mat * model_mat;
                        ubuf_data.pvm = proj_mat * view_mat * model_mat;
                        actor->udpate_ubuf(
                            framesync_.get_frame_index().get(),
                            ubuf_data,
                            device_.mem_alloc()
                        );
                    }
                }

                for (auto& pair : draw_sheet_.skinned_pairs_) {
                    for (auto& actor : pair.actors_) {
                        mirinae::U_GbufActorSkinned ubuf_data;
                        const auto model_m = actor->transform_.make_model_mat();
                        ubuf_data.view_model = view_mat * model_m;
                        ubuf_data.pvm = proj_mat * view_mat * model_m;

                        const auto& anim = pair.model_->animations_.front();
                        const auto anim_tick =
                            (sung::CalenderTime::from_now().to_total_seconds() *
                             anim.ticks_per_sec_);
                        const auto skin_mats = mirinae::make_skinning_matrix(
                            anim_tick, pair.model_->skeleton_, anim
                        );
                        const auto skin_mat_size = std::min<int>(
                            skin_mats.size(), mirinae::MAX_JOINTS
                        );
                        for (int i = 0; i < skin_mat_size; ++i) {
                            ubuf_data.joint_transforms_[i] = skin_mats[i];
                        }

                        actor->udpate_ubuf(
                            framesync_.get_frame_index().get(),
                            ubuf_data,
                            device_.mem_alloc()
                        );
                    }
                }

                {
                    mirinae::U_CompositionMain ubuf_data;
                    ubuf_data.proj_inv = glm::inverse(proj_mat);

                    rp_states_composition_.ubufs_
                        .at(framesync_.get_frame_index().get())
                        .set_data(
                            &ubuf_data, sizeof(ubuf_data), device_.mem_alloc()
                        );
                }
            }

            auto cur_cmd_buf = cmd_buf_.at(framesync_.get_frame_index().get());
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

            {
                auto& rp = *rp_gbuf_;

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

                for (auto& pair : draw_sheet_.ren_pairs_) {
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
                            auto actor_desc = actor->get_desc_set(
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

            {
                auto& rp = *rp_gbuf_skin_;

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

                for (auto& pair : draw_sheet_.skinned_pairs_) {
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
                            auto actor_desc = actor->get_desc_set(
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

            {
                auto& rp = *rp_composition_;
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

                auto desc_main = rp_states_composition_.desc_sets_.at(
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

            {
                auto& rp = *rp_transparent_;

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

                for (auto& pair : draw_sheet_.ren_pairs_) {
                    for (auto& unit : pair.model_->render_units_alpha_) {
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
                            auto actor_desc = actor->get_desc_set(
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

            {
                auto& rp = *rp_transparent_skin_;

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

                for (auto& pair : draw_sheet_.skinned_pairs_) {
                    for (auto& unit : pair.model_->runits_alpha_) {
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
                            auto actor_desc = actor->get_desc_set(
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

            {
                auto& rp = *rp_fillscreen_;
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

            {
                auto& rp = *rp_overlay_;
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

                overlay_man_.record_render(
                    framesync_.get_frame_index().get(),
                    cur_cmd_buf,
                    rp.pipeline_layout()
                );
                vkCmdEndRenderPass(cur_cmd_buf);
            }

            {
                if (vkEndCommandBuffer(cur_cmd_buf) != VK_SUCCESS) {
                    throw std::runtime_error(
                        "failed to record a command buffer!"
                    );
                }
            }

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

        bool is_ongoing() override { return true; }

        void notify_window_resize(uint32_t width, uint32_t height) override {
            fbuf_width_ = width;
            fbuf_height_ = height;
            fbuf_resized_ = true;
        }

        void notify_key_event(const mirinae::key::Event& e) override {
            if (overlay_man_.on_key_event(e))
                return;
            camera_controller_.on_key_event(e);
        }

        void notify_mouse_event(const mirinae::mouse::Event& e) override {
            if (overlay_man_.on_mouse_event(e))
                return;
            camera_controller_.on_mouse_event(e);
        }

    private:
        void create_swapchain_and_relatives(
            uint32_t fbuf_width, uint32_t fbuf_height
        ) {
            device_.wait_idle();
            swapchain_.init(fbuf_width, fbuf_height, device_);

            const auto [gbuf_width, gbuf_height] = ::calc_scaled_dimensions(
                swapchain_.width(), swapchain_.height(), 2
            );
            fbuf_images_.init(gbuf_width, gbuf_height, tex_man_);

            rp_gbuf_ = mirinae::create_gbuf(
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );
            rp_gbuf_skin_ = mirinae::create_gbuf_skin(
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );
            rp_composition_ = mirinae::create_composition(
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );
            rp_transparent_ = mirinae::create_transparent(
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );
            rp_transparent_skin_ = mirinae::create_transparent_skin(
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );
            rp_fillscreen_ = mirinae::create_fillscreen(
                swapchain_.width(),
                swapchain_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );
            rp_overlay_ = mirinae::create_overlay(
                swapchain_.width(),
                swapchain_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );

            rp_states_composition_.init(
                desclayout_, fbuf_images_, texture_sampler_.get(), device_
            );
            rp_states_fillscreen_.init(
                desclayout_, fbuf_images_, texture_sampler_.get(), device_
            );
        }

        void destroy_swapchain_and_relatives() {
            device_.wait_idle();

            rp_states_fillscreen_.destroy(device_);
            rp_states_composition_.destroy(device_);

            rp_overlay_.reset();
            rp_fillscreen_.reset();
            rp_transparent_skin_.reset();
            rp_transparent_.reset();
            rp_composition_.reset();
            rp_gbuf_skin_.reset();
            rp_gbuf_.reset();

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

        // This must be the first member variable
        mirinae::VulkanDevice device_;

        mirinae::TextureManager tex_man_;
        mirinae::ModelManager model_man_;
        mirinae::DesclayoutManager desclayout_;
        mirinae::FbufImageBundle fbuf_images_;
        mirinae::OverlayManager overlay_man_;
        std::unique_ptr<mirinae::IRenderPassBundle> rp_gbuf_;
        std::unique_ptr<mirinae::IRenderPassBundle> rp_gbuf_skin_;
        std::unique_ptr<mirinae::IRenderPassBundle> rp_composition_;
        std::unique_ptr<mirinae::IRenderPassBundle> rp_transparent_;
        std::unique_ptr<mirinae::IRenderPassBundle> rp_transparent_skin_;
        std::unique_ptr<mirinae::IRenderPassBundle> rp_fillscreen_;
        std::unique_ptr<mirinae::IRenderPassBundle> rp_overlay_;
        ::RpStatesComposition rp_states_composition_;
        ::RpStatesFillscreen rp_states_fillscreen_;
        ::DrawSheet draw_sheet_;
        mirinae::Swapchain swapchain_;
        ::FrameSync framesync_;
        mirinae::CommandPool cmd_pool_;
        std::vector<VkCommandBuffer> cmd_buf_;
        mirinae::Sampler texture_sampler_;
        mirinae::cpnt::Transform camera_view_;
        mirinae::PerspectiveCamera<double> camera_proj_;
        mirinae::syst::NoclipController camera_controller_;
        dal::Timer fps_timer_;

        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
        bool fbuf_resized_ = false;

        mirinae::ScriptEngine script_;
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
