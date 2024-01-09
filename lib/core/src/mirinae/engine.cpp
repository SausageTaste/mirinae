#include "mirinae/engine.hpp"

#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

#include <daltools/util.h>

#include <mirinae/render/renderpass.hpp>
#include <mirinae/util/mamath.hpp>


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


    class FrameSync {

    public:
        void init(VkDevice logi_device) {
            this->destroy(logi_device);

            for (auto& x : img_available_semaphores_)
                x.init(logi_device);
            for (auto& x : render_finished_semaphores_)
                x.init(logi_device);
            for (auto& x : in_flight_fences_)
                x.init(true, logi_device);
        }

        void destroy(VkDevice logi_device) {
            for (auto& x : img_available_semaphores_)
                x.destroy(logi_device);
            for (auto& x : render_finished_semaphores_)
                x.destroy(logi_device);
            for (auto& x : in_flight_fences_)
                x.destroy(logi_device);
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
        std::array<mirinae::Semaphore, mirinae::MAX_FRAMES_IN_FLIGHT> img_available_semaphores_;
        std::array<mirinae::Semaphore, mirinae::MAX_FRAMES_IN_FLIGHT> render_finished_semaphores_;
        std::array<mirinae::Fence, mirinae::MAX_FRAMES_IN_FLIGHT> in_flight_fences_;
        FrameIndex cur_frame_{ 0 };

    };


    struct DrawSheet {
        struct RenderPairs {
            std::shared_ptr<mirinae::RenderModel> model_;
            std::vector<std::shared_ptr<mirinae::RenderActor>> actors_;
        };

        std::vector<RenderPairs> ren_pairs_;
    };


    class EngineGlfw : public mirinae::IEngine {

    public:
        EngineGlfw(mirinae::EngineCreateInfo&& cinfo)
            : device_(std::move(cinfo))
            , tex_man_(device_)
            , model_man_(device_)
            , desclayout_(device_)
        {
            framesync_.init(device_.logi_device());
            this->create_swapchain_and_relatives(fbuf_width_, fbuf_height_);

            cmd_pool_.init(device_.graphics_queue_family_index().value(), device_.logi_device());
            for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i)
                cmd_buf_.push_back(cmd_pool_.alloc(device_.logi_device()));

            // Texture
            texture_sampler_.init(
                device_.is_anisotropic_filtering_supported(),
                device_.max_sampler_anisotropy(),
                device_.logi_device()
            );

            const std::vector<mirinae::respath_t> mesh_paths{
                "sponza/sponza.dmd",
                "honoka sugar perfume/DOAXVV Honoka - Sugar Perfume.dmd",
                "artist/artist_de_subset.dmd",
            };
            const std::vector<float> model_scales{
                0.01f,
                1,
                1,
            };

            for (size_t i = 0; i < mesh_paths.size(); ++i) {
                const auto& model_path = mesh_paths.at(i);
                auto model = model_man_.request_static(model_path, desclayout_, tex_man_);
                if (!model) {
                    spdlog::error("Failed to load model: {}", model_path);
                    continue;
                }

                auto& ren_pair = draw_sheet_.ren_pairs_.emplace_back();
                ren_pair.model_ = model;

                auto& actor = ren_pair.actors_.emplace_back(std::make_shared<mirinae::RenderActor>(device_));
                actor->init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    desclayout_
                );
                actor->transform_.pos_ = glm::vec3(i*3, 0, 0);
                actor->transform_.scale_ = glm::vec3(model_scales[i]);
            }
        }

        ~EngineGlfw() {
            device_.wait_idle();

            texture_sampler_.destroy(device_.logi_device());
            cmd_pool_.destroy(device_.logi_device());
            this->destroy_swapchain_and_relatives();
            framesync_.destroy(device_.logi_device());
        }

        void do_frame() override {
            const auto delta_time = fps_timer_.check_get_elapsed();
            camera_controller_.apply(camera_view_, static_cast<float>(delta_time));

            const auto image_index_opt = this->try_acquire_image();
            if (!image_index_opt) {
                return;
            }
            const auto image_index = image_index_opt.value();

            // Update uniform
            {
                const auto proj_mat = camera_proj_.make_proj_mat(swapchain_.extent().width, swapchain_.extent().height);
                const auto view_mat = camera_view_.make_view_mat();

                for (auto& pair : draw_sheet_.ren_pairs_) {
                    for (auto& actor : pair.actors_) {
                        actor->udpate_ubuf(framesync_.get_frame_index().get(), view_mat, proj_mat, device_.mem_alloc());
                    }
                }
            }

            auto cur_cmd_buf = cmd_buf_.at(framesync_.get_frame_index().get());
            {
                vkResetCommandBuffer(cur_cmd_buf, 0);

                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = 0;
                beginInfo.pInheritanceInfo = nullptr;

                if (vkBeginCommandBuffer(cur_cmd_buf, &beginInfo) != VK_SUCCESS) {
                    throw std::runtime_error("failed to begin recording command buffer!");
                }

                std::array<VkClearValue, 4> clear_values;
                clear_values[0].color = { 0.f, 0.f, 0.f, 1.f };
                clear_values[1].depthStencil = { 1.f, 0 };
                clear_values[2].color = { 0.f, 0.f, 0.f, 1.f };
                clear_values[3].color = { 0.f, 0.f, 0.f, 1.f };

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp_unorthodox_->renderpass();
                renderPassInfo.framebuffer = rp_unorthodox_->fbuf_at(image_index.get());
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = swapchain_.extent();
                renderPassInfo.clearValueCount = static_cast<uint32_t>(clear_values.size());
                renderPassInfo.pClearValues = clear_values.data();

                vkCmdBeginRenderPass(cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp_unorthodox_->pipeline());

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<float>(swapchain_.extent().width);
                viewport.height = static_cast<float>(swapchain_.extent().height);
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = swapchain_.extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                for (auto& pair : draw_sheet_.ren_pairs_) {
                    for (auto& unit : pair.model_->render_units_) {
                        auto unit_desc = unit.get_desc_set(framesync_.get_frame_index().get());
                        vkCmdBindDescriptorSets(
                            cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rp_unorthodox_->pipeline_layout(),
                            0,
                            1, &unit_desc,
                            0, nullptr
                        );
                        unit.record_bind_vert_buf(cur_cmd_buf);

                        for (auto& actor : pair.actors_) {
                            auto actor_desc = actor->get_desc_set(framesync_.get_frame_index().get());
                            vkCmdBindDescriptorSets(
                                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                rp_unorthodox_->pipeline_layout(),
                                1,
                                1, &actor_desc,
                                0, nullptr
                            );

                            vkCmdDrawIndexed(cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0);
                        }
                    }
                }

                vkCmdEndRenderPass(cur_cmd_buf);

                if (vkEndCommandBuffer(cur_cmd_buf) != VK_SUCCESS) {
                    throw std::runtime_error("failed to record command buffer!");
                }
            }

            {
                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

                VkSemaphore waitSemaphores[] = { framesync_.get_cur_img_ava_semaph().get() };
                VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = waitSemaphores;
                submitInfo.pWaitDstStageMask = waitStages;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &cur_cmd_buf;

                VkSemaphore signalSemaphores[] = { framesync_.get_cur_render_fin_semaph().get() };
                submitInfo.signalSemaphoreCount = 1;
                submitInfo.pSignalSemaphores = signalSemaphores;

                if (vkQueueSubmit(device_.graphics_queue(), 1, &submitInfo, framesync_.get_cur_in_flight_fence().get()) != VK_SUCCESS) {
                    throw std::runtime_error("failed to submit draw command buffer!");
                }

                std::array<uint32_t, 1> swapchain_indices{ image_index.get() };
                std::array<VkSwapchainKHR, 1> swapchains{ swapchain_.get() };

                VkPresentInfoKHR presentInfo{};
                presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pWaitSemaphores = signalSemaphores;
                presentInfo.swapchainCount = static_cast<uint32_t>(swapchains.size());
                presentInfo.pSwapchains = swapchains.data();
                presentInfo.pImageIndices = swapchain_indices.data();
                presentInfo.pResults = nullptr;

                vkQueuePresentKHR(device_.present_queue(), &presentInfo);
            }

            framesync_.increase_frame_index();
        }

        bool is_ongoing() override {
            return true;
        }

        void notify_window_resize(uint32_t width, uint32_t height) override {
            fbuf_width_ = width;;
            fbuf_height_ = height;
            fbuf_resized_ = true;
        }

        void notify_key_event(const mirinae::key::Event& e) override {
            camera_controller_.on_key_event(e);
        }

    private:
        void create_swapchain_and_relatives(uint32_t fbuf_width, uint32_t fbuf_height) {
            device_.wait_idle();
            swapchain_.init(fbuf_width, fbuf_height, device_);
            const auto extent = swapchain_.extent();

            fbuf_images_.init(extent.width, extent.height, tex_man_);
            rp_unorthodox_ = mirinae::create_unorthodox(extent.width, extent.height, fbuf_images_, desclayout_, swapchain_, device_);
        }

        void destroy_swapchain_and_relatives() {
            device_.wait_idle();
            rp_unorthodox_.reset();
            swapchain_.destroy(device_.logi_device());
        }

        std::optional<mirinae::ShainImageIndex> try_acquire_image() {
            framesync_.get_cur_in_flight_fence().wait(device_.logi_device());

            if (fbuf_resized_) {
                if (::is_fbuf_too_small(fbuf_width_, fbuf_height_)) {
                    fbuf_resized_ = true;
                }
                else {
                    fbuf_resized_ = false;
                    this->destroy_swapchain_and_relatives();
                    this->create_swapchain_and_relatives(fbuf_width_, fbuf_height_);
                }
                return std::nullopt;
            }

            const auto image_index_opt = swapchain_.acquire_next_image(framesync_.get_cur_img_ava_semaph().get(), device_.logi_device());
            if (!image_index_opt) {
                if (::is_fbuf_too_small(fbuf_width_, fbuf_height_)) {
                    fbuf_resized_ = true;
                }
                else {
                    fbuf_resized_ = false;
                    this->destroy_swapchain_and_relatives();
                    this->create_swapchain_and_relatives(fbuf_width_, fbuf_height_);
                }
                return std::nullopt;
            }

            framesync_.get_cur_in_flight_fence().reset(device_.logi_device());
            return image_index_opt.value();
        }

        mirinae::VulkanDevice device_;  // This must be the first member variable
        mirinae::TextureManager tex_man_;
        mirinae::ModelManager model_man_;
        mirinae::DesclayoutManager desclayout_;
        mirinae::FbufImageBundle fbuf_images_;
        std::unique_ptr<mirinae::IRenderPassBundle> rp_unorthodox_;
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

    };

}


namespace mirinae {

    std::unique_ptr<IEngine> create_engine(mirinae::EngineCreateInfo&& create_info) {
        return std::make_unique<EngineGlfw>(std::move(create_info));
    }

}
