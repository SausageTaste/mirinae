#include "mirinae/engine.hpp"

#include <vulkan/vulkan.h>
#include <spdlog/spdlog.h>

#include <daltools/util.h>

#include <mirinae/actor/transform.hpp>
#include <mirinae/render/mem_alloc.hpp>
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


    class TextureManager {

    public:
        struct TextureData {
            mirinae::Image texture_;
            mirinae::ImageView texture_view_;
            std::string id_;
        };

        std::shared_ptr<TextureData> request(
            const std::string& path,
            mirinae::IFilesys& filesys,
            mirinae::VulkanMemoryAllocator mem_alloc,
            mirinae::CommandPool& cmd_pool,
            mirinae::LogiDevice& logi_device
        ) {
            if (auto index = this->find_index(path))
                return textures_.at(index.value());

            const auto img_data = filesys.read_file_to_vector(path.c_str());
            const auto image = mirinae::parse_image(img_data->data(), img_data->size());
            auto& output = textures_.emplace_back(new TextureData);
            output->id_ = path;
            this->create(*image, *output, mem_alloc, cmd_pool, logi_device);
            return output;
        }

        void claer(mirinae::VulkanMemoryAllocator mem_alloc, mirinae::LogiDevice& logi_device) {
            for (auto& tex : textures_) {
                if (tex.use_count() > 1)
                    spdlog::warn("Want to destroy texture '{}' is still in use", tex->id_);

                tex->texture_view_.destroy(logi_device);
                tex->texture_.destroy(mem_alloc);
            }
            textures_.clear();
        }

    private:
        std::optional<size_t> find_index(const std::string& id) {
            for (size_t i = 0; i < textures_.size(); ++i) {
                if (textures_.at(i)->id_ == id)
                    return i;
            }
            return std::nullopt;
        }

        static void create(
            const mirinae::IImage2D& image,
            TextureData& output,
            mirinae::VulkanMemoryAllocator mem_alloc,
            mirinae::CommandPool& cmd_pool,
            mirinae::LogiDevice& logi_device
        ) {
            mirinae::Buffer staging_buffer;
            staging_buffer.init_staging(image.data_size(), mem_alloc);
            staging_buffer.set_data(image.data(), image.data_size(), mem_alloc);

            output.texture_.init_rgba8_srgb(image.width(), image.height(), mem_alloc);
            mirinae::copy_to_img_and_transition(
                output.texture_.image(),
                output.texture_.width(),
                output.texture_.height(),
                output.texture_.format(),
                staging_buffer.buffer(),
                cmd_pool,
                logi_device
            );
            staging_buffer.destroy(mem_alloc);

            output.texture_view_.init(output.texture_.image(), output.texture_.format(), VK_IMAGE_ASPECT_COLOR_BIT, logi_device);
        }

        std::vector<std::shared_ptr<TextureData>> textures_;

    };


    class RenderUnit {

    public:
        void init(
            uint32_t max_flight_count,
            const mirinae::VerticesStaticPair& vertices,
            VkImageView image_view,
            VkSampler texture_sampler,
            mirinae::CommandPool& cmd_pool,
            mirinae::DescriptorSetLayout& layout,
            mirinae::VulkanMemoryAllocator mem_alloc,
            mirinae::LogiDevice& logi_device
        ) {
            desc_pool_.init(max_flight_count, logi_device);
            desc_sets_ = desc_pool_.alloc(max_flight_count, layout, logi_device);

            for (uint32_t i = 0; i < max_flight_count; ++i) {
                auto& ubuf = uniform_buf_.emplace_back();
                ubuf.init_ubuf(sizeof(mirinae::U_Unorthodox), mem_alloc);
            }

            for (size_t i = 0; i < max_flight_count; i++) {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = uniform_buf_.at(i).buffer();
                bufferInfo.offset = 0;
                bufferInfo.range = uniform_buf_.at(i).size();

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = image_view;
                imageInfo.sampler = texture_sampler;

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

                vkUpdateDescriptorSets(logi_device.get(), static_cast<uint32_t>(write_info.size()), write_info.data(), 0, nullptr);
            }

            vert_index_pair_.init(vertices, cmd_pool, mem_alloc, logi_device);
        }

        void destroy(mirinae::VulkanMemoryAllocator mem_alloc, mirinae::LogiDevice& logi_device) {
            for (auto& ubuf : uniform_buf_)
                ubuf.destroy(mem_alloc);
            uniform_buf_.clear();

            vert_index_pair_.destroy(mem_alloc);
            desc_pool_.destroy(logi_device);
        }

        void udpate_ubuf(uint32_t index, const glm::mat4& view_mat, const glm::mat4& proj_mat, mirinae::VulkanMemoryAllocator mem_alloc) {
            auto& ubuf = uniform_buf_.at(index);
            ubuf_data_.model = transform_.make_model_mat();
            ubuf_data_.view = view_mat;
            ubuf_data_.proj = proj_mat;
            ubuf.set_data(&ubuf_data_, sizeof(mirinae::U_Unorthodox), mem_alloc);
        }

        VkDescriptorSet get_desc_set(size_t index) {
            return desc_sets_.at(index);
        }

        void record_bind_vert_buf(VkCommandBuffer cmdbuf) {
            vert_index_pair_.record_bind(cmdbuf);
        }

        auto vertex_count() const {
            return vert_index_pair_.vertex_count();
        }

        mirinae::TransformQuat transform_;

    private:
        mirinae::U_Unorthodox ubuf_data_;
        mirinae::DescriptorPool desc_pool_;
        mirinae::VertexIndexPair vert_index_pair_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<mirinae::Buffer> uniform_buf_;

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
            mem_allocator_ = mirinae::create_vma_allocator(instance_.get(), phys_device_.get(), logi_device_.get());

            mirinae::SwapChainSupportDetails swapchain_details;
            swapchain_details.init(surface_, phys_device_.get());
            if (!swapchain_details.is_complete()) {
                throw std::runtime_error{ "The swapchain is not complete" };
            }

            this->create_swapchain_and_relatives(fbuf_width_, fbuf_height_);

            cmd_pool_.init(phys_device_.graphics_family_index().value(), logi_device_);
            for (int i = 0; i < framesync_.MAX_FRAMES_IN_FLIGHT; ++i)
                cmd_buf_.push_back(cmd_pool_.alloc(logi_device_));

            // Texture
            texture_sampler_.init(phys_device_, logi_device_);

            const std::vector<std::string> texture_paths{
                "textures/grass1.tga",
                "textures/iceland_heightmap.png",
                "textures/lorem_ipsum.png",
                "textures/missing_texture.png",
            };

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

            for (int i = 0; i < 20; ++i) {
                auto texture = tex_man_.request(texture_paths.at(i % texture_paths.size()), *create_info_.filesys_, mem_allocator_, cmd_pool_, logi_device_);
                auto& ren_unit = render_units_.emplace_back();
                ren_unit.init(
                    framesync_.MAX_FRAMES_IN_FLIGHT,
                    vertices,
                    texture->texture_view_.get(),
                    texture_sampler_.get(),
                    cmd_pool_,
                    desclayout_,
                    mem_allocator_,
                    logi_device_
                );

                ren_unit.transform_.pos_ = glm::vec3{ 1.2 * i, 0, 0 };
            }
        }

        ~EngineGlfw() {
            this->logi_device_.wait_idle();

            for (auto& ren_unit : render_units_)
                ren_unit.destroy(mem_allocator_, logi_device_);
            render_units_.clear();

            texture_sampler_.destroy(logi_device_);
            tex_man_.claer(mem_allocator_, logi_device_);
            cmd_pool_.destroy(logi_device_);
            this->destroy_swapchain_and_relatives();
            mirinae::destroy_vma_allocator(mem_allocator_); mem_allocator_ = nullptr;
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

                auto proj_mat = glm::perspective(glm::radians(45.0f), swapchain_.extent().width / (float) swapchain_.extent().height, 0.1f, 100.0f);
                proj_mat[1][1] *= -1;

                for (size_t i = 0; i < render_units_.size(); ++i) {
                    render_units_.at(i).udpate_ubuf(
                        framesync_.get_frame_index().get(), camera_.make_view_mat(), proj_mat, mem_allocator_
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

                depth_image_.init_depth(  swapchain_.extent().width, swapchain_.extent().height, depth_format, mem_allocator_);
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
            depth_image_.destroy(mem_allocator_);
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
        mirinae::VulkanMemoryAllocator mem_allocator_;
        mirinae::Swapchain swapchain_;
        ::FrameSync framesync_;
        ::TextureManager tex_man_;
        mirinae::DescriptorSetLayout desclayout_;
        mirinae::Pipeline pipeline_;
        mirinae::RenderPass renderpass_;
        std::vector<mirinae::Framebuffer> swapchain_fbufs_;
        mirinae::CommandPool cmd_pool_;
        std::vector<VkCommandBuffer> cmd_buf_;
        std::vector<RenderUnit> render_units_;
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
