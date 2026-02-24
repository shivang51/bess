#include "ui/ui_main/project_settings_window.h"
#include "imgui.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {
    ProjectSettingsWindow::ProjectSettingsWindow() : Panel("Project Settings") {
        m_showInMenuBar = false;
        m_defaultDock = Dock::none;
    }

    void ProjectSettingsWindow::onBeforeDraw() {
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    }

    void ProjectSettingsWindow::onDraw() {
        const float buttonHeight = ImGui::GetFrameHeight();
        const float textHeight = ImGui::CalcTextSize("ajP").y;
        const float verticalOffset = (buttonHeight - textHeight) / 2.0f;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + verticalOffset);
        ImGui::Text("Project Name");
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - verticalOffset);
        Widgets::TextBox("##Project Name", m_projectName);

        ImGui::Spacing();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + verticalOffset);
        ImGui::Text("Project Path");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_projectFile->getPath().c_str());
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - verticalOffset);
        if (m_projectFile->getPath() == "" && ImGui::Button("Browse")) {
        }

        if (ImGui::Button("Save")) {
            m_projectFile->getNameRef() = m_projectName;
            m_projectFile->save();
        }
    }

    void ProjectSettingsWindow::onShow() {
        m_projectFile = Pages::MainPage::getInstance()->getState().getCurrentProjectFile();
        m_projectName = m_projectFile->getName();
    }
} // namespace Bess::UI
