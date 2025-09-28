#include "ui/ui_main/popups.h"

#include "imgui.h"

namespace Bess::UI {
    const std::string Popups::PopupIds::unsavedProjectWarning = "Save Current Project";

    Popups::PopupRes Popups::handleUnsavedProjectWarning() {
        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        auto val = PopupRes::none;

        if (ImGui::BeginPopupModal(PopupIds::unsavedProjectWarning.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("All the changes will be lost if you don't save current project. Save Changes?");
            ImGui::Separator();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                val = PopupRes::cancel;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0))) {
                val = PopupRes::no;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            ImGui::SetItemDefaultFocus();
            if (ImGui::Button("Yes", ImVec2(120, 0))) {
                val = PopupRes::yes;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        return val;
    }
} // namespace Bess::UI
