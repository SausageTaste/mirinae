#include "mirinae/render/renderpass/common.hpp"

#include <set>

#include "mirinae/lightweight/include_spdlog.hpp"


namespace {

    glm::vec3 make_normal(
        const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2
    ) {
        return glm::normalize(glm::cross(p1 - p0, p2 - p0));
    }

}  // namespace


// RenderPass
namespace mirinae {

    RenderPass::RenderPass() {}

    RenderPass::~RenderPass() {
        if (VK_NULL_HANDLE != rp_)
            MIRINAE_ABORT("RenderPass is not destroyed");
    }

    RenderPass& RenderPass::operator=(VkRenderPass rp) {
        if (VK_NULL_HANDLE != rp_)
            MIRINAE_ABORT("RenderPass is not destroyed");

        rp_ = rp;
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

    RpPipeline::RpPipeline() {}

    RpPipeline::~RpPipeline() {
        if (VK_NULL_HANDLE != handle_)
            MIRINAE_ABORT("Pipeline is not destroyed");
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
            MIRINAE_ABORT("PipelineLayout is not destroyed");
    }

    RpPipeLayout& RpPipeLayout::operator=(VkPipelineLayout handle) {
        if (VK_NULL_HANDLE != handle_)
            MIRINAE_ABORT("PipelineLayout is not destroyed");

        handle_ = handle;
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


// RpResources
namespace mirinae {

    class RpResources::ImageRecord {

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


    RpResources::RpResources(sung::HTaskSche task_sche, VulkanDevice& device)
        : tex_man_(create_tex_mgr(task_sche, device)), device_(device) {}

    RpResources::~RpResources() {
        for (auto& [id, img] : imgs_) {
            img.destroy(device_);
        }
    }

    void RpResources::free_img(const str& id, const str& user_id) {
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

    HRpImage RpResources::new_img(const str& name, const str& user_id) {
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

    HRpImage RpResources::get_img_reader(const str& id, const str& user_id) {
        auto it = imgs_.find(id);
        if (it == imgs_.end())
            return nullptr;

        auto& r = it->second;
        r.readers_.insert(user_id);

        // SPDLOG_INFO("RpImage reader: {} <- {}", id, user_id);
        return r.data_;
    }

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
