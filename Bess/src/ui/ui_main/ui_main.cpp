#include "ui/ui_main/ui_main.h"

#include "application_state.h"
#include "glad/glad.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <string>

#include "camera.h"
#include "components_manager/components_manager.h"
#include "pages/main_page/main_page_state.h"
#include "scene/renderer/gl/gl_wrapper.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/icons/MaterialIcons.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/dialogs.h"
#include "ui/ui_main/popups.h"
#include "ui/ui_main/project_settings_window.h"
#include "ui/ui_main/properties_panel.h"
#include "ui/ui_main/settings_window.h"

namespace Bess::UI {
    UIState UIMain::state{};
    std::shared_ptr<Pages::MainPageState> UIMain::m_pageState;

    void UIMain::draw() {
        static bool firstTime = true;
        if (firstTime) {
            firstTime = false;
            m_pageState = Pages::MainPageState::getInstance();
            resetDockspace();
        }
        drawMenubar();
        drawProjectExplorer();
        drawViewport();
        ComponentExplorer::draw();
        PropertiesPanel::draw();
        drawExternalWindows();
        // ImGui::ShowDemoWindow();
    }

    void UIMain::drawStats(int fps) {
        auto stats = Gl::Api::getStats();
        ImGui::Text("Draw Calls: %d", stats.drawCalls);
        ImGui::Text("Vertices: %d", stats.vertices);
        ImGui::Text("GL Check Calls: %d", stats.glCheckCalls);
    }

    void UIMain::setViewportTexture(GLuint64 texture) {
        state.viewportTexture = texture;
    }

    void UIMain::drawProjectExplorer() {
        ImGui::Begin("Project Explorer");

        for (auto &id : Simulator::ComponentsManager::renderComponents) {
            auto &entity = Simulator::ComponentsManager::components[id];
            if (ImGui::Selectable(entity->getRenderName().c_str(), m_pageState->isBulkIdPresent(entity->getId()))) {
                m_pageState->setBulkId(entity->getId());
            }
        }

        ImGui::End();
    }

    void UIMain::drawMenubar() {
        bool newFileClicked = false, openFileClicked = false;
        ImGui::BeginMainMenuBar();
        if (ImGui::BeginMenu("File")) {
            // New File
            std::string temp_name = Icons::FontAwesomeIcons::FA_FILE_ALT;
            temp_name += "   New";
            if (ImGui::MenuItem(temp_name.c_str(), "Ctrl+N")) {
                newFileClicked = true;
            };

            // Open File
            temp_name = Icons::FontAwesomeIcons::FA_FOLDER_OPEN;
            temp_name += "  Open";
            if (ImGui::MenuItem(temp_name.c_str(), "Ctrl+O")) {
                openFileClicked = true;
            };

            // Save File
            temp_name = Icons::FontAwesomeIcons::FA_SAVE;
            temp_name += "   Save";
            if (ImGui::MenuItem(temp_name.c_str(), "Ctrl+S")) {
                m_pageState->getCurrentProjectFile()->update(Simulator::ComponentsManager::components);
                m_pageState->getCurrentProjectFile()->save();
            };

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            temp_name = Icons::FontAwesomeIcons::FA_WRENCH;
            temp_name += "  Settings";
            if (ImGui::MenuItem(temp_name.c_str())) {
                SettingsWindow::show();
            }

            temp_name = Icons::FontAwesomeIcons::FA_FILE_EXPORT;
            temp_name += "  Export";
            if (ImGui::BeginMenu(temp_name.c_str())) {
                temp_name = Icons::FontAwesomeIcons::FA_FILE_IMAGE;
                temp_name += "  Export to Image";
                if (ImGui::MenuItem(temp_name.c_str())) {
                }
                ImGui::EndMenu();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Exit
            if (ImGui::MenuItem("Quit", "")) {
                ApplicationState::quit();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {

            if (ImGui::MenuItem("Project Settings", "Ctrl+P")) {
                ProjectSettingsWindow::show();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Simulation")) {
            std::string text = m_pageState->isSimulationPaused() ? Icons::FontAwesomeIcons::FA_PLAY : Icons::FontAwesomeIcons::FA_PAUSE;
            text += m_pageState->isSimulationPaused() ? " Play" : " Pause";

            if (ImGui::MenuItem(text.c_str(), "Ctrl+Space")) {
                m_pageState->setSimulationPaused(!m_pageState->isSimulationPaused());
            }
            ImGui::EndMenu();
        }

        auto menubar_size = ImGui::GetWindowSize();
        ImGui::EndMainMenuBar();

        if (newFileClicked) {
            onNewProject();
        } else if (openFileClicked) {
            onOpenProject();
        }

        Popups::PopupRes res;
        if ((res = Popups::handleUnsavedProjectWarning()) != Popups::PopupRes::none) {
            if (res == Popups::PopupRes::yes) {
                m_pageState->saveCurrentProject();
                if (!m_pageState->getCurrentProjectFile()->isSaved()) {
                    state._internalData.newFileClicked = false;
                    state._internalData.openFileClicked = false;
                    return;
                }
            }

            if (res != Popups::PopupRes::cancel) {
                if (state._internalData.newFileClicked) {
                    m_pageState->createNewProject();
                    state._internalData.newFileClicked = false;
                } else if (state._internalData.openFileClicked) {
                    m_pageState->loadProject(state._internalData.path);
                    state._internalData.path = "";
                    state._internalData.openFileClicked = false;
                }
            }
        }
    }

    void UIMain::drawViewport() {
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoDecoration;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::SetNextWindowSizeConstraints({400.f, 400.f}, {-1.f, -1.f});

        ImGui::Begin("Viewport", nullptr, flags);

        auto viewportPanelSize = ImGui::GetContentRegionAvail();
        state.viewportSize = {viewportPanelSize.x, viewportPanelSize.y};

        auto offset = ImGui::GetCursorPos();

        ImGui::Image((void *)state.viewportTexture,
                     ImVec2(viewportPanelSize.x, viewportPanelSize.y), ImVec2(0, 1),
                     ImVec2(1, 0));

        auto pos = ImGui::GetWindowPos();
        auto gPos = ImGui::GetMainViewport()->Pos;
        state.viewportPos = {pos.x - gPos.x + offset.x + 5.f, pos.y - gPos.y + offset.y + 5.f};

        ImGui::End();
        ImGui::PopStyleVar();

        // Camera controls
        {
            ImGui::SetNextWindowPos(
                {pos.x + viewportPanelSize.x - 208, pos.y + viewportPanelSize.y - 40});
            ImGui::SetNextWindowSize({208, 0});
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

            ImGui::Begin("Camera", nullptr, flags);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
            ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 8);
            if (ImGui::SliderFloat("Zoom", &state.cameraZoom, Camera::zoomMin,
                                   Camera::zoomMax, nullptr,
                                   ImGuiSliderFlags_AlwaysClamp)) {
                // step size logic
                float stepSize = 0.1f;
                state.cameraZoom = roundf(state.cameraZoom / stepSize) * stepSize;
            }
            ImGui::PopStyleVar(2);
            ImGui::End();
            ImGui::PopStyleVar(2);
        }
    }

    void UIMain::resetDockspace() {
        auto mainDockspaceId = ImGui::GetID("MainDockspace");

        ImGui::DockBuilderRemoveNode(mainDockspaceId);
        ImGui::DockBuilderAddNode(mainDockspaceId, ImGuiDockNodeFlags_NoTabBar);

        auto dock_id_left = ImGui::DockBuilderSplitNode(mainDockspaceId, ImGuiDir_Left, 0.15f, nullptr, &mainDockspaceId);
        auto dock_id_right = ImGui::DockBuilderSplitNode(mainDockspaceId, ImGuiDir_Right, 0.25f, nullptr, &mainDockspaceId);

        auto dock_id_right_bot = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.5f, nullptr, &dock_id_right);

        ImGui::DockBuilderDockWindow("Component Explorer", dock_id_left);
        ImGui::DockBuilderDockWindow("Viewport", mainDockspaceId);
        ImGui::DockBuilderDockWindow("Project Explorer", dock_id_right);
        ImGui::DockBuilderDockWindow("Properties", dock_id_right_bot);

        ImGui::DockBuilderFinish(mainDockspaceId);
    }

    void UIMain::drawExternalWindows() {
        SettingsWindow::draw();
        ProjectSettingsWindow::draw();
    }

    void UIMain::onNewProject() {
        if (!m_pageState->getCurrentProjectFile()->isSaved()) {
            state._internalData.newFileClicked = true;
            ImGui::OpenPopup(Popups::PopupIds::unsavedProjectWarning.c_str());
        } else {
            m_pageState->createNewProject();
        }
    }

    void UIMain::onOpenProject() {

        auto filepath =
            Dialogs::showOpenFileDialog("Open BESS Project File", "*.bproj|");

        if (filepath == "" || !std::filesystem::exists(filepath))
            return;

        if (!m_pageState->getCurrentProjectFile()->isSaved()) {
            state._internalData.openFileClicked = true;
            state._internalData.path = filepath;
            ImGui::OpenPopup(Popups::PopupIds::unsavedProjectWarning.c_str());
        } else {
            m_pageState->loadProject(filepath);
        }
    }

    void UIMain::onSaveProject() {}

} // namespace Bess::UI
