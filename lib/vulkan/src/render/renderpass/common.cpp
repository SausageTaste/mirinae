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


    RpResources::RpResources(VulkanDevice& device) : device_(device) {}

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
        SPDLOG_INFO("RpImage '{}' is no longer used by '{}'", id, user_id);

        if (r.writers_.empty() && r.readers_.empty()) {
            r.destroy(device_);
            imgs_.erase(it);
            SPDLOG_INFO("RpImage '{}' is obsolete", id);
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

        SPDLOG_INFO("RpImage created: {}", id);
        return r.data_;
    }

    HRpImage RpResources::get_img_reader(const str& id, const str& user_id) {
        auto it = imgs_.find(id);
        if (it == imgs_.end())
            return nullptr;

        auto& r = it->second;
        r.readers_.insert(user_id);

        SPDLOG_INFO("RpImage reader: {} <- {}", id, user_id);
        return r.data_;
    }

}  // namespace mirinae


// ViewFrustum
namespace mirinae {

    void ViewFrustum::update(const glm::dmat4& proj, const glm::dmat4& view) {
        constexpr float VALUE = 0.8;

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
        vertices_.resize(v.size());
        for (size_t i = 0; i < v.size(); ++i) {
            auto ndc = proj_inv * glm::vec4(v[i], 1);
            auto ndc3 = glm::vec3(ndc) / ndc.w;
            vertices_[i] = ndc3;
        }

        // Since 'Separating Axis Theorem' is used, the direction of normal
        // doesn't really matter
        axes_.resize(6);
        axes_[0] = ::make_normal(v[0], v[2], v[3]);  // Fixed +x
        axes_[1] = ::make_normal(v[1], v[4], v[5]);  // Fixed -x
        axes_[2] = ::make_normal(v[0], v[1], v[3]);  // Fixed +y
        axes_[3] = ::make_normal(v[2], v[4], v[6]);  // Fixed -y
        axes_[4] = ::make_normal(v[0], v[1], v[2]);  // Fixed +z
        axes_[5] = ::make_normal(v[3], v[5], v[6]);  // Fixed -z

        return;
    }

}  // namespace mirinae
