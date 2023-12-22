#include "mirinae/engine.hpp"

#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

#include <daltools/util.h>

#include <mirinae/render/pipeline.hpp>
#include <mirinae/render/renderee.hpp>
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
            img_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
            render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
            in_flight_fences_.resize(MAX_FRAMES_IN_FLIGHT);

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

            img_available_semaphores_.clear();
            render_finished_semaphores_.clear();
            in_flight_fences_.clear();
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
            cur_frame_ = (cur_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        constexpr static int MAX_FRAMES_IN_FLIGHT = 2;

    private:
        std::vector<mirinae::Semaphore> img_available_semaphores_;
        std::vector<mirinae::Semaphore> render_finished_semaphores_;
        std::vector<mirinae::Fence> in_flight_fences_;
        FrameIndex cur_frame_{ 0 };

    };


    class EngineGlfw : public mirinae::IEngine {

    public:
        EngineGlfw(mirinae::EngineCreateInfo&& cinfo)
            : device_(std::move(cinfo))
            , tex_man_(device_)
        {
            this->create_swapchain_and_relatives(fbuf_width_, fbuf_height_);

            cmd_pool_.init(device_.graphics_queue_family_index().value(), device_.logi_device());
            for (int i = 0; i < framesync_.MAX_FRAMES_IN_FLIGHT; ++i)
                cmd_buf_.push_back(cmd_pool_.alloc(device_.logi_device()));

            // Texture
            texture_sampler_.init(
                device_.is_anisotropic_filtering_supported(),
                device_.max_sampler_anisotropy(),
                device_.logi_device()
            );

            const std::vector<std::string> texture_paths{
                "textures/grass1.tga",
                "textures/iceland_heightmap.png",
                "textures/lorem_ipsum.png",
                "textures/missing_texture.png",
            };

            const std::vector<std::string> mesh_paths{
                "models/cube.dmd",
                "models/sphere.dmd",
                "models/suzanne.dmd",
            };
            std::vector<mirinae::VerticesStaticPair> meshes;
            for (auto& mesh_path : mesh_paths) {
                const auto content = device_.filesys().read_file_to_vector(mesh_path.c_str());
                meshes.push_back(mirinae::parse_dmd_static(content->data(), content->size()).value());
            }

            /*
            constexpr float v = 0.5;
            mirinae::VerticesStaticPair vertices;
            vertices.vertices_.push_back(mirinae::VertexStatic{ glm::vec3{ -v, -v, 0 }, glm::vec2{0, 0}, glm::vec3{1, 1, 0} });
            vertices.vertices_.push_back(mirinae::VertexStatic{ glm::vec3{ -v,  v, 0 }, glm::vec2{0, 1}, glm::vec3{0, 0, 1} });
            vertices.vertices_.push_back(mirinae::VertexStatic{ glm::vec3{  v,  v, 0 }, glm::vec2{1, 1}, glm::vec3{0, 0, 1} });
            vertices.vertices_.push_back(mirinae::VertexStatic{ glm::vec3{  v, -v, 0 }, glm::vec2{1, 0}, glm::vec3{1, 1, 1} });

            vertices.indices_ = std::vector<uint16_t>{
                0, 1, 2, 0, 2, 3,
                3, 2, 0, 2, 1, 0,
            };
            */

            for (int i = 0; i < 20; ++i) {
                auto texture = tex_man_.request(texture_paths.at(i % texture_paths.size()));

                auto& ren_unit = render_units_.emplace_back();
                ren_unit.init(
                    framesync_.MAX_FRAMES_IN_FLIGHT,
                    meshes.at(i % meshes.size()),
                    texture->image_view(),
                    texture_sampler_.get(),
                    cmd_pool_,
                    desclayout_,
                    device_
                );

                ren_unit.transform_.pos_ = glm::vec3{ 2.5 * i, 0, 0 };
            }
        }

        ~EngineGlfw() {
            device_.wait_idle();

            for (auto& ren_unit : render_units_)
                ren_unit.destroy(device_.mem_alloc(), device_.logi_device());
            render_units_.clear();

            texture_sampler_.destroy(device_.logi_device());
            cmd_pool_.destroy(device_.logi_device());
            this->destroy_swapchain_and_relatives();
        }

        void do_frame() override {
            const auto delta_time = fps_timer_.check_get_elapsed();
            camera_controller_.apply(camera_, static_cast<float>(delta_time));

            const auto image_index_opt = this->try_acquire_image();
            if (!image_index_opt) {
                return;
            }
            const auto image_index = image_index_opt.value();

            // Update uniform
            {
                static const auto startTime = std::chrono::high_resolution_clock::now();
                const auto currentTime = std::chrono::high_resolution_clock::now();
                const auto time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

                auto proj_mat = glm::perspective(glm::radians(45.0f), swapchain_.extent().width / (float) swapchain_.extent().height, 0.1f, 100.0f);
                proj_mat[1][1] *= -1;

                for (size_t i = 0; i < render_units_.size(); ++i) {
                    render_units_.at(i).udpate_ubuf(
                        framesync_.get_frame_index().get(), camera_.make_view_mat(), proj_mat, device_.mem_alloc()
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

                if (vkBeginCommandBuffer(cur_cmd_buf, &beginInfo) != VK_SUCCESS) {
                    throw std::runtime_error("failed to begin recording command buffer!");
                }

                std::array<VkClearValue, 2> clear_values;
                clear_values[0].color = { 0.f, 0.f, 0.f, 1.f };
                clear_values[1].depthStencil = { 1.f, 0 };

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = renderpass_.get();
                renderPassInfo.framebuffer = swapchain_fbufs_[image_index.get()].get();
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = swapchain_.extent();
                renderPassInfo.clearValueCount = static_cast<uint32_t>(clear_values.size());
                renderPassInfo.pClearValues = clear_values.data();

                vkCmdBeginRenderPass(cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.pipeline());

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

                for (auto& ren_unit : render_units_) {
                    std::array<VkDescriptorSet, 1> desc_sets{
                        ren_unit.get_desc_set(framesync_.get_frame_index().get())
                    };
                    vkCmdBindDescriptorSets(
                        cur_cmd_buf,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipeline_.layout(),
                        0,
                        static_cast<uint32_t>(desc_sets.size()),
                        desc_sets.data(),
                        0,
                        nullptr
                    );

                    ren_unit.record_bind_vert_buf(cur_cmd_buf);
                    vkCmdDrawIndexed(cur_cmd_buf, ren_unit.vertex_count(), 1, 0, 0, 0);
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

            // Depth texture
            {
                const auto depth_format = device_.find_supported_format(
                    { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
                );

                depth_image_.init_depth(  swapchain_.extent().width, swapchain_.extent().height, depth_format, device_.mem_alloc());
                depth_image_view_.init(depth_image_.image(), depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, device_.logi_device());
            }

            framesync_.init(device_.logi_device());
            renderpass_.init(swapchain_.format(), depth_image_.format(), device_.logi_device());
            desclayout_.init(device_.logi_device());
            pipeline_ = mirinae::create_unorthodox_pipeline(swapchain_.extent(), renderpass_, desclayout_, device_.filesys(), device_.logi_device());

            swapchain_fbufs_.resize(swapchain_.views_count());
            for (size_t i = 0; i < swapchain_fbufs_.size(); ++i) {
                swapchain_fbufs_[i].init(swapchain_.extent(), swapchain_.view_at(i), depth_image_view_.get(), renderpass_, device_.logi_device());
            }
        }

        void destroy_swapchain_and_relatives() {
            device_.wait_idle();

            for (auto& x : swapchain_fbufs_) x.destroy(device_.logi_device()); swapchain_fbufs_.clear();
            pipeline_.destroy(device_.logi_device());
            desclayout_.destroy(device_.logi_device());
            renderpass_.destroy(device_.logi_device());
            framesync_.destroy(device_.logi_device());
            depth_image_view_.destroy(device_.logi_device());
            depth_image_.destroy(device_.mem_alloc());
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

        mirinae::VulkanDevice device_;
        mirinae::TextureManager tex_man_;
        mirinae::Swapchain swapchain_;
        ::FrameSync framesync_;
        mirinae::DescriptorSetLayout desclayout_;
        mirinae::Pipeline pipeline_;
        mirinae::RenderPass renderpass_;
        std::vector<mirinae::Framebuffer> swapchain_fbufs_;
        mirinae::CommandPool cmd_pool_;
        std::vector<VkCommandBuffer> cmd_buf_;
        std::vector<mirinae::RenderUnit> render_units_;
        mirinae::Sampler texture_sampler_;
        mirinae::Image depth_image_;
        mirinae::ImageView depth_image_view_;
        mirinae::TransformQuat camera_;
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
