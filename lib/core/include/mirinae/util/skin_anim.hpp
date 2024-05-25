#pragma once

#include <daltools/struct.h>


namespace mirinae {

    std::vector<glm::mat4> make_skinning_matrix(
        double tick_point,
        const dal::parser::Skeleton& skeleton,
        const dal::parser::Animation& anim
    );


    class SkinAnimState {

    public:
        void sample_anim(
            glm::mat4* const out_buf,
            const size_t buf_size,
            const double delta_time,
            const dal::parser::Skeleton& skeleton,
            const dal::parser::Animation& anim
        );

    private:
        double tick_ = 0;
        size_t anim_index_ = 0;
    };

}  // namespace mirinae
