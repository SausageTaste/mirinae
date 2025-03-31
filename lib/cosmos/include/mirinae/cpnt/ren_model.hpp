#pragma once

#include <array>
#include <filesystem>

#include <glm/vec3.hpp>

#include "mirinae/lightweight/skin_anim.hpp"


namespace mirinae {

    struct IModelAccessor {
        virtual ~IModelAccessor() = default;

        virtual bool position(const glm::vec3& p) = 0;
    };


    struct IRenModel {
        virtual ~IRenModel() = default;

        virtual bool is_ready() const = 0;
        virtual void access_positions(IModelAccessor& acc) const = 0;

        virtual size_t ren_unit_count() const = 0;
        virtual std::string_view ren_unit_name(size_t index) const = 0;
    };


    struct IRenActor {
        virtual ~IRenActor() = default;
    };


    class VisibilityArray {

    public:
        bool get(size_t index) const {
            if (index >= MAX_SIZE)
                return true;
            if (index >= hidden_.size())
                return true;
            return !hidden_.at(index);
        }

        void set(size_t index, bool visible) {
            if (index >= MAX_SIZE)
                return;

            if (index >= hidden_.size()) {
                if (visible)
                    return;
                else
                    hidden_.resize(index + 1);
            }

            hidden_.at(index) = !visible;
        }

    private:
        constexpr static size_t MAX_SIZE = 128;
        std::vector<bool> hidden_;
    };

}  // namespace mirinae


namespace mirinae::cpnt {

    class MdlActorStatic {

    public:
        void render_imgui();

        template <typename T>
        T* get_model() {
            return dynamic_cast<T*>(model_.get());
        }

        template <typename T>
        T* get_actor() {
            return dynamic_cast<T*>(actor_.get());
        }

    public:
        std::filesystem::path model_path_;
        VisibilityArray visibility_;

        // Managed by renderer
        std::shared_ptr<IRenModel> model_;
        std::shared_ptr<IRenActor> actor_;
    };


    class MdlActorSkinned {

    public:
        void render_imgui(const sung::SimClock& clock);

        template <typename T>
        T* get_model() {
            return dynamic_cast<T*>(model_.get());
        }

        template <typename T>
        T* get_actor() {
            return dynamic_cast<T*>(actor_.get());
        }

    public:
        std::filesystem::path model_path_;
        VisibilityArray visibility_;
        SkinAnimState anim_state_;

        // Managed by renderer
        std::shared_ptr<IRenModel> model_;
        std::shared_ptr<IRenActor> actor_;

    private:
        std::array<char, 128> search_buffer_{};  // For ImGui
    };

}  // namespace mirinae::cpnt
