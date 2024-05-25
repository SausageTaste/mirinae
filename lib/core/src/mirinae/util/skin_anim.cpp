#include "mirinae/util/skin_anim.hpp"

#include <unordered_map>


namespace {

    float interpolate(float start, float end, float factor) {
        const auto delta = end - start;
        return start + factor * delta;
    }

    glm::vec3 interpolate(
        const glm::vec3& start, const glm::vec3& end, float factor
    ) {
        const auto delta = end - start;
        return start + factor * delta;
    }

    glm::quat interpolate(
        const glm::quat& start, const glm::quat& end, float factor
    ) {
        return glm::slerp(start, end, factor);
    }


    template <typename T>
    size_t find_index_to_start_interp(
        const std::vector<std::pair<float, T>>& container, const float criteria
    ) {
        if (container.empty())
            return 0;

        for (size_t i = 0; i < container.size() - 1; i++) {
            if (criteria < container[i + 1].first)
                return i;
        }

        return container.size() - 1;
    }

    template <typename T>
    T make_interp_value(
        const float tick, const std::vector<std::pair<float, T>>& container
    ) {
        if (container.size() <= 1) {
            return container[0].second;
        }

        const auto start_index = ::find_index_to_start_interp(container, tick);
        const auto next_index = start_index + 1;
        if (next_index >= container.size()) {
            return container.back().second;
        }

        const auto delta_time = container[next_index].first -
                                container[start_index].first;

        auto factor = (tick - container[start_index].first) / delta_time;

        if (0.f <= factor && factor <= 1.f) {
            const auto start = container[start_index].second;
            const auto end = container[next_index].second;
            return ::interpolate(start, end, factor);
        } else {
            const auto start = container[start_index].second;
            const auto end = container[next_index].second;
            return ::interpolate(start, end, 0.f);
        }
    }


    glm::vec3 make_joint_translation(
        double tick, const dal::parser::AnimJoint& anim_joint
    ) {
        if (anim_joint.translations_.empty())
            return glm::vec3{ 0 };
        else
            return ::make_interp_value(tick, anim_joint.translations_);
    }

    glm::quat make_joint_rotation(
        double tick, const dal::parser::AnimJoint& anim_joint
    ) {
        if (anim_joint.rotations_.empty())
            return glm::quat{ 1, 0, 0, 0 };
        else
            return ::make_interp_value(tick, anim_joint.rotations_);
    }

    float make_joint_scale(
        double tick, const dal::parser::AnimJoint& anim_joint
    ) {
        if (anim_joint.scales_.empty())
            return 1;
        else
            return ::make_interp_value(tick, anim_joint.scales_);
    }

    glm::mat4 make_joint_transform(
        double tick, const dal::parser::AnimJoint& anim_joint
    ) {
        const auto translate = ::make_joint_translation(tick, anim_joint);
        const auto rotate = ::make_joint_rotation(tick, anim_joint);
        const auto scale = ::make_joint_scale(tick, anim_joint);

        const glm::mat4 identity{ 1 };
        const auto translate_mat = glm::translate(identity, translate);
        const auto rotate_mat = glm::mat4_cast(rotate);
        const auto scale_mat = glm::scale(identity, glm::vec3{ scale });

        return translate_mat * rotate_mat * scale_mat;
    }

}  // namespace


namespace mirinae {

    std::vector<glm::mat4> make_skinning_matrix(
        double tick_point,
        const dal::parser::Skeleton& skeleton,
        const dal::parser::Animation& anim
    ) {
        tick_point = std::fmod(tick_point, anim.calc_duration_in_ticks());

        std::unordered_map<std::string, glm::mat4> joint_transforms;
        joint_transforms.reserve(anim.joints_.size());
        for (auto& anim_joint : anim.joints_) {
            const auto m = ::make_joint_transform(tick_point, anim_joint);
            joint_transforms[anim_joint.name_] = m;
        }

        std::vector<glm::mat4> to_parent_mats(skeleton.joints_.size());
        for (size_t i = 0; i < skeleton.joints_.size(); i++) {
            const auto& joint = skeleton.joints_[i];

            if (joint.parent_index_ < 0) {
                to_parent_mats[i] = joint.offset_mat_;
            } else {
                auto parent = skeleton.joints_.at(joint.parent_index_);
                to_parent_mats[i] = glm::inverse(parent.offset_mat_) *
                                    joint.offset_mat_;
            }
        }

        std::vector<glm::mat4> trans_array(skeleton.joints_.size());
        for (size_t i = 0; i < skeleton.joints_.size(); i++) {
            dal::parser::jointID_t cur_jid = i;
            trans_array[i] = glm::inverse(skeleton.joints_[i].offset_mat_);

            while (-1 < cur_jid) {
                auto& cur_joint = skeleton.joints_.at(cur_jid);
                auto found = joint_transforms.find(cur_joint.name_);

                const auto j_mat = (found == joint_transforms.end())
                                       ? glm::mat4{ 1 }
                                       : found->second;

                trans_array.at(i) = to_parent_mats.at(cur_jid) * j_mat *
                                    trans_array.at(i);
                cur_jid = cur_joint.parent_index_;
            }
        }

        for (auto& x : trans_array) {
            x = skeleton.root_transform_ * x;
        }

        return trans_array;
    }

}  // namespace mirinae


// SkinAnimState
namespace mirinae {

    void SkinAnimState::sample_anim(
        glm::mat4* const out_buf,
        const size_t buf_size,
        const double delta_time,
        const dal::parser::Skeleton& skeleton,
        const dal::parser::Animation& anim
    ) {
        tick_ += delta_time * anim.ticks_per_sec_;
        const auto mats = mirinae::make_skinning_matrix(tick_, skeleton, anim);
        const auto copy_size = std::min<size_t>(buf_size, mats.size());
        std::copy(mats.begin(), mats.begin() + copy_size, out_buf);
    }

}  // namespace mirinae
