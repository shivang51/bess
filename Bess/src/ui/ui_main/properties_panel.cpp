#include "ui/ui_main/properties_panel.h"
#include "components_manager/components_manager.h"
#include "pages/main_page/main_page_state.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/ui_main/ui_main.h"
#include <imgui.h>

namespace Bess::UI {
    void PropertiesPanel::draw() {
        ImGui::Begin("Properties");

        if (Pages::MainPageState::getInstance()->getBulkIds().size() != 1) {
            ImGui::End();
            return;
        }

        bool deleted = false;
        auto &selectedEnt = Simulator::ComponentsManager::components[Pages::MainPageState::getInstance()->getBulkIdAt(0)];
        if (selectedEnt == nullptr)
            goto end;

        // NAME
        {
            float textHeight = ImGui::GetTextLineHeight(), buttonHeight = ImGui::GetFrameHeight();
            float verticalOffset = (buttonHeight - textHeight) * 0.5f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + verticalOffset);
            ImGui::Text("%s", selectedEnt->getName().c_str());
            ImGui::SameLine();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - verticalOffset);
        }
        // DELETE BUTTON
        {
            std::string temp = Icons::FontAwesomeIcons::FA_TRASH;
            temp += " Delete";

            float windowWidth = ImGui::GetWindowSize().x;
            float buttonWidth = ImGui::CalcTextSize(temp.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f; // Add padding to button width
            float spacing = ImGui::GetStyle().ItemSpacing.x;

            ImGui::SameLine(windowWidth - buttonWidth - spacing);
            ImVec4 deleteButtonColor = ImVec4(217.0f / 255.0f, 83.0f / 255.0f, 79.0f / 255.0f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, deleteButtonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(255.0f / 255.0f, 83.0f / 255.0f, 79.0f / 255.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(185.0f / 255.0f, 62.0f / 255.0f, 58.0f / 255.0f, 1.0f));

            if (ImGui::Button(temp.c_str())) {
                Simulator::ComponentsManager::deleteComponent(Pages::MainPageState::getInstance()->getBulkIdAt(0));
                deleted = true;
            }
            ImGui::PopStyleColor(3);
        }

        if (!deleted)
            selectedEnt->drawProperties();
    end:
        ImGui::End();
    }
} // namespace Bess::UI
