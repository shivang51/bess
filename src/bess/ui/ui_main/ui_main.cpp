#include "ui/ui_main/ui_main.h"
#include "application/application_state.h"
#include "common/logger.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/main_page.h"
#include "simulation_engine.h"
#include "stb_image_write.h"
#include "ui/icons/CodIcons.h"
#include "ui/ui_main/log_window.h"
#include "ui/ui_main/scene_export_window.h"
#include "ui/widgets/m_widgets.h"
#include <cstdint>
#include <string>

#include "pages/main_page/main_page_state.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/dialogs.h"
#include "ui/ui_main/graph_view_window.h"
#include "ui/ui_main/popups.h"
#include "ui/ui_main/project_explorer.h"
#include "ui/ui_main/project_settings_window.h"
#include "ui/ui_main/properties_panel.h"
#include "ui/ui_main/settings_window.h"
#include "ui/ui_main/truth_table_window.h"
#include <filesystem>

namespace Bess::UI {
    UIState UIMain::state{};
    Pages::MainPageState *UIMain::m_pageState;

    static constexpr ImGuiWindowFlags NO_MOVE_FLAGS =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoDecoration;

#ifdef DEBUG
    void drawDebugWindow() {

        auto &mainPageState = Pages::MainPage::getTypedInstance()->getState();
        auto &sceneState = mainPageState.getSceneDriver()->getState();

        ImGui::Begin("Debug Window");

        if (Widgets::TreeNode(0, "Selected components")) {
            const auto &selComps = sceneState.getSelectedComponents();
            ImGui::Indent();
            for (const auto &[compId, selected] : selComps) {
                ImGui::BulletText("%lu", (uint64_t)compId);
            }
            ImGui::Unindent();
            ImGui::TreePop();
        }

        if (Widgets::TreeNode(1, "Connections")) {
            ImGui::Indent();
            const auto &connections = sceneState.getAllComponentConnections();
            for (const auto &[compId, connIds] : connections) {

                ImGui::Text("Component %lu", (uint64_t)compId);
                ImGui::Indent();
                for (const auto &connId : connIds) {
                    ImGui::BulletText("%lu", (uint64_t)connId);
                }
                ImGui::Unindent();
            }
            ImGui::Unindent();
            ImGui::TreePop();
        }

        if (Widgets::TreeNode(2, "First Sim Component Serilaized")) {
            const auto &selComps = sceneState.getSelectedComponents();
            if (!selComps.empty()) {
                const auto &compId = selComps.begin()->first;
                const auto &comp = sceneState.getComponentByUuid(compId);

                const auto &j = comp->toJson();
                ImGui::TextWrapped("%s", j.toStyledString().c_str());
            }
            ImGui::TreePop();
        }

        ImGui::End();
    }
#endif

    void UIMain::draw() {
        static bool firstTime = true;
        if (firstTime) {
            firstTime = false;
            m_pageState = &Pages::MainPage::getTypedInstance()->getState();
            resetDockspace();
        }
        drawMenubar();
        drawViewport();
        drawStatusbar();
        drawExternalWindows();
#ifdef DEBUG
        drawDebugWindow();
#endif

        if (m_pageState->actionFlags.saveProject) {
            onSaveProject();
            m_pageState->actionFlags.saveProject = false;
        } else if (m_pageState->actionFlags.openProject) {
            onOpenProject();
            m_pageState->actionFlags.openProject = false;
        }

        Popups::PopupRes res = Popups::handleUnsavedProjectWarning();
        if (res != Popups::PopupRes::none) {
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
                    state._internalData.statusMessage = std::format("Opened project: {}",
                                                                    std::filesystem::path(state._internalData.path).filename().string());
                    state._internalData.path = "";
                    state._internalData.openFileClicked = false;
                }
            }
        }

        // ImGui::Begin("Scene State JSON");
        // static std::string sceneJson;
        // if (ImGui::Button("Refresh")) {
        //     Json::Value j;
        //     JsonConvert::toJsonValue(Canvas::Scene::instance()->getState(), j);
        //     sceneJson = j.toStyledString();
        // }
        // ImGui::TextWrapped("%s", sceneJson.data());
        // ImGui::End();
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
        ImGuiViewportP *viewport = (ImGuiViewportP *)ImGui::GetMainViewport();
        const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
        auto &simEngine = SimEngine::SimulationEngine::instance();
        const float height = ImGui::GetFrameHeight();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if (ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, height, window_flags)) {
            if (ImGui::BeginMenuBar()) {
                if (simEngine.getSimulationState() == SimEngine::SimulationState::running) {
                    ImGui::Text("Simulation Running");
                } else if (simEngine.getSimulationState() == SimEngine::SimulationState::paused) {
                    ImGui::Text("Simulation Paused");
                } else {
                    ImGui::Text("Unknown State");
                }

                if (!state._internalData.statusMessage.empty()) {
                    const auto msg = std::format("{}\t", state._internalData.statusMessage);
                    const auto size = ImGui::CalcTextSize(msg.c_str());
                    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - size.x);
                    ImGui::TextDisabled("%s", msg.c_str());
                }

                ImGui::EndMenuBar();
            }
            ImGui::End();
        }
        ImGui::PopStyleVar();
    }

    void UIMain::drawMenubar() {
        bool newFileClicked = false;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 6.f));
        ImGui::BeginMainMenuBar();
        const float menuBarHeight = ImGui::GetFrameHeight();

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
                m_pageState->actionFlags.openProject = true;
            };

            // Save File
            temp_name = Icons::FontAwesomeIcons::FA_SAVE;
            temp_name += "   Save";
            if (ImGui::MenuItem(temp_name.c_str(), "Ctrl+S")) {
                m_pageState->actionFlags.saveProject = true;
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
            auto &mainPageState = Pages::MainPage::getTypedInstance()->getState();
            auto &cmdSystem = mainPageState.getCommandSystem();

            std::string icon = Icons::FontAwesomeIcons::FA_UNDO;
            if (ImGui::MenuItem((icon + "  Undo").c_str(), "Ctrl+Z", false, cmdSystem.canUndo())) {
                cmdSystem.undo();
            }

            icon = Icons::FontAwesomeIcons::FA_REDO;
            if (ImGui::MenuItem((icon + "  Redo").c_str(), "Ctrl+Shift+Z", false, cmdSystem.canRedo())) {
                cmdSystem.redo();
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

            Widgets::CheckboxWithLabel(PropertiesPanel::windowName.data(), &PropertiesPanel::isShown);

            Widgets::CheckboxWithLabel(GraphViewWindow::windowName.data(), &GraphViewWindow::isShown);

            Widgets::CheckboxWithLabel(TruthTableWindow::windowName.data(), &TruthTableWindow::isShown);

            Widgets::CheckboxWithLabel(LogWindow::windowName.data(), &LogWindow::isShown);

            ImGui::EndMenu();
        }

        const auto menubar_size = ImGui::GetWindowSize();

        // project name textbox - begin

        const auto &style = ImGui::GetStyle();
        auto &name = Pages::MainPage::getTypedInstance()->getState().getCurrentProjectFile()->getNameRef();
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

        // right aligned controls
        constexpr size_t buttonCount = 2;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

        const float targetHeight = menubar_size.y - 4.0f;
        const ImVec2 buttonSize = ImVec2(targetHeight - 2.f, targetHeight - 2.f);
        const float winSizeX = (buttonCount * (buttonSize.x + style.ItemSpacing.x));
        const float winX = menubar_size.x - winSizeX - 4.f;
        auto window = ImGui::GetCurrentWindow();
        window->DrawList->AddRectFilled(ImVec2(winX, 2.f),
                                        ImVec2(winX + winSizeX - 4.f, menubar_size.y - 2.f),
                                        ImGui::GetColorU32(ImGuiCol_WindowBg), 4.f);
        ImGui::SameLine(winX - 8.f);
        ImGui::SetCursorPosY(((targetHeight - buttonSize.y) * 0.5f) + 2.f);

        {
            auto &simEngine = SimEngine::SimulationEngine::instance();
            const auto isSimPaused = simEngine.getSimulationState() == SimEngine::SimulationState::paused;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0, 0, 0, 0});

            // Play / Pause
            {

                const auto icon = isSimPaused
                                      ? Icons::CodIcons::DEBUG_START
                                      : Icons::CodIcons::DEBUG_PAUSE;

                if (isSimPaused) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, 0.667f, 0.f, 1.f});
                }

                if (ImGui::Button(icon, buttonSize)) {
                    simEngine.toggleSimState();
                }

                if (isSimPaused) {
                    ImGui::PopStyleColor();
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    const auto msg = isSimPaused ? "Resume Simulation" : "Pause Simulation";
                    ImGui::SetTooltip("%s", msg);
                }
            }

            ImGui::SameLine();
            ImGui::SetCursorPosY(((targetHeight - buttonSize.y) * 0.5f) + 2.f);

            // Step when paused
            {
                ImGui::BeginDisabled(!isSimPaused);

                if (ImGui::Button(Icons::CodIcons::DEBUG_STEP_OVER, buttonSize)) {
                    simEngine.stepSimulation();
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("%s", "Step");
                }
                ImGui::EndDisabled();
            }

            ImGui::PopStyleColor();
        }
        ImGui::PopStyleVar(2);

        ImGui::EndMainMenuBar();
        ImGui::PopStyleVar(2);

        if (newFileClicked) {
            onNewProject();
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

        const auto dockIdLeft = ImGui::DockBuilderSplitNode(mainDockspaceId, ImGuiDir_Left, 0.15f, nullptr, &mainDockspaceId);
        auto dockIdRight = ImGui::DockBuilderSplitNode(mainDockspaceId, ImGuiDir_Right, 0.25f, nullptr, &mainDockspaceId);
        const auto dockIdBot = ImGui::DockBuilderSplitNode(mainDockspaceId, ImGuiDir_Down, 0.25f, nullptr, &mainDockspaceId);

        const auto dockIdRightBot = ImGui::DockBuilderSplitNode(dockIdRight, ImGuiDir_Down, 0.35f, nullptr, &dockIdRight);

        ImGui::DockBuilderDockWindow("MainViewport", mainDockspaceId);
        ImGui::DockBuilderDockWindow(ProjectExplorer::windowName.data(), dockIdLeft);
        ImGui::DockBuilderDockWindow(PropertiesPanel::windowName.data(), dockIdRight);
        ImGui::DockBuilderDockWindow(GraphViewWindow::windowName.data(), dockIdBot);
        ImGui::DockBuilderDockWindow(TruthTableWindow::windowName.data(), dockIdBot);
        ImGui::DockBuilderDockWindow(LogWindow::windowName.data(), dockIdBot);
        ImGui::DockBuilderDockWindow("Debug Window", dockIdBot);

        ImGui::DockBuilderFinish(mainDockspaceId);
    }

    void UIMain::drawExternalWindows() {
        SettingsWindow::draw();
        ProjectSettingsWindow::draw();
        SceneExportWindow::draw();
        ComponentExplorer::draw();
        ProjectExplorer::draw();
        PropertiesPanel::draw();
        GraphViewWindow::draw();
        TruthTableWindow::draw();
        LogWindow::draw();
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
            state._internalData.statusMessage = "No or invalid file path selected";
            return;
        }

        if (!m_pageState->getCurrentProjectFile()->isSaved()) {
            state._internalData.openFileClicked = true;
            state._internalData.path = filepath;
            ImGui::OpenPopup(Popups::PopupIds::unsavedProjectWarning.c_str());
        } else {
            m_pageState->loadProject(filepath);
            state._internalData.statusMessage = std::format("Project loaded from {}", filepath);
        }
    }

    void UIMain::onSaveProject() {
        m_pageState->getCurrentProjectFile()->save();
        state._internalData.statusMessage = std::format("Project saved to {}",
                                                        m_pageState->getCurrentProjectFile()->getPath());
    }

} // namespace Bess::UI
