#pragma once


namespace mirinae {

    class FlagShip {

    public:
        bool need_resize() const { return need_resize_; }
        void set_need_resize(bool flag) { need_resize_ = flag; }

        bool dont_render() const { return dont_render_; }
        void set_dont_render(bool flag) { dont_render_ = flag; }

    private:
        bool need_resize_{ false };
        bool dont_render_{ false };
    };

}  // namespace mirinae
