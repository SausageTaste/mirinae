#pragma once

#include <memory>
#include <variant>

#include <daltools/struct.h>


namespace mirinae {

    std::vector<glm::mat4> make_skinning_matrix(
        double tick_point,
        const dal::parser::Skeleton& skeleton,
        const dal::parser::Animation& anim
    );


    class SkelAnimPair {

    public:
        std::optional<size_t> find_anim_idx(const std::string& name);

    public:
        dal::parser::Skeleton skel_;
        std::vector<dal::parser::Animation> anims_;
    };

    using HSkelAnim = std::shared_ptr<SkelAnimPair>;


    class SkinAnimState {

    public:
        auto& skel() const { return this->skel_anim_->skel_; }
        auto& anims() const { return this->skel_anim_->anims_; }

        void sample_anim(
            glm::mat4* const out_buf,
            const size_t buf_size,
            const double delta_time
        );

        void set_skel_anim(const HSkelAnim& skel_anim);

        void select_anim_index(const size_t index);
        void select_anim_name(const std::string& name);
        void deselect_anim();

        double play_speed_ = 1;

    private:
        class AnimSelection {

        public:
            std::optional<size_t> index() const;
            std::optional<std::string> name() const;
            void set_index(const size_t index);
            void set_name(const std::string& name);
            void reset();

        private:
            std::variant<std::monostate, size_t, std::string> data_;
        };

        class DeferredData {

        public:
            std::optional<double> anim_duration() const;
            std::optional<size_t> anim_index() const;

            bool is_ready() const;
            void notify(const AnimSelection& selection, HSkelAnim skel_anim);

        private:
            double anim_duration_ = 10;
            size_t anim_index_ = 0;
            bool is_ready_ = false;
        };

        HSkelAnim skel_anim_;
        AnimSelection selection_;
        DeferredData deferred_data_;
        double tick_ = 0;
    };

}  // namespace mirinae
