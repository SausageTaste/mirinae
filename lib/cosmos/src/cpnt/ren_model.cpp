#include "mirinae/cpnt/ren_model.hpp"

#include <imgui.h>
#include <daltools/filesys/path.hpp>

#include "mirinae/lightweight/include_spdlog.hpp"


namespace mirinae::cpnt {

    void MdlActorStatic::render_imgui() {
        ImGui::Text("Model path: %s", dal::tostr(model_path_).c_str());

        if (!model_) {
            ImGui::Text("Model not loaded.");
            return;
        }
        if (!actor_) {
            ImGui::Text("Actor not loaded.");
            return;
        }

        if (ImGui::Button("Hide all units")) {
            const auto ren_unit_count = model_->ren_unit_count();
            for (size_t i = 0; i < ren_unit_count; i++) {
                visibility_.set(i, false);
            }
        }

        if (ImGui::CollapsingHeader("Render units")) {
            const auto ren_unit_count = model_->ren_unit_count();

            ImGui::Indent(10);
            for (size_t i = 0; i < ren_unit_count; i++) {
                const auto name = model_->ren_unit_name(i);
                bool visible = visibility_.get(i);
                ImGui::Checkbox(name.data(), &visible);
                visibility_.set(i, visible);
            }
            ImGui::Unindent(10);
        }
    }

    void MdlActorSkinned::render_imgui(const sung::SimClock& clock) {
        ImGui::Text("Model path: %s", dal::tostr(model_path_).c_str());

        if (!model_) {
            ImGui::Text("Model not loaded.");
            return;
        }
        if (!actor_) {
            ImGui::Text("Actor not loaded.");
            return;
        }

        auto& anim = anim_state_;

        float anim_speed = anim.play_speed();
        ImGui::SliderFloat("Animation speed", &anim_speed, 0.0, 10.0);
        anim.set_play_speed(anim_speed);

        const auto anim_name = anim.get_cur_anim_name();
        if (anim_name) {
            ImGui::Text("Current animation: %s", anim_name->c_str());
        }

        if (ImGui::Button("Next animation")) {
            const auto anim_count = anim.anims().size();
            if (anim_count > 0) {
                const auto cur_idx = anim.get_cur_anim_idx();
                if (cur_idx) {
                    const auto next_idx = (*cur_idx + 1) % anim_count;
                    anim.select_anim_index(next_idx, clock);
                } else {
                    anim.select_anim_index(0, clock);
                }
            }
        }

        auto selected_index = anim.get_cur_anim_idx().value_or(0);
        const auto prev_anim_name = anim.get_cur_anim_name().value_or("None");
        ImGui::InputText(
            "Search Animation", search_buffer_.data(), search_buffer_.size()
        );

        if (ImGui::BeginCombo("Animations", prev_anim_name.c_str())) {
            for (int i = 0; i < anim.anims().size(); i++) {
                const auto& i_anim = anim.anims().at(i);
                const auto similarity = std::strstr(
                    i_anim.name_.c_str(), search_buffer_.data()
                );
                if (similarity) {  // Filter based on input
                    bool is_selected = (selected_index == i);
                    if (ImGui::Selectable(
                            anim.anims()[i].name_.c_str(), is_selected
                        )) {
                        selected_index = i;
                        anim.select_anim_index(i, clock);
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Hide all units")) {
            const auto ren_unit_count = model_->ren_unit_count();
            for (size_t i = 0; i < ren_unit_count; i++) {
                visibility_.set(i, false);
            }
        }

        if (ImGui::CollapsingHeader("Render units")) {
            const auto ren_unit_count = model_->ren_unit_count();

            ImGui::Indent(10);
            for (size_t i = 0; i < ren_unit_count; i++) {
                const auto name = model_->ren_unit_name(i);
                bool visible = visibility_.get(i);
                ImGui::Checkbox(name.data(), &visible);
                visibility_.set(i, visible);
            }
            ImGui::Unindent(10);
        }
    }

}  // namespace mirinae::cpnt
