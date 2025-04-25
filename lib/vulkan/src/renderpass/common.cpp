#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/common.hpp"

#include <set>

#include "mirinae/lightweight/task.hpp"


namespace {

    glm::vec3 make_normal(
        const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2
    ) {
        return glm::normalize(glm::cross(p1 - p0, p2 - p0));
    }

}  // namespace


// RenderPass
namespace mirinae {

    RenderPass::RenderPass() = default;

    RenderPass::~RenderPass() {
        if (VK_NULL_HANDLE != rp_)
            SPDLOG_ERROR("RenderPass is not destroyed");
    }

    RenderPass& RenderPass::operator=(VkRenderPass rp) {
        if (VK_NULL_HANDLE != rp_)
            MIRINAE_ABORT("RenderPass is not destroyed");

        rp_ = rp;
        return *this;
    }

    RenderPass::operator VkRenderPass() const { return rp_; }

    VkRenderPass RenderPass::operator*() const { return rp_; }

    VkRenderPass RenderPass::get() const { return rp_; }

    void RenderPass::reset(VkRenderPass rp, mirinae::VulkanDevice& device) {
        this->destroy(device);
        rp_ = rp;
    }

    void RenderPass::destroy(mirinae::VulkanDevice& device) {
        if (rp_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device.logi_device(), rp_, nullptr);
            rp_ = VK_NULL_HANDLE;
        }
    }

}  // namespace mirinae


// RpPipeline
namespace mirinae {

    RpPipeline::RpPipeline() = default;

    RpPipeline::~RpPipeline() {
        if (VK_NULL_HANDLE != handle_)
            SPDLOG_ERROR("Pipeline is not destroyed");
    }

    bool RpPipeline::create(
        const VkComputePipelineCreateInfo& cinfo, VulkanDevice& device
    ) {
        return vkCreateComputePipelines(
                   device.logi_device(),
                   VK_NULL_HANDLE,
                   1,
                   &cinfo,
                   nullptr,
                   &handle_
               ) == VK_SUCCESS;
    }

    RpPipeline& RpPipeline::operator=(VkPipeline handle) {
        if (VK_NULL_HANDLE != handle_)
            MIRINAE_ABORT("Pipeline is not destroyed");

        handle_ = handle;
        return *this;
    }

    RpPipeline::operator VkPipeline() const { return handle_; }

    VkPipeline RpPipeline::operator*() const { return handle_; }

    VkPipeline RpPipeline::get() const { return handle_; }

    void RpPipeline::reset(VkPipeline handle, VulkanDevice& device) {
        this->destroy(device);
        handle_ = handle;
    }

    void RpPipeline::destroy(VulkanDevice& device) {
        if (handle_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device.logi_device(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}  // namespace mirinae


// RpPipeLayout
namespace mirinae {

    RpPipeLayout::RpPipeLayout() {}

    RpPipeLayout::~RpPipeLayout() {
        if (VK_NULL_HANDLE != handle_)
            SPDLOG_ERROR("PipelineLayout is not destroyed");
    }

    RpPipeLayout& RpPipeLayout::operator=(VkPipelineLayout handle) {
        if (VK_NULL_HANDLE != handle_)
            MIRINAE_ABORT("PipelineLayout is not destroyed");

        handle_ = handle;
        return *this;
    }

    RpPipeLayout::operator VkPipelineLayout() const { return handle_; }

    RpPipeLayout::operator VkPipelineLayout&() { return handle_; }

    VkPipelineLayout RpPipeLayout::operator*() const { return handle_; }

    VkPipelineLayout RpPipeLayout::get() const { return handle_; }

    void RpPipeLayout::reset(VkPipelineLayout handle, VulkanDevice& device) {
        this->destroy(device);
        handle_ = handle;
    }

    void RpPipeLayout::destroy(VulkanDevice& device) {
        if (handle_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device.logi_device(), handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

}  // namespace mirinae


// FbufImageBundle
namespace mirinae {

    void FbufImageBundle::init(
        uint32_t max_frames_in_flight,
        uint32_t width,
        uint32_t height,
        mirinae::ITextureManager& tex_man,
        mirinae::VulkanDevice& device
    ) {
        this->destroy();

        for (uint32_t i = 0; i < max_frames_in_flight; ++i) {
            depth_.push_back(create_tex_depth(width, height, device));
            albedo_.push_back(create_tex_attach(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "albedo",
                device
            ));
            normal_.push_back(create_tex_attach(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "normal",
                device
            ));
            material_.push_back(create_tex_attach(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "material",
                device
            ));
            compo_.push_back(create_tex_attach(
                width,
                height,
                device.img_formats().rgb_hdr(),
                mirinae::FbufUsage::color_attachment,
                "compo",
                device
            ));
        }
    }

    void FbufImageBundle::destroy() {
        depth_.clear();
        albedo_.clear();
        normal_.clear();
        material_.clear();
        compo_.clear();
    }

    uint32_t FbufImageBundle::width() const { return depth_.front()->width(); }

    uint32_t FbufImageBundle::height() const {
        return depth_.front()->height();
    }

    VkExtent2D FbufImageBundle::extent() const {
        return { this->width(), this->height() };
    }

    mirinae::ITexture& FbufImageBundle::depth(uint32_t f_index) {
        return *depth_.at(f_index);
    }

    mirinae::ITexture& FbufImageBundle::albedo(uint32_t f_index) {
        return *albedo_.at(f_index);
    }

    mirinae::ITexture& FbufImageBundle::normal(uint32_t f_index) {
        return *normal_.at(f_index);
    }

    mirinae::ITexture& FbufImageBundle::material(uint32_t f_index) {
        return *material_.at(f_index);
    }

    mirinae::ITexture& FbufImageBundle::compo(uint32_t f_index) {
        return *compo_.at(f_index);
    }

}  // namespace mirinae


// Command buffer for render passes
namespace {

    class ResettingCommandPool {

    public:
        ~ResettingCommandPool() { MIRINAE_ASSERT(VK_NULL_HANDLE == handle_); }

        void init(mirinae::VulkanDevice& device) {
            if (handle_ != VK_NULL_HANDLE) {
                SPDLOG_WARN("Command pool is already initialized!");
                return;
            }

            VkCommandPoolCreateInfo cinfo{};
            cinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            cinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            cinfo.queueFamilyIndex = *device.graphics_queue_family_index();

            const auto res = vkCreateCommandPool(
                device.logi_device(), &cinfo, nullptr, &handle_
            );
            MIRINAE_ASSERT(res == VK_SUCCESS);
        }

        void destroy(mirinae::VulkanDevice& device) {
            if (VK_NULL_HANDLE != handle_) {
                vkDestroyCommandPool(device.logi_device(), handle_, nullptr);
                handle_ = VK_NULL_HANDLE;
            } else {
                SPDLOG_WARN("Command pool is already destroyed!");
            }
        }

        void reset(mirinae::VulkanDevice& device) {
            if (VK_NULL_HANDLE == handle_) {
                SPDLOG_ERROR("Command pool is not initialized!");
                return;
            }

            const auto res = vkResetCommandPool(
                device.logi_device(), handle_, 0
            );
            if (res != VK_SUCCESS) {
                SPDLOG_ERROR("Failed to reset command pool!");
            }
        }

        VkCommandBuffer alloc(mirinae::VulkanDevice& device) {
            VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
            if (this->alloc(&cmdbuf, 1, device))
                return cmdbuf;

            return VK_NULL_HANDLE;
        }

        bool alloc(
            VkCommandBuffer* dst,
            const uint32_t count,
            VkCommandBufferLevel level,
            mirinae::VulkanDevice& device
        ) {
            if (VK_NULL_HANDLE == handle_) {
                SPDLOG_ERROR("Command pool is not initialized!");
                return false;
            }

            VkCommandBufferAllocateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            info.commandPool = handle_;
            info.level = level;
            info.commandBufferCount = count;

            return VK_SUCCESS ==
                   vkAllocateCommandBuffers(device.logi_device(), &info, dst);
        }

        bool alloc(
            VkCommandBuffer* dst,
            const uint32_t count,
            mirinae::VulkanDevice& device
        ) {
            return this->alloc(
                dst, count, VK_COMMAND_BUFFER_LEVEL_PRIMARY, device
            );
        }

    private:
        VkCommandPool handle_ = VK_NULL_HANDLE;
    };

}  // namespace
namespace mirinae {

    class RpCommandPool::Impl {

    public:
        void init(VulkanDevice& device) {
            thread_data_.resize(dal::tasker().GetNumTaskThreads());
            for (auto& td : thread_data_) {
                td.init(device);
            }
        }

        void destroy(VulkanDevice& device) {
            for (auto& td : thread_data_) {
                td.destroy(device);
            }
            thread_data_.clear();
        }

        // Make sure it's called before enki task starts
        void reset_pool(FrameIndex f_index, VulkanDevice& device) {
            for (auto& td : thread_data_) {
                td.get_frame_data(f_index).reset_pool(device);
            }
        }

        VkCommandBuffer get(
            FrameIndex f_index, uint32_t threadnum, VulkanDevice& device
        ) {
            auto& fd = this->get_fd(f_index, threadnum);
            return fd.get(f_index, threadnum, device);
        }

    private:
        class FrameData {

        public:
            void init(VulkanDevice& device) {
                pool_.init(device);
                cmd_bufs_.clear();
                cursor_ = 0;
            }

            void destroy(VulkanDevice& device) {
                pool_.destroy(device);
                cmd_bufs_.clear();
                cursor_ = 0;
            }

            void reset_pool(VulkanDevice& device) {
                pool_.reset(device);
                cursor_ = 0;
            }

            VkCommandBuffer get(
                FrameIndex f_index, uint32_t threadnum, VulkanDevice& device
            ) {
                if (cursor_ == cmd_bufs_.size()) {
                    const size_t GROW = 4;
                    const auto old_size = cmd_bufs_.size();

                    cmd_bufs_.resize(old_size + GROW);
                    if (!pool_.alloc(&cmd_bufs_[old_size], GROW, device)) {
                        cmd_bufs_.resize(old_size);
                        SPDLOG_ERROR("Failed to allocate command buffer!");
                        return VK_NULL_HANDLE;
                    }

                    SPDLOG_DEBUG(
                        "RP cmd pool growing: count={}, frame={}, thread={}",
                        cmd_bufs_.size(),
                        f_index.get(),
                        threadnum
                    );
                } else if (cursor_ > cmd_bufs_.size()) {
                    // Since cursor_ is incremented by 1 in every get() call,
                    // this should never happen.
                    SPDLOG_ERROR(
                        "This shouldn't happen (cursor: {}, cmd_bufs size: {})",
                        cursor_,
                        cmd_bufs_.size()
                    );
                    return VK_NULL_HANDLE;
                }

                return cmd_bufs_[cursor_++];
            }

        private:
            ResettingCommandPool pool_;
            std::vector<VkCommandBuffer> cmd_bufs_;
            size_t cursor_ = 0;
        };

        class ThreadData {

        public:
            void init(VulkanDevice& device) {
                frame_data_.resize(mirinae::MAX_FRAMES_IN_FLIGHT);
                for (auto& fd : frame_data_) {
                    fd.init(device);
                }
            }

            void destroy(VulkanDevice& device) {
                for (auto& fd : frame_data_) {
                    fd.destroy(device);
                }
                frame_data_.clear();
            }

            FrameData& get_frame_data(FrameIndex f_index) {
                MIRINAE_ASSERT(f_index.get() < frame_data_.size());
                return frame_data_[f_index.get()];
            }

        private:
            std::vector<FrameData> frame_data_;
        };

        FrameData& get_fd(FrameIndex f_index, uint32_t threadnum) {
            MIRINAE_ASSERT(threadnum < thread_data_.size());
            return thread_data_[threadnum].get_frame_data(f_index);
        }

        std::vector<ThreadData> thread_data_;
    };


    RpCommandPool::RpCommandPool() { pimpl_ = std::make_unique<Impl>(); }

    RpCommandPool::~RpCommandPool() {}

    void RpCommandPool::init(VulkanDevice& device) { pimpl_->init(device); }

    void RpCommandPool::destroy(VulkanDevice& device) {
        pimpl_->destroy(device);
    }

    void RpCommandPool::reset_pool(FrameIndex f_index, VulkanDevice& device) {
        pimpl_->reset_pool(f_index, device);
    }

    VkCommandBuffer RpCommandPool::get(
        FrameIndex f_index, uint32_t threadnum, VulkanDevice& device
    ) {
        return pimpl_->get(f_index, threadnum, device);
    }

}  // namespace mirinae


// RenderTargetManager
namespace mirinae {

    class RenderTargetManager::ImageRecord {

    public:
        void destroy(VulkanDevice& device) {
            data_->img_.destroy(device.mem_alloc());
            data_->view_.destroy(device);
            data_.reset();
        }

        std::set<str> writers_;
        std::set<str> readers_;
        str creater_;
        HImage data_;
    };


    RenderTargetManager::RenderTargetManager(VulkanDevice& device)
        : device_(device) {}

    RenderTargetManager::~RenderTargetManager() {
        for (auto& [id, img] : imgs_) {
            img.destroy(device_);
        }
    }

    void RenderTargetManager::free_img(const str& id, const str& user_id) {
        auto it = imgs_.find(id);
        if (it == imgs_.end())
            return;

        auto& r = it->second;
        r.writers_.erase(user_id);
        r.readers_.erase(user_id);
        // SPDLOG_INFO("RpImage '{}' is no longer used by '{}'", id, user_id);

        if (r.writers_.empty() && r.readers_.empty()) {
            r.destroy(device_);
            imgs_.erase(it);
            // SPDLOG_INFO("RpImage '{}' is obsolete", id);
        }
    }

    HRpImage RenderTargetManager::new_img(const str& name, const str& user_id) {
        const auto id = user_id + ":" + name;

        auto it = imgs_.find(id);
        if (it != imgs_.end()) {
            MIRINAE_ABORT(
                "Image creation requested by '{}' failed because the image has "
                "already been created by '{}'",
                user_id,
                it->second.creater_
            );
        }

        auto& r = imgs_[id];
        r.creater_ = user_id;
        r.writers_.insert(user_id);
        r.data_ = std::make_shared<Image>(id);

        // SPDLOG_INFO("RpImage created: {}", id);
        return r.data_;
    }

    HRpImage RenderTargetManager::get_img_reader(
        const str& id, const str& user_id
    ) {
        auto it = imgs_.find(id);
        if (it == imgs_.end())
            return nullptr;

        auto& r = it->second;
        r.readers_.insert(user_id);

        // SPDLOG_INFO("RpImage reader: {} <- {}", id, user_id);
        return r.data_;
    }

}  // namespace mirinae


// RpResources
namespace mirinae {

    RpResources::RpResources(sung::HTaskSche task_sche, VulkanDevice& device)
        : desclays_(device)
        , tex_man_(create_tex_mgr(task_sche, device))
        , ren_img_(device)
        , device_(device) {
        cmd_pool_.init(device);
    }

    RpResources::~RpResources() { cmd_pool_.destroy(device_); }

}  // namespace mirinae


// ViewFrustum
namespace mirinae {

    void ViewFrustum::update(const glm::dmat4& proj, const glm::dmat4& view) {
        constexpr float VALUE = 1;

        const static std::vector<glm::vec3> v{
            glm::vec3(VALUE, VALUE, VALUE),    // 0
            glm::vec3(-VALUE, VALUE, VALUE),   // 1
            glm::vec3(VALUE, -VALUE, VALUE),   // 2
            glm::vec3(VALUE, VALUE, 0),        // 3
            glm::vec3(-VALUE, -VALUE, VALUE),  // 4
            glm::vec3(-VALUE, VALUE, 0),       // 5
            glm::vec3(VALUE, -VALUE, 0),       // 6
            glm::vec3(-VALUE, -VALUE, 0),      // 7
        };

        view_inv_ = glm::inverse(view);

        const auto proj_inv = glm::mat4(glm::inverse(proj));
        for (size_t i = 0; i < v.size(); ++i) {
            auto ndc = proj_inv * glm::vec4(v[i], 1);
            auto ndc3 = glm::vec3(ndc) / ndc.w;
            vtx_[i] = ndc3;
        }

        // Since 'Separating Axis Theorem' is used, the direction of normal
        // doesn't really matter
        axes_[0] = ::make_normal(vtx_[0], vtx_[2], vtx_[3]);  // Fixed +x
        axes_[1] = ::make_normal(vtx_[1], vtx_[4], vtx_[5]);  // Fixed -x
        axes_[2] = ::make_normal(vtx_[0], vtx_[1], vtx_[3]);  // Fixed +y
        axes_[3] = ::make_normal(vtx_[2], vtx_[4], vtx_[6]);  // Fixed -y
        axes_[4] = ::make_normal(vtx_[0], vtx_[1], vtx_[2]);  // Fixed +z
        axes_[5] = ::make_normal(vtx_[3], vtx_[5], vtx_[6]);  // Fixed -z

        return;
    }

}  // namespace mirinae
