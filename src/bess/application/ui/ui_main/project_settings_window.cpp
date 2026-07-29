#include "ui/ui_main/project_settings_window.h"
#include "bess_core/g_app_context.h"
#include "imgui.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "project_session/project_session.h"
#include "ui/ui_main/ui_main.h"
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
        auto &state = Pages::MainPage::getInstance()->getState();
        auto &session =
            *GAppContext::getInstance().getSubSystem<ProjectSession>();
        const auto path = session.doc().path().string();
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
        ImGui::TextDisabled("%s", path.empty() ? "Not saved" : path.c_str());
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - verticalOffset);
        bool save = false;
        if (path.empty() && ImGui::Button("Browse")) {
            save = true;
        }

        if (ImGui::Button("Save")) {
            save = true;
        }

        if (save) {
            const auto result = session.setName(m_projectName);
            if (!result) {
                UIMain::getState()._internalData.statusMessage =
                    "Could not rename project: " + result.status.msg();
                return;
            }
            state.actionFlags.saveProject = true;
        }
    }

    void ProjectSettingsWindow::onShow() {
        m_projectName = GAppContext::getInstance()
                            .getSubSystem<ProjectSession>()
                            ->doc()
                            .name();
    }
} // namespace Bess::UI
