#include "ui/ui_main/popups.h"

#include "common/logger.h"
#include "imgui.h"

namespace Bess::UI {

    Popups::PopupRes Popups::handleUnsavedProjectWarning() {
        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(
            center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        PopupRes val = PopupRes::none;

        if (ImGui::BeginPopupModal(PopupIds::unsavedProjectWarning,
                                   nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("All the changes will be lost if you don't save "
                               "current project. Save Changes?");
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

    void Popups::showAboutPopup() {
        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(
            center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(
                PopupIds::about, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("BESS - Basic Electrical Simulation Software");
            ImGui::Text("Version: <dev>");
            ImGui::Text("");
            ImGui::Text("");
            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

} // namespace Bess::UI
