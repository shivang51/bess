#include "ui/ui_main/project_settings_window.h"
#include "imgui.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {
    void ProjectSettingsWindow::draw() {
        if (!m_shown)
            return;
        const float buttonHeight = ImGui::GetFrameHeight();
        const float textHeight = ImGui::CalcTextSize("ajP").y;
        const float verticalOffset = (buttonHeight - textHeight) / 2.0f;

        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
        ImGui::Begin("Project Settings", &m_shown);

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
        ImGui::End();
    }

    void ProjectSettingsWindow::show() {
        m_projectFile = Pages::MainPage::getTypedInstance()->getState().getCurrentProjectFile();
        m_projectName = m_projectFile->getName();
        m_shown = true;
    }

    void ProjectSettingsWindow::hide() {
        m_shown = false;
    }

    bool ProjectSettingsWindow::isShown() {
        return m_shown;
    }

    bool ProjectSettingsWindow::m_shown = false;
    std::string ProjectSettingsWindow::m_projectName;
    std::shared_ptr<ProjectFile> ProjectSettingsWindow::m_projectFile;

} // namespace Bess::UI
