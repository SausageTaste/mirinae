#include "mirinae/lightweight/skin_anim.hpp"

#include <algorithm>
#include <unordered_map>

#include "mirinae/lightweight/include_spdlog.hpp"


namespace {

    double mod_tick(double tick, double lower, double upper) {
        const auto span = upper - lower;
        const auto into_floor = (tick - lower) / span;
        return tick - std::floor(into_floor) * span;
    }

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
        double tick, const dal::AnimJoint& anim_joint
    ) {
        if (anim_joint.translations_.empty())
            return glm::vec3{ 0 };
        else
            return ::make_interp_value(tick, anim_joint.translations_);
    }

    glm::quat make_joint_rotation(
        double tick, const dal::AnimJoint& anim_joint
    ) {
        if (anim_joint.rotations_.empty())
            return glm::quat{ 1, 0, 0, 0 };
        else
            return ::make_interp_value(tick, anim_joint.rotations_);
    }

    float make_joint_scale(double tick, const dal::AnimJoint& anim_joint) {
        if (anim_joint.scales_.empty())
            return 1;
        else
            return ::make_interp_value(tick, anim_joint.scales_);
    }

    glm::mat4 make_joint_transform(
        double tick, const dal::AnimJoint& anim_joint
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


    void set_all_identity(glm::mat4* const mats, const size_t size) {
        for (size_t i = 0; i < size; i++) {
            mats[i] = glm::mat4{ 1 };
        }
    }

}  // namespace


// Free functions
namespace mirinae {

    std::vector<glm::mat4> make_skinning_matrix(
        double tick_point,
        const dal::Skeleton& skeleton,
        const dal::Animation& anim
    ) {
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
            dal::jointID_t cur_jid = i;
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


// SkelAnimPair
namespace mirinae {

    std::optional<size_t> SkelAnimPair::find_anim_idx(const std::string& name) {
        for (size_t i = 0; i < this->anims_.size(); i++) {
            if (this->anims_.at(i).name_ == name) {
                return i;
            }
        }

        return std::nullopt;
    }

}  // namespace mirinae


// SkinAnimState::AnimSelection
namespace mirinae {

    std::optional<size_t> SkinAnimState::AnimSelection::index() const {
        if (data_.index() == 1)
            return std::get<size_t>(data_);
        else
            return std::nullopt;
    }

    std::optional<std::string> SkinAnimState::AnimSelection::name() const {
        if (data_.index() == 2)
            return std::get<std::string>(data_);
        else
            return std::nullopt;
    }

    void SkinAnimState::AnimSelection::set_index(
        const size_t index, const clock_t& clock
    ) {
        data_ = index;
        start_sim_time_ = clock.t();
    }

    void SkinAnimState::AnimSelection::set_name(
        const std::string& name, const clock_t& clock
    ) {
        data_ = name;
        start_sim_time_ = clock.t();
    }

    void SkinAnimState::AnimSelection::set_play_speed(const double speed) {
        play_speed_ = speed;
    }

    void SkinAnimState::AnimSelection::reset_anim(const clock_t& clock) {
        data_ = std::monostate{};
        start_sim_time_ = clock.t();
    }

    void SkinAnimState::AnimSelection::update_clock(const clock_t& clock) {
        local_clock_ += clock.dt() * play_speed_;
    }

    void SkinAnimState::AnimSelection::reset_clock() { local_clock_ = 0; }

}  // namespace mirinae


// SkinAnimState::DeferredData
namespace mirinae {

    std::optional<double> SkinAnimState::DeferredData::anim_duration() const {
        if (is_ready_)
            return anim_duration_;
        else
            return std::nullopt;
    }

    std::optional<double> SkinAnimState::DeferredData::ticks_per_sec() const {
        if (is_ready_)
            return ticks_per_sec_;
        else
            return std::nullopt;
    }

    std::optional<size_t> SkinAnimState::DeferredData::anim_index() const {
        if (is_ready_)
            return anim_index_;
        else
            return std::nullopt;
    }

    bool SkinAnimState::DeferredData::is_ready() const { return is_ready_; }

    void SkinAnimState::DeferredData::notify(
        const AnimSelection& selection, HSkelAnim skel_anim
    ) {
        if (!skel_anim) {
            is_ready_ = false;
            return;
        }

        if (const auto idx = selection.index()) {
            if (idx.value() < skel_anim->anims_.size()) {
                auto& anim = skel_anim->anims_.at(*idx);
                ticks_per_sec_ = anim.ticks_per_sec_;
                anim_duration_ = anim.calc_duration_in_ticks();
                anim_index_ = *idx;
                is_ready_ = true;
                return;
            }
        } else if (const auto name = selection.name()) {
            if (auto idx = skel_anim->find_anim_idx(*name)) {
                auto& anim = skel_anim->anims_.at(*idx);
                ticks_per_sec_ = anim.ticks_per_sec_;
                anim_duration_ = anim.calc_duration_in_ticks();
                anim_index_ = *idx;
                is_ready_ = true;
                return;
            }
        }

        is_ready_ = false;
    }

}  // namespace mirinae


// SkinAnimState
namespace mirinae {

    std::optional<size_t> SkinAnimState::get_cur_anim_idx() const {
        if (auto idx = deferred_data_.anim_index())
            return idx;
        if (auto idx = selection_.index())
            return idx;
        return std::nullopt;
    }

    std::optional<std::string> SkinAnimState::get_cur_anim_name() const {
        if (auto idx = deferred_data_.anim_index()) {
            if (skel_anim_) {
                return skel_anim_->anims_.at(*idx).name_;
            }
        }

        if (auto name = selection_.name())
            return name;

        return std::nullopt;
    }

    void SkinAnimState::sample_anim(
        glm::mat4* const out_buf, const size_t buf_size, const clock_t& clock
    ) const {
        const auto anim_idx = deferred_data_.anim_index();
        if (!anim_idx)
            return ::set_all_identity(out_buf, buf_size);

        const auto anim_duration = deferred_data_.anim_duration();
        if (!anim_duration)
            return ::set_all_identity(out_buf, buf_size);

        auto& anim = this->anims()[*anim_idx];
        if (buf_size < this->skel().joints_.size()) {
            SPDLOG_WARN(
                "Buffer size ({}) is too small to store all joint matrices "
                "({}) in '{}'",
                buf_size,
                this->skel().joints_.size(),
                anim.name_
            );
            return ::set_all_identity(out_buf, buf_size);
        }

        const auto tick = this->calc_tick(clock, selection_, deferred_data_);
        const auto mtick = ::mod_tick(tick, 0, anim_duration.value());
        const auto mats = make_skinning_matrix(mtick, this->skel(), anim);
        const size_t copy_size = std::min(buf_size, mats.size());
        std::copy(mats.begin(), mats.begin() + copy_size, out_buf);
    }

    void SkinAnimState::update_tick(const clock_t& clock) {
        selection_.update_clock(clock);
    }

    void SkinAnimState::set_skel_anim(const HSkelAnim& skel_anim) {
        skel_anim_ = skel_anim;
        deferred_data_.notify(selection_, skel_anim_);
    }

    void SkinAnimState::select_anim_index(
        const size_t index, const clock_t& clock
    ) {
        selection_.set_index(index, clock);
        selection_.reset_clock();
        deferred_data_.notify(selection_, skel_anim_);
    }

    void SkinAnimState::select_anim_name(
        const std::string& name, const clock_t& clock
    ) {
        selection_.set_name(name, clock);
        selection_.reset_clock();
        deferred_data_.notify(selection_, skel_anim_);
    }

    void SkinAnimState::deselect_anim(const clock_t& clock) {
        selection_.reset_anim(clock);
        selection_.reset_clock();
        deferred_data_.notify(selection_, skel_anim_);
    }

    double SkinAnimState::calc_tick(
        const clock_t& clock,
        const SkinAnimState::AnimSelection& selection,
        const SkinAnimState::DeferredData& deferred
    ) {
        const auto elapsed = selection.local_clock();
        const auto ticks_per_sec = deferred.ticks_per_sec();
        if (!ticks_per_sec)
            return 0;

        return elapsed * ticks_per_sec.value();
    }

}  // namespace mirinae
