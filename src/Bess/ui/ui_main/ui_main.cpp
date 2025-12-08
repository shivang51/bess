#include "ui/ui_main/ui_main.h"
#include "application/application_state.h"
#include "common/log.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "simulation_engine.h"
#include "stb_image_write.h"
#include "ui/ui_main/scene_export_window.h"
#include "ui/widgets/m_widgets.h"
#include <string>

#include "pages/main_page/main_page_state.h"
#include "scene/scene.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/dialogs.h"
#include "ui/ui_main/graph_view_window.h"
#include "ui/ui_main/popups.h"
#include "ui/ui_main/project_explorer.h"
#include "ui/ui_main/project_settings_window.h"
#include "ui/ui_main/properties_panel.h"
#include "ui/ui_main/settings_window.h"
#include <filesystem>

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
        drawViewport();
        ComponentExplorer::draw();
        ProjectExplorer::draw();
        PropertiesPanel::draw();
        GraphViewWindow::draw();

        drawStatusbar();
        drawExternalWindows();
    }

    void UIMain::setViewportTexture(const uint64_t texture) {
        state.mainViewport.setViewportTexture(texture);
    }

    ImVec2 getTextSize(const std::string &text, const bool includePadding = true) {
        auto size = ImGui::CalcTextSize(text.c_str());
        if (!includePadding)
            return size;
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const auto style = g.Style;
        size.x += style.FramePadding.x * 2;
        size.y += style.FramePadding.y * 2;
        return size;
    }

    void UIMain::drawStatusbar() {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        auto style = g.Style;
        ImGuiViewportP *viewport = (ImGuiViewportP *)(void *)ImGui::GetMainViewport();
        const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
        auto &simEngine = SimEngine::SimulationEngine::instance();
        const float height = ImGui::GetFrameHeight();
        if (ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, height, window_flags)) {
            if (ImGui::BeginMenuBar()) {
                if (simEngine.getSimulationState() == SimEngine::SimulationState::running) {
                    ImGui::Text("Simulation Running");
                } else if (simEngine.getSimulationState() == SimEngine::SimulationState::paused) {
                    ImGui::Text("Simulation Paused");
                } else {
                    ImGui::Text("Unknown State");
                }

                // std::string rightContent[] = {};
                // float offset = style.FramePadding.x;
                // for (auto &content : rightContent)
                //     offset += getTextSize(content).x;

                // ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - offset);
                // for (auto &content : rightContent)
                //     ImGui::Text("%s", content.c_str());
                ImGui::EndMenuBar();
            }
            ImGui::End();
        }
    }

    void UIMain::drawMenubar() {
        bool newFileClicked = false, openFileClicked = false;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 6.f));
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
                onSaveProject();
            };

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            temp_name = Icons::FontAwesomeIcons::FA_PENCIL_ALT;
            temp_name += "  Prefrences";
            if (ImGui::MenuItem(temp_name.c_str())) {
                SettingsWindow::show();
            }

            temp_name = Icons::FontAwesomeIcons::FA_FILE_EXPORT;
            temp_name += "  Export";
            if (ImGui::BeginMenu(temp_name.c_str())) {
                temp_name = Icons::FontAwesomeIcons::FA_FILE_IMAGE;
                temp_name += "  Scene View PNG";
                if (ImGui::MenuItem(temp_name.c_str())) {
                    SceneExportWindow::show();
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
            static auto scene = Canvas::Scene::instance();
            static auto &cmdManager = scene->getCmdManager();

            std::string icon = Icons::FontAwesomeIcons::FA_UNDO;
            if (ImGui::MenuItem((icon + "  Undo").c_str(), "Ctrl+Z", false, cmdManager.canUndo())) {
                cmdManager.undo();
            }

            icon = Icons::FontAwesomeIcons::FA_REDO;
            if (ImGui::MenuItem((icon + "  Redo").c_str(), "Ctrl+Shift+Z", false, cmdManager.canRedo())) {
                cmdManager.redo();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            icon = Icons::FontAwesomeIcons::FA_WRENCH;
            if (ImGui::MenuItem((icon + "  Project Settings").c_str(), "Ctrl+P")) {
                ProjectSettingsWindow::show();
            }

            ImGui::EndMenu();
        }

        ImGui::SetNextWindowSize(ImVec2(300, 0));
        if (ImGui::BeginMenu("View")) {

            Widgets::CheckboxWithLabel(ProjectExplorer::windowName.data(), &ProjectExplorer::isShown);

            Widgets::CheckboxWithLabel(ComponentExplorer::windowName.data(), &ComponentExplorer::isShown);

            Widgets::CheckboxWithLabel(PropertiesPanel::windowName.data(), &PropertiesPanel::isShown);

            Widgets::CheckboxWithLabel(GraphViewWindow::windowName.data(), &GraphViewWindow::isShown);

            ImGui::EndMenu();
        }

        const auto menubar_size = ImGui::GetWindowSize();

        // project name textbox - begin

        const auto &style = ImGui::GetStyle();
        auto &name = Pages::MainPageState::getInstance()->getCurrentProjectFile()->getNameRef();
        const auto fontSize = ImGui::CalcTextSize(name.c_str());
        auto width = fontSize.x + (style.FramePadding.x * 2);
        if (width < 150)
            width = 150;
        else if (width > 200)
            width = 200;

        ImGui::PushItemWidth(width);
        ImGui::SameLine((menubar_size.x / 2.f) - (width / 2.f)); // Align to the right side
        ImGui::SetCursorPosY(((menubar_size.y - ImGui::GetFontSize()) / 2.f) - 2.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 2.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, style.Colors[ImGuiCol_WindowBg]);
        Widgets::TextBox("", name, "Project Name");
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        state._internalData.isTbFocused = ImGui::IsItemFocused();

        // project name textbox - end

        ImGui::EndMainMenuBar();
        ImGui::PopStyleVar(2);

        if (newFileClicked) {
            onNewProject();
        } else if (openFileClicked) {
            onOpenProject();
        }

        Popups::PopupRes res{};
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

    /// will change when moving to multiple viewports
    /// need to think about scenes and viewports
    /// what does each actually mean, own and represent
    void UIMain::drawViewport() {
        state.mainViewport.draw();
    }

    void UIMain::resetDockspace() {
        auto mainDockspaceId = ImGui::GetID("MainDockspace");

        ImGui::DockBuilderRemoveNode(mainDockspaceId);
        ImGui::DockBuilderAddNode(mainDockspaceId, ImGuiDockNodeFlags_NoTabBar);

        const auto dock_id_left = ImGui::DockBuilderSplitNode(mainDockspaceId, ImGuiDir_Left, 0.15f, nullptr, &mainDockspaceId);
        auto dock_id_right = ImGui::DockBuilderSplitNode(mainDockspaceId, ImGuiDir_Right, 0.25f, nullptr, &mainDockspaceId);
        const auto dock_id_bot = ImGui::DockBuilderSplitNode(mainDockspaceId, ImGuiDir_Down, 0.25f, nullptr, &mainDockspaceId);

        const auto dock_id_right_bot = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.5f, nullptr, &dock_id_right);

        // ImGui::DockBuilderDockWindow(ComponentExplorer::windowName.data(), dock_id_left);
        ImGui::DockBuilderDockWindow("MainViewport", mainDockspaceId);
        ImGui::DockBuilderDockWindow(ProjectExplorer::windowName.data(), dock_id_left);
        ImGui::DockBuilderDockWindow(PropertiesPanel::windowName.data(), dock_id_right);
        ImGui::DockBuilderDockWindow(GraphViewWindow::windowName.data(), dock_id_bot);

        ImGui::DockBuilderFinish(mainDockspaceId);
    }

    void UIMain::drawExternalWindows() {
        SettingsWindow::draw();
        ProjectSettingsWindow::draw();
        SceneExportWindow::draw();
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

        const auto filepath =
            Dialogs::showOpenFileDialog("Open BESS Project File", "*.bproj|");

        if (filepath == "" || !std::filesystem::exists(filepath)) {
            BESS_WARN("No or invalid file path selcted");
            return;
        }

        if (!m_pageState->getCurrentProjectFile()->isSaved()) {
            state._internalData.openFileClicked = true;
            state._internalData.path = filepath;
            ImGui::OpenPopup(Popups::PopupIds::unsavedProjectWarning.c_str());
        } else {
            m_pageState->loadProject(filepath);
        }
    }

    void UIMain::onSaveProject() {
        m_pageState->getCurrentProjectFile()->save();
    }

    void UIMain::onExportSceneView() {
        auto path = UI::Dialogs::showSelectPathDialog("Save To");
        if (path == "")
            return;
        BESS_TRACE("[ExportSceneView] Saving to {}", path);
        Canvas::Scene::instance()->saveScenePNG(path);
    }

} // namespace Bess::UI
