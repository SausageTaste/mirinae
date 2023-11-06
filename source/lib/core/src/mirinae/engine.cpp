#include "mirinae/engine.hpp"

#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

#include <daltools/util.h>

#include <mirinae/actor/transform.hpp>
#include <mirinae/render/pipeline.hpp>
#include <mirinae/render/vkcomposition.hpp>
#include <mirinae/util/mamath.hpp>


namespace {

    using FrameIndex = mirinae::StrongType<int, struct FrameIndexStrongTypeTag>;


    VkSurfaceKHR surface_cast(uint64_t value) {
        static_assert(sizeof(VkSurfaceKHR) == sizeof(uint64_t));
        return *reinterpret_cast<VkSurfaceKHR*>(&value);
    }

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
        void init(mirinae::LogiDevice& logi_device) {
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

        void destroy(mirinae::LogiDevice& logi_device) {
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
            : create_info_(std::move(cinfo))
        {
            // Check engine creation info
            if (!create_info_.filesys_) {
                spdlog::critical("Filesystem is not set");
                throw std::runtime_error{ "Filesystem is not set" };
            }

            mirinae::InstanceFactory instance_factory;
            if (create_info_.enable_validation_layers_) {
                instance_factory.enable_validation_layer();
                instance_factory.ext_layers_.add_validation();
            }
            instance_factory.ext_layers_.extensions_.insert(
                instance_factory.ext_layers_.extensions_.end(),
                create_info_.instance_extensions_.begin(),
                create_info_.instance_extensions_.end()
            );

            instance_.init(instance_factory);
            surface_ = ::surface_cast(create_info_.surface_creator_(instance_.get()));
            phys_device_.set(instance_.select_phys_device(surface_), surface_);
            spdlog::info("Physical device selected: {}\n{}", phys_device_.name(), phys_device_.make_report_str());

            std::vector<std::string> device_extensions;
            device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            if (phys_device_.count_unsupported_extensions(device_extensions))
                throw std::runtime_error{ "Some extensions are not supported" };

            logi_device_.init(phys_device_, device_extensions);

            mirinae::SwapChainSupportDetails swapchain_details;
            swapchain_details.init(surface_, phys_device_.get());
            if (!swapchain_details.is_complete()) {
                throw std::runtime_error{ "The swapchain is not complete" };
            }

            this->create_swapchain_and_relatives(fbuf_width_, fbuf_height_);

            cmd_pool_.init(phys_device_.graphics_family_index().value(), logi_device_);
            for (int i = 0; i < framesync_.MAX_FRAMES_IN_FLIGHT; ++i)
                cmd_buf_.push_back(cmd_pool_.alloc(logi_device_));

            desc_pool_.init(framesync_.MAX_FRAMES_IN_FLIGHT, logi_device_);
            desc_sets_ = desc_pool_.alloc(framesync_.MAX_FRAMES_IN_FLIGHT, desclayout_, logi_device_);

            // Vertices
            {
                constexpr float v = 0.5;
                std::vector<mirinae::VertexStatic> vertices;
                vertices.push_back(mirinae::VertexStatic{ glm::vec3{ -v, -v, 0}, glm::vec2{0, 0}, glm::vec3{1, 1, 0} });
                vertices.push_back(mirinae::VertexStatic{ glm::vec3{ -v,  v, 0}, glm::vec2{0, 1}, glm::vec3{0, 0, 1} });
                vertices.push_back(mirinae::VertexStatic{ glm::vec3{  v,  v, 0}, glm::vec2{1, 1}, glm::vec3{0, 0, 1} });
                vertices.push_back(mirinae::VertexStatic{ glm::vec3{  v, -v, 0}, glm::vec2{1, 0}, glm::vec3{1, 1, 1} });

                vertices.push_back(mirinae::VertexStatic{ glm::vec3{ -v, -v, -v}, glm::vec2{0, 0}, glm::vec3{1, 1, 0} });
                vertices.push_back(mirinae::VertexStatic{ glm::vec3{ -v,  v, -v}, glm::vec2{0, 1}, glm::vec3{0, 0, 1} });
                vertices.push_back(mirinae::VertexStatic{ glm::vec3{  v,  v, -v}, glm::vec2{1, 1}, glm::vec3{0, 0, 1} });
                vertices.push_back(mirinae::VertexStatic{ glm::vec3{  v, -v, -v}, glm::vec2{1, 0}, glm::vec3{1, 1, 1} });

                std::vector<uint16_t> indices{
                    0, 1, 2, 0, 2, 3,
                    3, 2, 0, 2, 1, 0,
                    4, 5, 6, 4, 6, 7,
                    7, 6, 4, 6, 5, 4,
                };

                vert_index_pair_.init(vertices, indices, cmd_pool_, phys_device_, logi_device_);
            }

            // Texture
            {
                const auto path = "textures/lorem_ipsum.png";
                std::vector<uint8_t> img_data;
                create_info_.filesys_->read_file_to_vector(path, img_data);
                const auto image = mirinae::parse_image(img_data.data(), img_data.size());

                mirinae::Buffer staging_buffer;
                staging_buffer.init(
                    image->data_size(),
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    phys_device_,
                    logi_device_
                );
                staging_buffer.set_data(image->data(), image->data_size(), logi_device_);

                texture_.init(
                    image->width(), image->height(),
                    VK_FORMAT_R8G8B8A8_SRGB,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    phys_device_,
                    logi_device_
                );
                texture_.copy_and_transition(staging_buffer, cmd_pool_, logi_device_);
                staging_buffer.destroy(logi_device_);

                texture_view_.init(texture_.image(), texture_.format(), VK_IMAGE_ASPECT_COLOR_BIT, logi_device_);
                texture_sampler_.init(phys_device_, logi_device_);
            }

            // Uniform
            {
                for (int i = 0; i < framesync_.MAX_FRAMES_IN_FLIGHT; ++i) {
                    auto& ubuf = uniform_buf_.emplace_back();
                    ubuf.init(
                        sizeof(mirinae::U_Unorthodox),
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        phys_device_,
                        logi_device_
                    );
                }
            }

            for (size_t i = 0; i < framesync_.MAX_FRAMES_IN_FLIGHT; i++) {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = uniform_buf_.at(i).buffer();
                bufferInfo.offset = 0;
                bufferInfo.range = uniform_buf_.at(i).size();

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = texture_view_.get();
                imageInfo.sampler = texture_sampler_.get();

                std::vector<VkWriteDescriptorSet> write_info{};
                {
                    auto& descriptorWrite = write_info.emplace_back();
                    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrite.dstSet = desc_sets_.at(i);
                    descriptorWrite.dstBinding = static_cast<uint32_t>(write_info.size() - 1);
                    descriptorWrite.dstArrayElement = 0;
                    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    descriptorWrite.descriptorCount = 1;
                    descriptorWrite.pBufferInfo = &bufferInfo;
                }
                {
                    auto& descriptorWrite = write_info.emplace_back();
                    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrite.dstSet = desc_sets_.at(i);
                    descriptorWrite.dstBinding = static_cast<uint32_t>(write_info.size() - 1);
                    descriptorWrite.dstArrayElement = 0;
                    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    descriptorWrite.descriptorCount = 1;
                    descriptorWrite.pImageInfo = &imageInfo;
                }

                vkUpdateDescriptorSets(logi_device_.get(), static_cast<uint32_t>(write_info.size()), write_info.data(), 0, nullptr);
            }
        }

        ~EngineGlfw() {
            this->logi_device_.wait_idle();

            for (auto& ubuf : uniform_buf_) ubuf.destroy(logi_device_); uniform_buf_.clear();
            texture_sampler_.destroy(logi_device_);
            texture_view_.destroy(logi_device_);
            texture_.destroy(logi_device_);
            vert_index_pair_.destroy(logi_device_);
            desc_pool_.destroy(logi_device_);
            cmd_pool_.destroy(logi_device_);
            this->destroy_swapchain_and_relatives();
            logi_device_.destroy();
            vkDestroySurfaceKHR(instance_.get(), surface_, nullptr); surface_ = VK_NULL_HANDLE;
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

                auto& ubuf = uniform_buf_.at(framesync_.get_frame_index().get());
                mirinae::U_Unorthodox ubuf_data;
                ubuf_data.model = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                ubuf_data.view = camera_.make_view_mat();
                ubuf_data.proj = glm::perspective(glm::radians(45.0f), swapchain_.extent().width / (float) swapchain_.extent().height, 0.1f, 10.0f);
                ubuf_data.proj[1][1] *= -1;
                ubuf.set_data(&ubuf_data, sizeof(mirinae::U_Unorthodox), logi_device_);
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

                vkCmdBindDescriptorSets(
                    cur_cmd_buf,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline_.layout(),
                    0,
                    1,
                    &desc_sets_.at(framesync_.get_frame_index().get()),
                    0,
                    nullptr
                );

                vert_index_pair_.record_bind(cur_cmd_buf);
                vkCmdDrawIndexed(cur_cmd_buf, vert_index_pair_.vertex_count(), 1, 0, 0, 0);

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

                if (vkQueueSubmit(logi_device_.graphics_queue(), 1, &submitInfo, framesync_.get_cur_in_flight_fence().get()) != VK_SUCCESS) {
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

                vkQueuePresentKHR(logi_device_.present_queue(), &presentInfo);
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
            this->logi_device_.wait_idle();
            swapchain_.init(fbuf_width, fbuf_height, surface_, phys_device_, logi_device_);

            // Depth texture
            {
                const auto depth_format = phys_device_.find_supported_format(
                    { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
                );

                depth_image_.init(
                    swapchain_.extent().width, swapchain_.extent().height,
                    depth_format,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    phys_device_,
                    logi_device_
                );
                depth_image_view_.init(depth_image_.image(), depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, logi_device_);
            }

            framesync_.init(logi_device_);
            renderpass_.init(swapchain_.format(), depth_image_.format(), logi_device_);
            desclayout_.init(logi_device_);
            pipeline_ = mirinae::create_unorthodox_pipeline(swapchain_.extent(), renderpass_, desclayout_, *create_info_.filesys_, logi_device_);

            swapchain_fbufs_.resize(swapchain_.views_count());
            for (size_t i = 0; i < swapchain_fbufs_.size(); ++i) {
                swapchain_fbufs_[i].init(swapchain_.extent(), swapchain_.view_at(i), depth_image_view_.get(), renderpass_, logi_device_);
            }
        }

        void destroy_swapchain_and_relatives() {
            this->logi_device_.wait_idle();

            for (auto& x : swapchain_fbufs_) x.destroy(logi_device_); swapchain_fbufs_.clear();
            pipeline_.destroy(logi_device_);
            desclayout_.destroy(logi_device_);
            renderpass_.destroy(logi_device_);
            framesync_.destroy(logi_device_);
            depth_image_view_.destroy(logi_device_);
            depth_image_.destroy(logi_device_);
            swapchain_.destroy(logi_device_);
        }

        std::optional<mirinae::ShainImageIndex> try_acquire_image() {
            framesync_.get_cur_in_flight_fence().wait(logi_device_);

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

            const auto image_index_opt = swapchain_.acquire_next_image(framesync_.get_cur_img_ava_semaph(), logi_device_);
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

            framesync_.get_cur_in_flight_fence().reset(logi_device_);
            return image_index_opt.value();
        }

        mirinae::EngineCreateInfo create_info_;

        mirinae::VulkanInstance instance_;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
        mirinae::PhysDevice phys_device_;
        mirinae::LogiDevice logi_device_;
        mirinae::Swapchain swapchain_;
        ::FrameSync framesync_;
        mirinae::DescriptorSetLayout desclayout_;
        mirinae::Pipeline pipeline_;
        mirinae::RenderPass renderpass_;
        std::vector<mirinae::Framebuffer> swapchain_fbufs_;
        mirinae::CommandPool cmd_pool_;
        std::vector<VkCommandBuffer> cmd_buf_;
        mirinae::DescriptorPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
        mirinae::VertexIndexPair vert_index_pair_;
        mirinae::TextureImage texture_;
        mirinae::ImageView texture_view_;
        mirinae::Sampler texture_sampler_;
        mirinae::TextureImage depth_image_;
        mirinae::ImageView depth_image_view_;
        std::vector<mirinae::Buffer> uniform_buf_;
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
