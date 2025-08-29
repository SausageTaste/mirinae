#pragma once

#include <memory>
#include <variant>

#include <daltools/scene/struct.h>
#include <sung/basic/time.hpp>


namespace mirinae {

    using clock_t = sung::SimClock;


    std::vector<glm::mat4> make_skinning_matrix(
        double tick_point,
        const dal::parser::Skeleton& skeleton,
        const dal::parser::Animation& anim
    );


    class SkelAnimPair {

    public:
        std::optional<size_t> find_anim_idx(const std::string& name);

        size_t joint_count() const { return skel_.joints_.size(); }

    public:
        dal::parser::Skeleton skel_;
        std::vector<dal::parser::Animation> anims_;
    };

    using HSkelAnim = std::shared_ptr<SkelAnimPair>;


    class SkinAnimState {

    public:
        auto& skel() const { return this->skel_anim_->skel_; }
        auto& anims() const { return this->skel_anim_->anims_; }

        std::optional<size_t> get_cur_anim_idx() const;
        std::optional<std::string> get_cur_anim_name() const;

        void sample_anim(
            glm::mat4* const out_buf,
            const size_t buf_size,
            const clock_t& clock
        ) const;

        void update_tick(const clock_t& clock);
        void set_skel_anim(const HSkelAnim& skel_anim);

        void select_anim_index(const size_t index, const clock_t& clock);
        void select_anim_name(const std::string& name, const clock_t& clock);
        void deselect_anim(const clock_t& clock);

        double play_speed() const { return this->selection_.play_speed(); }
        void set_play_speed(const double speed) {
            selection_.set_play_speed(speed);
        }

    private:
        class AnimSelection {

        public:
            std::optional<size_t> index() const;
            std::optional<std::string> name() const;
            double start_sim_time() const { return this->start_sim_time_; }
            double play_speed() const { return this->play_speed_; }

            void set_index(const size_t index, const clock_t& clock);
            void set_name(const std::string& name, const clock_t& clock);
            void set_play_speed(const double speed);

            void reset_anim(const clock_t& clock);

            double local_clock() const { return this->local_clock_; }
            void update_clock(const clock_t& clock);
            void reset_clock();

        private:
            std::variant<std::monostate, size_t, std::string> data_;
            double start_sim_time_ = 0;
            double play_speed_ = 1;
            double local_clock_ = 0;
        };

        class DeferredData {

        public:
            std::optional<double> anim_duration() const;
            std::optional<double> ticks_per_sec() const;
            std::optional<size_t> anim_index() const;

            bool is_ready() const;
            void notify(const AnimSelection& selection, HSkelAnim skel_anim);

        private:
            double ticks_per_sec_;
            double anim_duration_;
            size_t anim_index_;
            bool is_ready_ = false;
        };

        static double calc_tick(
            const clock_t& clock,
            const AnimSelection& selection,
            const DeferredData& deferred
        );

        HSkelAnim skel_anim_;
        AnimSelection selection_;
        DeferredData deferred_data_;
    };

}  // namespace mirinae
