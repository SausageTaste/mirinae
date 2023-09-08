#include "mirinae/engine.hpp"

#include <spdlog/spdlog.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <mirinae/render/pipeline.hpp>
#include <mirinae/render/vkmajorplayers.hpp>
#include <mirinae/util/filesys.hpp>


namespace {

    auto get_glfw_extensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        return std::vector<std::string>{ glfwExtensions, glfwExtensions + glfwExtensionCount };
    }


    class GlfwRaii {

    public:
        GlfwRaii() {
            glfwInit();
            spdlog::set_level(spdlog::level::level_enum::trace);
        }

        ~GlfwRaii() {
            glfwTerminate();
        }

    } g_glfw_raii;


    class GlfwWindow {

    public:
        GlfwWindow(void* userdata) {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
            this->window = glfwCreateWindow(800, 450, "Mirinapp", nullptr, nullptr);

            glfwSetWindowUserPointer(this->window, userdata);
            glfwSetFramebufferSizeCallback(this->window, GlfwWindow::callback_fbuf_size);
            glfwSetKeyCallback(this->window, GlfwWindow::callback_key);
        }

        ~GlfwWindow() {
            if (nullptr != this->window) {
                glfwDestroyWindow(this->window);
                this->window = nullptr;
            }
        }

        void swap_buffer() {
            glfwSwapBuffers(this->window);
        }

        bool is_ongoing() const {
            return !glfwWindowShouldClose(this->window);
        }

        VkSurfaceKHR create_surface(const mirinae::VulkanInstance& instance) {
            VkSurfaceKHR surface = nullptr;
            if (VK_SUCCESS != glfwCreateWindowSurface(instance.get(), window, nullptr, &surface)) {
                spdlog::error("Failed to create window surface");
                return nullptr;
            }
            return surface;
        }

        std::pair<int, int> get_fbuf_size() const {
            std::pair<int, int> output{ 0, 0 };
            if (nullptr == this->window)
                return output;

            glfwGetFramebufferSize(window, &output.first, &output.second);
            return output;
        }

        bool is_fbuf_too_small() const {
            const auto [width, height] = this->get_fbuf_size();
            if (width < 5)
                return true;
            if (height < 5)
                return true;
            else
                return false;
        }

        void notify_should_close() {
            glfwSetWindowShouldClose(this->window, true);
        }

    private:
        static void callback_fbuf_size(GLFWwindow* window, int width, int height) {
            auto ptr = glfwGetWindowUserPointer(window);
            if (nullptr == ptr)
                return;
            auto engine = reinterpret_cast<mirinae::IEngine*>(ptr);
            engine->notify_window_resize(width, height);
        }

        static void callback_key(GLFWwindow* window, int key, int scancode, int action, int mods) {
            auto ptr = glfwGetWindowUserPointer(window);
            if (nullptr == ptr)
                return;
            auto engine = reinterpret_cast<mirinae::IEngine*>(ptr);

            mirinae::key::Event e;
            switch (action) {
                case GLFW_RELEASE:
                    e.action_type = mirinae::key::ActionType::up;
                    break;
                case GLFW_PRESS:
                case GLFW_REPEAT:
                default:
                    e.action_type = mirinae::key::ActionType::down;
                    break;
            }

            e.key = map_key_code(key);
            engine->notify_key_event(e);
        }

        static mirinae::key::KeyCode map_key_code(const int glfw_key) {
            if (GLFW_KEY_A <= glfw_key && glfw_key <= GLFW_KEY_Z) {
                auto index = glfw_key - GLFW_KEY_A + int(mirinae::key::KeyCode::a);
                return mirinae::key::KeyCode(index);
            }
            else if (GLFW_KEY_0 <= glfw_key && glfw_key <= GLFW_KEY_9) {
                auto index = glfw_key - GLFW_KEY_0 + int(mirinae::key::KeyCode::n0);
                return mirinae::key::KeyCode(index);
            }
            else {
                static const std::unordered_map<uint32_t, mirinae::key::KeyCode> map{
                    {GLFW_KEY_GRAVE_ACCENT, mirinae::key::KeyCode::backquote},
                    {GLFW_KEY_MINUS, mirinae::key::KeyCode::minus},
                    {GLFW_KEY_EQUAL, mirinae::key::KeyCode::equal},
                    {GLFW_KEY_LEFT_BRACKET, mirinae::key::KeyCode::lbracket},
                    {GLFW_KEY_RIGHT_BRACKET, mirinae::key::KeyCode::rbracket},
                    {GLFW_KEY_BACKSLASH, mirinae::key::KeyCode::backslash},
                    {GLFW_KEY_SEMICOLON, mirinae::key::KeyCode::semicolon},
                    {GLFW_KEY_APOSTROPHE, mirinae::key::KeyCode::quote},
                    {GLFW_KEY_COMMA, mirinae::key::KeyCode::comma},
                    {GLFW_KEY_PERIOD, mirinae::key::KeyCode::period},
                    {GLFW_KEY_SLASH, mirinae::key::KeyCode::slash},

                    {GLFW_KEY_SPACE, mirinae::key::KeyCode::space},
                    {GLFW_KEY_ENTER, mirinae::key::KeyCode::enter},
                    {GLFW_KEY_BACKSPACE, mirinae::key::KeyCode::backspace},
                    {GLFW_KEY_TAB, mirinae::key::KeyCode::tab},

                    {GLFW_KEY_ESCAPE, mirinae::key::KeyCode::escape},
                    {GLFW_KEY_LEFT_SHIFT, mirinae::key::KeyCode::lshfit},
                    {GLFW_KEY_RIGHT_SHIFT, mirinae::key::KeyCode::rshfit},
                    {GLFW_KEY_LEFT_CONTROL, mirinae::key::KeyCode::lctrl},
                    {GLFW_KEY_RIGHT_CONTROL, mirinae::key::KeyCode::rctrl},
                    {GLFW_KEY_LEFT_ALT, mirinae::key::KeyCode::lalt},
                    {GLFW_KEY_RIGHT_ALT, mirinae::key::KeyCode::ralt},
                    {GLFW_KEY_UP, mirinae::key::KeyCode::up},
                    {GLFW_KEY_DOWN, mirinae::key::KeyCode::down},
                    {GLFW_KEY_LEFT, mirinae::key::KeyCode::left},
                    {GLFW_KEY_RIGHT, mirinae::key::KeyCode::right},
                };

                auto res = map.find(glfw_key);
                if (res == map.end()) {
                    return mirinae::key::KeyCode::eoe;
                }
                else {
                    return res->second;
                }
            }
        }

        GLFWwindow* window = nullptr;

    };


    using FrameIndex = mirinae::StrongType<int, struct FrameIndexStrongTypeTag>;


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
        EngineGlfw()
            : window_(this)
        {
            mirinae::InstanceFactory instance_factory;
            instance_factory.enable_validation_layer();
            instance_factory.ext_layers_.add_validation();
            {
                const auto glfwExtensions = ::get_glfw_extensions();
                instance_factory.ext_layers_.extensions_.insert(instance_factory.ext_layers_.extensions_.end(), glfwExtensions.begin(), glfwExtensions.end());
            }

            instance_.init(instance_factory);
            surface_ = window_.create_surface(instance_);
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

            this->create_swapchain_and_relatives();

            cmd_pool_.init(phys_device_.graphics_family_index().value(), logi_device_);
            for (int i = 0; i < framesync_.MAX_FRAMES_IN_FLIGHT; ++i)
                cmd_buf_.push_back(cmd_pool_.alloc(logi_device_));

            desc_pool_.init(framesync_.MAX_FRAMES_IN_FLIGHT, logi_device_);
            desc_sets_ = desc_pool_.alloc(framesync_.MAX_FRAMES_IN_FLIGHT, desclayout_, logi_device_);

            // Vertices
            {
                constexpr float v = 0.5;
                std::vector<mirinae::VertexStatic> vertices;
                vertices.push_back(mirinae::VertexStatic{ glm::vec3{ -v, -v, 0}, glm::vec3{1, 1, 0} });
                vertices.push_back(mirinae::VertexStatic{ glm::vec3{ -v,  v, 0}, glm::vec3{0, 0, 1} });
                vertices.push_back(mirinae::VertexStatic{ glm::vec3{  v,  v, 0}, glm::vec3{0, 0, 1} });
                vertices.push_back(mirinae::VertexStatic{ glm::vec3{  v, -v, 0}, glm::vec3{1, 1, 1} });
                const auto data_size = sizeof(mirinae::VertexStatic) * vertices.size();

                mirinae::Buffer staging_buffer;
                staging_buffer.init(
                    data_size,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    phys_device_,
                    logi_device_
                );
                staging_buffer.set_data(vertices.data(), data_size, logi_device_);

                vertex_buf_.init(data_size,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    phys_device_,
                    logi_device_
                );

                auto cmdbuf = cmd_pool_.alloc(logi_device_);
                vertex_buf_.record_copy_cmd(staging_buffer, cmdbuf, logi_device_);
                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &cmdbuf;
                vkQueueSubmit(logi_device_.graphics_queue(), 1, &submitInfo, VK_NULL_HANDLE);
                vkQueueWaitIdle(logi_device_.graphics_queue());
                cmd_pool_.free(cmdbuf, logi_device_);
                staging_buffer.destroy(logi_device_);
            }

            // Indices
            {
                std::vector<uint16_t> indices{
                    0, 1, 2, 0, 2, 3
                };
                const auto data_size = sizeof(uint16_t) * indices.size();

                mirinae::Buffer staging_buffer;
                staging_buffer.init(
                    data_size,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    phys_device_,
                    logi_device_
                );
                staging_buffer.set_data(indices.data(), data_size, logi_device_);

                index_buf_.init(data_size,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    phys_device_,
                    logi_device_
                );

                auto cmdbuf = cmd_pool_.alloc(logi_device_);
                index_buf_.record_copy_cmd(staging_buffer, cmdbuf, logi_device_);
                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &cmdbuf;
                vkQueueSubmit(logi_device_.graphics_queue(), 1, &submitInfo, VK_NULL_HANDLE);
                vkQueueWaitIdle(logi_device_.graphics_queue());
                cmd_pool_.free(cmdbuf, logi_device_);
                staging_buffer.destroy(logi_device_);
            }

            // Texture
            {
                const auto path = mirinae::find_resources_folder().value() / "textures" / "grass1.tga";
                auto image = mirinae::load_image(path.u8string().c_str());

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

                VkWriteDescriptorSet descriptorWrite{};
                descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrite.dstSet = desc_sets_.at(i);
                descriptorWrite.dstBinding = 0;
                descriptorWrite.dstArrayElement = 0;
                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pBufferInfo = &bufferInfo;
                descriptorWrite.pImageInfo = nullptr;
                descriptorWrite.pTexelBufferView = nullptr;

                vkUpdateDescriptorSets(logi_device_.get(), 1, &descriptorWrite, 0, nullptr);
            }
        }

        ~EngineGlfw() {
            this->logi_device_.wait_idle();

            for (auto& ubuf : uniform_buf_) ubuf.destroy(logi_device_); uniform_buf_.clear();
            texture_.destroy(logi_device_);
            index_buf_.destroy(logi_device_);
            vertex_buf_.destroy(logi_device_);
            desc_pool_.destroy(logi_device_);
            cmd_pool_.destroy(logi_device_);
            this->destroy_swapchain_and_relatives();
            logi_device_.destroy();
            vkDestroySurfaceKHR(instance_.get(), surface_, nullptr); surface_ = nullptr;
        }

        void do_frame() override {
            const auto image_index_opt = this->try_acquire_image();
            if (!image_index_opt) {
                glfwPollEvents();
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
                ubuf_data.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                ubuf_data.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
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

                VkClearValue clearColor = { {{0, 0, 0, 1}} };

                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = renderpass_.get();
                renderPassInfo.framebuffer = swapchain_fbufs_[image_index.get()].get();
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = swapchain_.extent();
                renderPassInfo.clearValueCount = 1;
                renderPassInfo.pClearValues = &clearColor;

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

                VkBuffer vertexBuffers[] = { vertex_buf_.buffer() };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cur_cmd_buf, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(cur_cmd_buf, index_buf_.buffer(), 0, VK_INDEX_TYPE_UINT16);
                vkCmdDrawIndexed(cur_cmd_buf, index_buf_.size() / sizeof(uint16_t), 1, 0, 0, 0);

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
                presentInfo.swapchainCount = swapchains.size();
                presentInfo.pSwapchains = swapchains.data();
                presentInfo.pImageIndices = swapchain_indices.data();
                presentInfo.pResults = nullptr;

                vkQueuePresentKHR(logi_device_.present_queue(), &presentInfo);
            }

            framesync_.increase_frame_index();
            window_.swap_buffer();
            glfwPollEvents();
        }

        bool is_ongoing() override {
            return window_.is_ongoing();
        }

        void notify_window_resize(unsigned width, unsigned height) {
            fbuf_resized_ = true;
            spdlog::info("Window resized: {} x {}", width, height);
        }

        void notify_key_event(const mirinae::key::Event& e) {

        }

    private:
        void create_swapchain_and_relatives() {
            this->logi_device_.wait_idle();

            const auto [fbuf_width, fbuf_height] = window_.get_fbuf_size();
            swapchain_.init(fbuf_width, fbuf_height, surface_, phys_device_, logi_device_);

            framesync_.init(logi_device_);
            renderpass_.init(swapchain_.format(), logi_device_);
            desclayout_.init(logi_device_);
            pipeline_ = mirinae::create_unorthodox_pipeline(swapchain_.extent(), renderpass_, desclayout_, logi_device_);

            swapchain_fbufs_.resize(swapchain_.views_count());
            for (size_t i = 0; i < swapchain_fbufs_.size(); ++i) {
                swapchain_fbufs_[i].init(swapchain_.extent(), swapchain_.view_at(i), renderpass_, logi_device_);
            }
        }

        void destroy_swapchain_and_relatives() {
            this->logi_device_.wait_idle();

            for (auto& x : swapchain_fbufs_) x.destroy(logi_device_); swapchain_fbufs_.clear();
            pipeline_.destroy(logi_device_);
            desclayout_.destroy(logi_device_);
            renderpass_.destroy(logi_device_);
            framesync_.destroy(logi_device_);
            swapchain_.destroy(logi_device_);
        }

        std::optional<mirinae::ShainImageIndex> try_acquire_image() {
            framesync_.get_cur_in_flight_fence().wait(logi_device_);

            if (fbuf_resized_) {
                if (this->window_.is_fbuf_too_small()) {
                    fbuf_resized_ = true;
                }
                else {
                    fbuf_resized_ = false;
                    this->destroy_swapchain_and_relatives();
                    this->create_swapchain_and_relatives();
                }
                return std::nullopt;
            }

            const auto image_index_opt = swapchain_.acquire_next_image(framesync_.get_cur_img_ava_semaph(), logi_device_);
            if (!image_index_opt) {
                if (this->window_.is_fbuf_too_small()) {
                    fbuf_resized_ = true;
                }
                else {
                    fbuf_resized_ = false;
                    this->destroy_swapchain_and_relatives();
                    this->create_swapchain_and_relatives();
                }
                return std::nullopt;
            }

            framesync_.get_cur_in_flight_fence().reset(logi_device_);
            return image_index_opt.value();
        }

        GlfwWindow window_;
        mirinae::VulkanInstance instance_;
        VkSurfaceKHR surface_ = nullptr;
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
        mirinae::Buffer vertex_buf_;
        mirinae::Buffer index_buf_;
        mirinae::TextureImage texture_;
        std::vector<mirinae::Buffer> uniform_buf_;
        bool fbuf_resized_ = false;

    };

}


namespace mirinae {

    std::unique_ptr<IEngine> create_engine() {
        return std::make_unique<EngineGlfw>();
    }

}
