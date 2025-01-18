#include "mirinae/render/renderpass/common.hpp"

#include <set>

#include "mirinae/lightweight/include_spdlog.hpp"


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
