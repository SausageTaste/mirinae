#include "mirinae/engine.hpp"

#include <spdlog/spdlog.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <mirinae/render/pipeline.hpp>
#include <mirinae/render/vkmajorplayers.hpp>


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

            const auto [fbuf_width, fbuf_height] = window_.get_fbuf_size();
            swapchain_.init(fbuf_width, fbuf_height, surface_, phys_device_, logi_device_);

            framesync_.init(logi_device_);
            renderpass_.init(swapchain_.format(), logi_device_);
            pipeline_ = mirinae::create_unorthodox_pipeline(swapchain_.extent(), renderpass_, logi_device_);

            swapchain_fbufs_.resize(swapchain_.views_count());
            for (size_t i = 0; i < swapchain_fbufs_.size(); ++i) {
                swapchain_fbufs_[i].init(swapchain_.extent(), swapchain_.view_at(i), renderpass_, logi_device_);
            }

            cmd_pool_.init(phys_device_.graphics_family_index().value(), logi_device_);
            for (int i = 0; i < framesync_.MAX_FRAMES_IN_FLIGHT; ++i)
                cmd_buf_.push_back(cmd_pool_.alloc(logi_device_));
        }

        ~EngineGlfw() {
            cmd_pool_.destroy(logi_device_);
            for (auto& x : swapchain_fbufs_) x.destroy(logi_device_); swapchain_fbufs_.clear();
            pipeline_.destroy(logi_device_);
            renderpass_.destroy(logi_device_);
            framesync_.destroy(logi_device_);
            swapchain_.destroy(logi_device_);
            logi_device_.destroy();
            vkDestroySurfaceKHR(instance_.get(), surface_, nullptr); surface_ = nullptr;
        }

        void do_frame() override {
            framesync_.get_cur_in_flight_fence().wait(logi_device_);
            const auto image_index_opt = swapchain_.acquire_next_image(framesync_.get_cur_img_ava_semaph(), logi_device_);
            if (!image_index_opt) {
                spdlog::critical("Swapchain image invalidated");
                throw std::runtime_error{ "Swapchain image invalidated" };
            }
            const auto image_index = image_index_opt.value();

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

                vkCmdDraw(cur_cmd_buf, 3, 1, 0, 0);
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
            spdlog::info("Window resized: {} x {}", width, height);
        }

        void notify_key_event(const mirinae::key::Event& e) {

        }

    private:
        GlfwWindow window_;
        mirinae::VulkanInstance instance_;
        VkSurfaceKHR surface_ = nullptr;
        mirinae::PhysDevice phys_device_;
        mirinae::LogiDevice logi_device_;
        mirinae::Swapchain swapchain_;
        ::FrameSync framesync_;
        mirinae::Pipeline pipeline_;
        mirinae::RenderPass renderpass_;
        std::vector<mirinae::Framebuffer> swapchain_fbufs_;
        mirinae::CommandPool cmd_pool_;
        std::vector<VkCommandBuffer> cmd_buf_;

    };

}


namespace mirinae {

    std::unique_ptr<IEngine> create_engine() {
        return std::make_unique<EngineGlfw>();
    }

}
