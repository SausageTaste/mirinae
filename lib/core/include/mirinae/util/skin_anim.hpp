#pragma once

#include <memory>

#include <daltools/struct.h>


namespace mirinae {

    std::vector<glm::mat4> make_skinning_matrix(
        double tick_point,
        const dal::parser::Skeleton& skeleton,
        const dal::parser::Animation& anim
    );


    class SkelAnimPair {

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

        bool set_anim_index(const size_t index);
        bool set_anim_name(const std::string& name);

        HSkelAnim skel_anim_;

    private:
        double tick_ = 0;
        size_t anim_index_ = 0;
    };

}  // namespace mirinae
