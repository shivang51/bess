#include "ui/ui_main/ui_main.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene_driver.h"
#include "bess_core/settings/viewport_theme.h"
#include "common/logger.h"
#include "debug_panel.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/main_page.h"
#include "simulation_engine.h"
#include "stb_image_write.h"
#include "bess_core/ui/icons/cod_icons.h"
#include "ui/ui_main/log_window.h"
#include "ui/ui_main/scene_export_window.h"
#include "ui/widgets/m_widgets.h"
#include <string>

#include "pages/main_page/main_page_state.h"
#include "ui/dock_ids.h"
#include "bess_core/ui/icons/font_awesome_icons.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/dialogs.h"
#include "ui/ui_main/graph_view_window.h"
#include "ui/ui_main/popups.h"
#include "ui/ui_main/project_explorer.h"
#include "ui/ui_main/project_settings_window.h"
#include "ui/ui_main/properties_panel.h"
#include "ui/ui_main/scene_viewport_panel.h"
#include "ui/ui_main/settings_window/settings_window.h"
#include <filesystem>
#include <vector>

namespace Bess::UI {
    static constexpr ImGuiWindowFlags NO_MOVE_FLAGS =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoDecoration;

    namespace {
        struct VerilogImportWizardState {
            bool open = false;
            bool requestOpenPopup = false;
            std::string filePath;
            std::vector<std::string> filePaths;
            std::string stageMessage = "Select Verilog files";
            float progress = 0.f;
            bool importing = false;
            bool finished = false;
            bool failed = false;
        };

        VerilogImportWizardState &getVerilogImportWizardState() {
            static VerilogImportWizardState state;
            return state;
        }

        void resetVerilogImportWizard(VerilogImportWizardState &state) {
            state.filePath.clear();
            state.filePaths.clear();
            state.importing = false;
            state.finished = false;
            state.failed = false;
            state.progress = 0.f;
            state.stageMessage = "Select Verilog files";
        }

        std::vector<std::string>
        selectedVerilogPaths(const VerilogImportWizardState &state) {
            if (!state.filePaths.empty()) {
                return state.filePaths;
            }
            if (!state.filePath.empty()) {
                return {state.filePath};
            }
            return {};
        }

        std::string
        importSelectionLabel(const std::vector<std::string> &paths) {
            if (paths.empty()) {
                return {};
            }

            auto primary =
                std::filesystem::path(paths.front()).filename().string();
            if (paths.size() == 1) {
                return primary;
            }

            return std::format("{} (+{} more)", primary, paths.size() - 1);
        }

        bool hasSupportedVerilogExtension(const std::filesystem::path &path) {
            const auto extension = path.extension().string();
            return extension == ".v" || extension == ".sv" ||
                   extension == ".vh" || extension == ".svh";
        }

        std::weak_ptr<SceneViewportPanel> &hoveredSceneViewportPanelRef() {
            static std::weak_ptr<SceneViewportPanel> panel;
            return panel;
        }

        std::weak_ptr<SceneViewportPanel> &focusedSceneViewportPanelRef() {
            static std::weak_ptr<SceneViewportPanel> panel;
            return panel;
        }

        std::weak_ptr<SceneViewportPanel> &activeSceneViewportPanelRef() {
            static std::weak_ptr<SceneViewportPanel> panel;
            return panel;
        }

        std::weak_ptr<SceneViewportPanel> &targetSceneViewportPanelRef() {
            static std::weak_ptr<SceneViewportPanel> panel;
            return panel;
        }

        std::shared_ptr<SceneDriver> currentSceneDriver() {
            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
            return projectCtx ? projectCtx->getSubSystem<SceneDriver>()
                              : nullptr;
        }

        bool
        sceneBelongsToDriver(const std::shared_ptr<SceneDriver> &sceneDriver,
                             const std::shared_ptr<Canvas::Scene> &scene) {
            return sceneDriver && scene &&
                   sceneDriver->getSceneWithId(scene->getSceneId()) == scene;
        }

        bool isUsableSceneViewportPanel(
            const std::shared_ptr<SceneViewportPanel> &panel) {
            return panel && panel->getVisible() &&
                   sceneBelongsToDriver(currentSceneDriver(),
                                        panel->getAttachedScene());
        }

        std::shared_ptr<SceneViewportPanel>
        validSceneViewportPanel(std::weak_ptr<SceneViewportPanel> &ref) {
            auto panel = ref.lock();
            if (isUsableSceneViewportPanel(panel)) {
                return panel;
            }

            ref.reset();
            return nullptr;
        }

        std::shared_ptr<SceneViewportPanel> firstUsableSceneViewportPanel() {
            for (const auto &panel : UIMain::getScenePanels()) {
                if (isUsableSceneViewportPanel(panel)) {
                    return panel;
                }
            }
            return nullptr;
        }

        std::shared_ptr<Canvas::Scene> attachedSceneForPanel(
            const std::shared_ptr<SceneViewportPanel> &panel) {
            return panel ? panel->getAttachedScene() : nullptr;
        }
    } // namespace

    bool UIMain::m_isDockSpaceDirty = true;
    bool UIMain::m_preferRetainedViewports = true;

    void UIMain::draw() {
        if (m_isDockSpaceDirty) {
            resetDockspace();
        }

        for (auto &panel : getPanels()) {
            if (panel->getVisible()) {
                panel->render();
            }
        }
        updateSceneViewportTargets();

        drawMenubar();
        drawStatusbar();
        drawVerilogImportWizard();

        auto &pageState = Pages::MainPage::getInstance()->getState();
        if (pageState.actionFlags.saveProject) {
            onSaveProject();
            pageState.actionFlags.saveProject = false;
        } else if (pageState.actionFlags.openProject) {
            onOpenProject();
            pageState.actionFlags.openProject = false;
        }

        Popups::showAboutPopup();

        Popups::PopupRes res = Popups::handleUnsavedProjectWarning();
        if (res != Popups::PopupRes::none) {
            if (res == Popups::PopupRes::yes) {
                pageState.saveCurrentProject();
                if (!pageState.getCurrentProjectFile()->isSaved()) {
                    getState()._internalData.newFileClicked = false;
                    getState()._internalData.openFileClicked = false;
                    return;
                }
            }

            if (res != Popups::PopupRes::cancel) {
                if (getState()._internalData.newFileClicked) {
                    pageState.createNewProject();
                    refreshSceneViewportAttachments();
                    getState()._internalData.newFileClicked = false;
                } else if (getState()._internalData.openFileClicked) {
                    pageState.loadProject(getState()._internalData.path);
                    refreshSceneViewportAttachments();
                    getState()._internalData.statusMessage = std::format(
                        "Opened project: {}",
                        std::filesystem::path(getState()._internalData.path)
                            .filename()
                            .string());
                    getState()._internalData.path = "";
                    getState()._internalData.openFileClicked = false;
                }
            }
        }

        // ImGui::Begin("Scene State JSON");
        // static std::string sceneJson;
        // if (ImGui::Button("Refresh")) {
        //     Json::Value j;
        //     JsonConvert::toJsonValue(Canvas::Scene::instance()->getState(),
        //     j); sceneJson = j.toStyledString();
        // }
        // ImGui::TextWrapped("%s", sceneJson.data());
        // ImGui::End();
    }

    ImVec2 getTextSize(const std::string &text,
                       const bool includePadding = true) {
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
        const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar |
                                              ImGuiWindowFlags_NoSavedSettings |
                                              ImGuiWindowFlags_MenuBar;
        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();
        const float height = ImGui::GetFrameHeight();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if (ImGui::BeginViewportSideBar("##MainStatusBar",
                                        viewport,
                                        ImGuiDir_Down,
                                        height,
                                        window_flags)) {

            const auto &simCtx = simEngine.getRunCtx();

            if (ImGui::BeginMenuBar()) {
                if (simCtx.isSimulating()) {
                    ImGui::Text("Simulation Running");
                } else if (simCtx.isPaused()) {
                    ImGui::Text("Simulation Paused");
                } else if (simCtx.isStopped()) {
                    ImGui::Text("Simulation Stopped");
                } else {
                    ImGui::Text("Unknown State");
                }

                if (simCtx.isTimedRun) {
                    ImGui::SameLine();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

                    ImGui::SameLine();
                    ImGui::TextDisabled("Elapsed Time:");
                    ImGui::SameLine();
                    const auto sec =
                        std::chrono::duration_cast<std::chrono::seconds>(
                            simCtx.elapsedTime);
                    ImGui::Text("%s s", std::format("{:%T}", sec).c_str());
                }

                if (!getState()._internalData.statusMessage.empty()) {
                    const auto msg = std::format(
                        "{}\t", getState()._internalData.statusMessage);
                    const auto size = ImGui::CalcTextSize(msg.c_str());
                    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x -
                                    size.x);
                    ImGui::TextDisabled("%s", msg.c_str());
                }

                ImGui::EndMenuBar();
            }
            ImGui::End();
        }
        ImGui::PopStyleVar();
    }

    void UIMain::drawMenubar() {
        bool newFileClicked = false, aboutClicked = false;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 6.f));
        ImGui::BeginMainMenuBar();
        const float menuBarHeight = ImGui::GetFrameHeight();

        auto &pageState = Pages::MainPage::getInstance()->getState();
        const auto applyHierarchicalLayout = [&]() {
            const auto result =
                pageState.applyHierarchicalLayoutToActiveScene();
            if (!result.applied) {
                if (result.laidOutNodes == 0) {
                    getState()._internalData.statusMessage =
                        "Hierarchical layout skipped: no scene components";
                } else if (result.uniqueEdges == 0) {
                    getState()._internalData.statusMessage =
                        "Hierarchical layout skipped: no signal graph to rank";
                } else {
                    getState()._internalData.statusMessage =
                        "Hierarchical layout skipped";
                }
                return;
            }

            getState()._internalData.statusMessage =
                std::format("Applied hierarchical layout to {} components",
                            result.laidOutNodes);
        };

        if (ImGui::BeginMenu("File")) {
            // New File
            if (ImGui::MenuItemEx(
                    "New", Icons::FontAwesomeIcons::FA_FILE, "Ctrl+N")) {
                newFileClicked = true;
            };

            // Open File
            if (ImGui::MenuItemEx("Open",
                                  Icons::FontAwesomeIcons::FA_FOLDER_OPEN,
                                  "Ctrl+O")) {
                pageState.actionFlags.openProject = true;
            };

            // Save File
            if (ImGui::MenuItemEx("Save",
                                  Icons::FontAwesomeIcons::FA_FLOPPY_DISK,
                                  "Ctrl+S")) {
                pageState.actionFlags.saveProject = true;
            };

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::BeginMenuEx("Export",
                                   Icons::FontAwesomeIcons::FA_FILE_EXPORT)) {
                if (ImGui::MenuItemEx("Scene View PNG",
                                      Icons::FontAwesomeIcons::FA_FILE_IMAGE)) {
                    getPanel<SceneExportWindow>()->show();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenuEx("Import",
                                   Icons::FontAwesomeIcons::FA_FILE_IMPORT)) {
                const std::string verilogLabel =
                    std::string(Icons::FontAwesomeIcons::FA_V) +
                    Icons::FontAwesomeIcons::FA_S + "  Verilog Script";
                if (ImGui::MenuItemEx(
                        "Verilog Script",
                        (std::string(Icons::FontAwesomeIcons::FA_V) +
                         Icons::FontAwesomeIcons::FA_S)
                            .c_str())) {
                    auto &wizard = getVerilogImportWizardState();
                    wizard.requestOpenPopup = true;
                }
                ImGui::EndMenu();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Exit
            if (ImGui::MenuItem("Quit", "")) {
                const auto &appCtx = Bess::GAppContext::getInstance();
                appCtx.getSubSystem<Window>()->close();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            auto &mainPageState = Pages::MainPage::getInstance()->getState();
            auto &cmdSystem = mainPageState.getCommandSystem();

            if (ImGui::MenuItemEx("Undo",
                                  Icons::CodIcons::DISCARD,
                                  "Ctrl+Z",
                                  false,
                                  cmdSystem.canUndo())) {
                cmdSystem.undo();
            }

            if (ImGui::MenuItemEx("Redo",
                                  Icons::CodIcons::REDO,
                                  "Ctrl+Shift+Z",
                                  false,
                                  cmdSystem.canRedo())) {
                cmdSystem.redo();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::BeginMenuEx("Layout",
                                   Icons::FontAwesomeIcons::FA_ALIGN_LEFT)) {
                if (ImGui::MenuItemEx("Hierarchical Layout",
                                      Icons::CodIcons::LAYOUT)) {
                    applyHierarchicalLayout();
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::MenuItemEx("Project Settings",
                                  Icons::FontAwesomeIcons::FA_PENCIL,
                                  "Ctrl+P")) {
                getPanel<ProjectSettingsWindow>()->show();
            }

            if (ImGui::MenuItemEx("Preferences",
                                  Icons::FontAwesomeIcons::FA_GEAR,
                                  "Ctrl+S")) {
                getPanel<SettingsWindow>()->show();
            }

            ImGui::EndMenu();
        }

        ImGui::SetNextWindowSize(ImVec2(300, 0));
        if (ImGui::BeginMenu("View")) {

            for (auto &panel : getPanels()) {
                if (panel->getShowInMenuBar()) {
                    Widgets::CheckboxWithLabel(panel->getName().c_str(),
                                               &panel->getVisible());
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                aboutClicked = true;
            }
            ImGui::EndMenu();
        }

        const auto menubar_size = ImGui::GetWindowSize();

        // project name textbox - begin

        const auto &style = ImGui::GetStyle();
        auto &name = Pages::MainPage::getInstance()
                         ->getState()
                         .getCurrentProjectFile()
                         ->getNameRef();
        const auto fontSize = ImGui::CalcTextSize(name.c_str());
        auto width = fontSize.x + (style.FramePadding.x * 2);
        if (width < 150)
            width = 150;
        else if (width > 200)
            width = 200;

        ImGui::PushItemWidth(width);
        ImGui::SameLine((menubar_size.x / 2.f) -
                        (width / 2.f)); // Align to the right side
        ImGui::SetCursorPosY(((menubar_size.y - ImGui::GetFontSize()) / 2.f) -
                             2.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 2.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,
                              style.Colors[ImGuiCol_WindowBg]);
        Widgets::TextBox("", name, "Project Name");
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        getState()._internalData.isTbFocused = ImGui::IsItemFocused();

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();
        const auto &simCtx = simEngine.getRunCtx();
        // project name textbox - end

        // right aligned controls
        size_t buttonCount = 2; // Layout + Start/Stop

        if (!simCtx.isStopped()) {
            buttonCount += 1; // Pause/Resume
        }

        if (simCtx.isPaused()) {
            buttonCount += 1; // Step when paused
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

        const float targetHeight = menubar_size.y - 4.0f;
        const ImVec2 buttonSize =
            ImVec2(targetHeight - 2.f, targetHeight - 2.f);
        const float winSizeX =
            (buttonCount * (buttonSize.x + style.ItemSpacing.x));
        const float winX = menubar_size.x - winSizeX - 4.f;
        auto window = ImGui::GetCurrentWindow();
        window->DrawList->AddRectFilled(
            ImVec2(winX, 2.f),
            ImVec2(winX + winSizeX - 4.f, menubar_size.y - 2.f),
            ImGui::GetColorU32(ImGuiCol_WindowBg),
            4.f);
        ImGui::SameLine(winX - 8.f);
        ImGui::SetCursorPosY(((targetHeight - buttonSize.y) * 0.5f) + 2.f);

        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0, 0, 0, 0});

            if (ImGui::Button(Icons::CodIcons::LAYOUT, buttonSize)) {
                applyHierarchicalLayout();
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s", "Apply Hierarchical Layout");
            }

            ImGui::SameLine();
            ImGui::SetCursorPosY(((targetHeight - buttonSize.y) * 0.5f) + 2.f);

            // Play / Pause
            {

                if (simCtx.isStopped()) {
                    ImGui::PushStyleColor(
                        ImGuiCol_Text,
                        ViewportTheme::colors.stateHigh.toHexRev());
                } else if (simCtx.isSimulating() || simCtx.isPaused()) {
                    ImGui::PushStyleColor(
                        ImGuiCol_Text, ViewportTheme::colors.error.toHexRev());
                }

                auto icon = simCtx.isStopped()
                                ? Icons::FontAwesomeIcons::FA_PLAY
                                : Icons::FontAwesomeIcons::FA_STOP;

                if (ImGui::Button(icon, buttonSize)) {
                    simEngine.toggleStartStop();
                }

                ImGui::PopStyleColor();

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    const auto msg = simCtx.isStopped() ? "Start Simulation"
                                                        : "Stop Simulation";
                    ImGui::SetTooltip("%s", msg);
                }

                if (!simCtx.isStopped()) {
                    icon = simCtx.isPaused()
                               ? Icons::CodIcons::DEBUG_START
                               : Icons::FontAwesomeIcons::FA_PAUSE;

                    ImGui::SameLine();
                    ImGui::SetCursorPosY(
                        ((targetHeight - buttonSize.y) * 0.5f) + 2.f);

                    ImGui::PushStyleColor(
                        ImGuiCol_Text,
                        ViewportTheme::colors.stateHighZ.toHexRev());
                    if (ImGui::Button(icon, buttonSize)) {
                        simEngine.togglePlayPause();
                    }
                    ImGui::PopStyleColor();

                    if (ImGui::IsItemHovered(
                            ImGuiHoveredFlags_AllowWhenDisabled)) {
                        const auto msg = simCtx.isPaused() ? "Resume Simulation"
                                                           : "Pause Simulation";
                        ImGui::SetTooltip("%s", msg);
                    }
                }
            }

            if (simCtx.isPaused()) {
                ImGui::SameLine();
                ImGui::SetCursorPosY(((targetHeight - buttonSize.y) * 0.5f) +
                                     2.f);

                // Step when paused
                {
                    ImGui::BeginDisabled(!simCtx.isPaused());

                    if (ImGui::Button(Icons::CodIcons::DEBUG_STEP_OVER,
                                      buttonSize)) {
                        simEngine.stepSimulation();
                    }

                    if (ImGui::IsItemHovered(
                            ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip("%s", "Step");
                    }
                    ImGui::EndDisabled();
                }
            }

            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar(2);

        ImGui::EndMainMenuBar();
        ImGui::PopStyleVar(2);

        if (newFileClicked) {
            onNewProject();
        }

        if (aboutClicked) {
            ImGui::OpenPopup(Popups::PopupIds::about);
        }
    }

    void UIMain::drawVerilogImportWizard() {
        auto &wizard = getVerilogImportWizardState();
        auto &pageState = Pages::MainPage::getInstance()->getState();

        if (wizard.requestOpenPopup) {
            wizard.requestOpenPopup = false;
            wizard.open = true;
            resetVerilogImportWizard(wizard);
            ImGui::OpenPopup("Import Verilog");
        }

        if (!wizard.open) {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(520.f, 0.f), ImGuiCond_FirstUseEver);
        if (!ImGui::BeginPopupModal(
                "Import Verilog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            return;
        }

        if (wizard.importing) {
            std::string errorMessage;
            const auto status = pageState.advanceVerilogImport(&errorMessage);
            wizard.progress = status.progress;
            wizard.stageMessage = status.stageMessage;
            wizard.importing = status.importing;
            wizard.finished = status.finished;
            wizard.failed = status.failed;
            getState()._internalData.statusMessage = status.stageMessage;

            if (status.finished) {
                if (status.failed) {
                    getState()._internalData.statusMessage =
                        status.stageMessage;
                } else {
                    const auto paths = selectedVerilogPaths(wizard);
                    getState()._internalData.statusMessage = std::format(
                        "Imported Verilog: {}", importSelectionLabel(paths));
                    refreshSceneViewportAttachments();
                    wizard.open = false;
                    resetVerilogImportWizard(wizard);
                    pageState.cancelVerilogImport();
                    ImGui::CloseCurrentPopup();
                }
            }
        }

        ImGui::TextUnformatted("Files");
        if (Widgets::TextBox("##VerilogImportPath",
                             wizard.filePath,
                             "Select Verilog files")) {
            wizard.filePaths.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse") && !wizard.importing) {
            const auto paths =
                Dialogs::showOpenFilesDialog("Import Verilog Files",
                                             {"Verilog Source Files",
                                              "*.sv *.v *.svh *.vh",
                                              "All Files",
                                              "*.*"});
            if (!paths.empty()) {
                wizard.filePaths = paths;
                wizard.filePath = importSelectionLabel(paths);
            }
        }

        if (ImGui::IsItemHovered() && !wizard.filePaths.empty()) {
            ImGui::BeginTooltip();
            for (const auto &path : wizard.filePaths) {
                ImGui::TextUnformatted(path.c_str());
            }
            ImGui::EndTooltip();
        }

        ImGui::Spacing();
        ImGui::TextWrapped("%s", wizard.stageMessage.c_str());
        ImGui::ProgressBar(wizard.progress, ImVec2(420.f, 0.f));

        ImGui::Spacing();
        const auto selectedPaths = selectedVerilogPaths(wizard);

        ImGui::BeginDisabled(wizard.importing || selectedPaths.empty());
        if (ImGui::Button("Import", ImVec2(120.f, 0.f))) {
            bool hasUnsupportedFiles = false;
            for (const auto &path : selectedPaths) {
                if (!hasSupportedVerilogExtension(path)) {
                    hasUnsupportedFiles = true;
                    break;
                }
            }

            if (hasUnsupportedFiles) {
                wizard.importing = false;
                wizard.finished = true;
                wizard.failed = true;
                wizard.progress = 1.f;
                wizard.stageMessage =
                    "Import failed: choose only .v, .sv, .vh, or .svh files";
                getState()._internalData.statusMessage = wizard.stageMessage;
            } else {
                pageState.startVerilogImport(selectedPaths);
                wizard.importing = true;
                wizard.finished = false;
                wizard.failed = false;
                wizard.progress = 0.05f;
                wizard.stageMessage = "Clearing current project";
                getState()._internalData.statusMessage = wizard.stageMessage;
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        const char *closeLabel = wizard.importing ? "Close Disabled" : "Close";
        ImGui::BeginDisabled(wizard.importing);
        if (ImGui::Button(closeLabel, ImVec2(120.f, 0.f))) {
            wizard.open = false;
            resetVerilogImportWizard(wizard);
            pageState.cancelVerilogImport();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::EndPopup();
    }

    void UIMain::resetDockspace() {
        static std::unordered_map<Dock, ImGuiID> DockIds{
            {Dock::left, 0},
            {Dock::right, 0},
            {Dock::top, 0},
            {Dock::bottom, 0},
        };

        auto mainDockspaceId = ImGui::GetID("MainDockspace");

        ImGui::DockBuilderRemoveNode(mainDockspaceId);
        ImGui::DockBuilderAddNode(mainDockspaceId, ImGuiDockNodeFlags_NoTabBar);

        const auto dockIdLeft = ImGui::DockBuilderSplitNode(
            mainDockspaceId, ImGuiDir_Left, 0.15f, nullptr, &mainDockspaceId);

        const auto dockIdRight = ImGui::DockBuilderSplitNode(
            mainDockspaceId, ImGuiDir_Right, 0.15f, nullptr, &mainDockspaceId);

        const auto dockIdTop = ImGui::DockBuilderSplitNode(
            mainDockspaceId, ImGuiDir_Up, 0.75f, nullptr, &mainDockspaceId);

        const auto dockIdBot = ImGui::DockBuilderSplitNode(
            mainDockspaceId, ImGuiDir_Down, 0.25f, nullptr, &mainDockspaceId);

        DockIds[Dock::left] = dockIdLeft;
        DockIds[Dock::right] = dockIdRight;
        DockIds[Dock::bottom] = dockIdBot;
        DockIds[Dock::top] = dockIdTop;

        ImGui::DockBuilderDockWindow("MainViewport", mainDockspaceId);

        for (auto &panel : getPanels()) {
            if (panel->getDefaultDock() == Dock::none)
                continue;

            ImGui::DockBuilderDockWindow(panel->getName().c_str(),
                                         DockIds[panel->getDefaultDock()]);
        }

        for (const auto &[panelName, dock] : getExtPanelsDockMap()) {
            if (dock == Dock::none)
                continue;

            ImGui::DockBuilderDockWindow(panelName.c_str(), DockIds[dock]);
        }

        ImGui::DockBuilderFinish(mainDockspaceId);
        m_isDockSpaceDirty = false;
    }

    void UIMain::onNewProject() {
        auto &pageState = Pages::MainPage::getInstance()->getState();
        if (!pageState.getCurrentProjectFile()->isSaved()) {
            getState()._internalData.newFileClicked = true;
            ImGui::OpenPopup(Popups::PopupIds::unsavedProjectWarning);
        } else {
            pageState.createNewProject();
            refreshSceneViewportAttachments();
        }
    }

    void UIMain::onOpenProject() {
        const auto filepath = Dialogs::showOpenFileDialog(
            "Open BESS Project File",
            {"Bess Project", "*.bproj", "All Files", "*.*"});

        if (filepath == "" || !std::filesystem::exists(filepath)) {
            BESS_WARN("No or invalid file path selcted");
            getState()._internalData.statusMessage =
                "No or invalid file path selected";
            return;
        }

        auto &pageState = Pages::MainPage::getInstance()->getState();
        if (false && !pageState.getCurrentProjectFile()->isSaved()) {
            getState()._internalData.openFileClicked = true;
            getState()._internalData.path = filepath;
            ImGui::OpenPopup(Popups::PopupIds::unsavedProjectWarning);
        } else {
            pageState.loadProject(filepath);
            refreshSceneViewportAttachments();
            getState()._internalData.statusMessage =
                std::format("Project loaded from {}", filepath);
        }
    }

    void UIMain::onSaveProject() {
        auto &pageState = Pages::MainPage::getInstance()->getState();
        pageState.getCurrentProjectFile()->save();
        const auto &path = pageState.getCurrentProjectFile()->getPath();
        if (path.empty()) {
            getState()._internalData.statusMessage = "No save path selected.";
            return;
        } else {
            getState()._internalData.statusMessage =
                std::format("Project saved to {}", path);
        }
    }

    void UIMain::destroy() {
        for (auto &panel : getPanels()) {
            panel->hide();
            panel->destroy();
        }

        clearSceneViewportTargets();
        getScenePanels().clear();
        getPanelMap().clear();
        getPanels().clear();
        getPreInitCallbacks().clear();

        getState() = UIState{};
    }

    void UIMain::init() {
        preInit();

        for (auto &panel : getPanels()) {
            panel->init();
        }
    }
    void UIMain::onPreInit(const PreInitCallback &callback) {
        getPreInitCallbacks().push_back(callback);
    }

    void UIMain::preInit() {
        for (const auto &callback : getPreInitCallbacks()) {
            callback();
        }

        registerPanel<DebugPanel>();
        registerPanel<ComponentExplorer>();
        registerPanel<GraphViewWindow>();
        registerPanel<LogWindow>();
        registerPanel<ProjectExplorer>();
        registerPanel<PropertiesPanel>();
        registerPanel<ProjectSettingsWindow>();
        registerPanel<SceneExportWindow>();
        registerPanel<SettingsWindow>();
        // Legacy ImGui viewports remain registered for API compatibility, but
        // stay hidden while the retained MainUIView SceneView owns the scene.
        registerPanel<SceneViewportPanel>("Scene Viewport");
        registerPanel<SceneViewportPanel>("Scene Viewport 2");
        if (m_preferRetainedViewports) {
            for (const auto &panel : getScenePanels()) {
                if (panel) {
                    panel->hide();
                    panel->setShowInMenuBar(false);
                }
            }
        }
    }

    std::vector<std::shared_ptr<Panel>> &UIMain::getPanels() {
        static std::vector<std::shared_ptr<Panel>> m_panels;
        return m_panels;
    }

    std::vector<PreInitCallback> &UIMain::getPreInitCallbacks() {
        static std::vector<PreInitCallback> s_preInitCallbacks;
        return s_preInitCallbacks;
    }

    std::unordered_map<std::string, Dock> &UIMain::getExtPanelsDockMap() {
        static std::unordered_map<std::string, Dock> m_extPanelsDockMap;
        return m_extPanelsDockMap;
    }

    std::unordered_map<std::type_index, std::shared_ptr<Panel>> &
    UIMain::getPanelMap() {
        static std::unordered_map<std::type_index, std::shared_ptr<Panel>>
            m_panelMap;
        return m_panelMap;
    }

    std::vector<std::shared_ptr<SceneViewportPanel>> &UIMain::getScenePanels() {
        static std::vector<std::shared_ptr<SceneViewportPanel>> m_scenePanels;
        return m_scenePanels;
    }

    std::shared_ptr<SceneViewportPanel> UIMain::getHoveredSceneViewportPanel() {
        return validSceneViewportPanel(hoveredSceneViewportPanelRef());
    }

    std::shared_ptr<SceneViewportPanel> UIMain::getFocusedSceneViewportPanel() {
        return validSceneViewportPanel(focusedSceneViewportPanelRef());
    }

    std::shared_ptr<SceneViewportPanel> UIMain::getActiveSceneViewportPanel() {
        auto panel = validSceneViewportPanel(activeSceneViewportPanelRef());
        return panel ? panel : getTargetSceneViewportPanel();
    }

    std::shared_ptr<SceneViewportPanel> UIMain::getTargetSceneViewportPanel() {
        if (auto panel =
                validSceneViewportPanel(targetSceneViewportPanelRef())) {
            return panel;
        }

        if (auto panel =
                validSceneViewportPanel(activeSceneViewportPanelRef())) {
            targetSceneViewportPanelRef() = panel;
            return panel;
        }

        auto panel = firstUsableSceneViewportPanel();
        if (panel) {
            targetSceneViewportPanelRef() = panel;
            activeSceneViewportPanelRef() = panel;
        }
        return panel;
    }

    void UIMain::refreshSceneViewportAttachments() {
        const auto sceneDriver = currentSceneDriver();
        const auto activeScene =
            sceneDriver ? sceneDriver->getActiveScene() : nullptr;

        if (!activeScene) {
            clearSceneViewportTargets();
            return;
        }

        // Retained controllers own the live viewport while ImGui panels are
        // suppressed. Keep their scene attachment synchronized with project
        // open/new flows.
        for (const auto &weak : getSceneViewportControllers()) {
            auto controller = weak.lock();
            if (!controller) {
                continue;
            }
            if (!sceneBelongsToDriver(sceneDriver,
                                      controller->getAttachedScene())) {
                controller->setAttachedScene(activeScene);
            }
        }

        auto preferredPanel = activeSceneViewportPanelRef().lock();
        if (!preferredPanel || !preferredPanel->getVisible()) {
            preferredPanel = targetSceneViewportPanelRef().lock();
        }
        if (!preferredPanel || !preferredPanel->getVisible()) {
            preferredPanel = focusedSceneViewportPanelRef().lock();
        }
        if (!preferredPanel || !preferredPanel->getVisible()) {
            preferredPanel = hoveredSceneViewportPanelRef().lock();
        }

        for (const auto &panel : getScenePanels()) {
            if (!panel) {
                continue;
            }

            if (!preferredPanel && panel->getVisible()) {
                preferredPanel = panel;
            }

            if (!sceneBelongsToDriver(sceneDriver, panel->getAttachedScene())) {
                panel->setAttachedScene(activeScene);
            }
        }

        if (!preferredPanel) {
            preferredPanel = firstUsableSceneViewportPanel();
        }

        if (preferredPanel) {
            activeSceneViewportPanelRef() = preferredPanel;
            targetSceneViewportPanelRef() = preferredPanel;
        } else if (!getActiveSceneViewportController()) {
            clearSceneViewportTargets();
        }
    }

    void UIMain::setTargetSceneViewportPanel(
        const std::shared_ptr<SceneViewportPanel> &panel) {
        if (!isUsableSceneViewportPanel(panel)) {
            return;
        }

        targetSceneViewportPanelRef() = panel;
        activeSceneViewportPanelRef() = panel;
    }

    std::vector<std::weak_ptr<SceneViewportController>> &
    UIMain::getSceneViewportControllers() {
        static std::vector<std::weak_ptr<SceneViewportController>> controllers;
        return controllers;
    }

    void UIMain::registerSceneViewportController(
        const std::shared_ptr<SceneViewportController> &controller) {
        if (!controller) {
            return;
        }
        auto &controllers = getSceneViewportControllers();
        for (const auto &weak : controllers) {
            if (weak.lock() == controller) {
                return;
            }
        }
        controllers.push_back(controller);
    }

    void UIMain::unregisterSceneViewportController(
        const SceneViewportController *controller) {
        auto &controllers = getSceneViewportControllers();
        std::erase_if(controllers, [controller](const auto &weak) {
            const auto locked = weak.lock();
            return !locked || locked.get() == controller;
        });
    }

    std::shared_ptr<SceneViewportController>
    UIMain::getHoveredSceneViewportController() {
        for (const auto &weak : getSceneViewportControllers()) {
            auto controller = weak.lock();
            if (controller && controller->isHovered()) {
                return controller;
            }
        }
        return nullptr;
    }

    std::shared_ptr<SceneViewportController>
    UIMain::getFocusedSceneViewportController() {
        for (const auto &weak : getSceneViewportControllers()) {
            auto controller = weak.lock();
            if (controller && controller->isFocused()) {
                return controller;
            }
        }
        return getHoveredSceneViewportController();
    }

    std::shared_ptr<SceneViewportController>
    UIMain::getActiveSceneViewportController() {
        if (auto controller = getFocusedSceneViewportController()) {
            return controller;
        }
        for (const auto &weak : getSceneViewportControllers()) {
            auto controller = weak.lock();
            if (controller && controller->isUsable()) {
                return controller;
            }
        }
        for (const auto &weak : getSceneViewportControllers()) {
            if (auto controller = weak.lock()) {
                return controller;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Canvas::Scene> UIMain::getHoveredViewportScene() {
        if (const auto controller = getHoveredSceneViewportController()) {
            return controller->getAttachedScene();
        }
        return attachedSceneForPanel(getHoveredSceneViewportPanel());
    }

    std::shared_ptr<Canvas::Scene> UIMain::getFocusedViewportScene() {
        if (const auto controller = getFocusedSceneViewportController()) {
            return controller->getAttachedScene();
        }
        return attachedSceneForPanel(getFocusedSceneViewportPanel());
    }

    std::shared_ptr<Canvas::Scene> UIMain::getActiveViewportScene() {
        if (const auto controller = getActiveSceneViewportController()) {
            return controller->getAttachedScene();
        }
        return attachedSceneForPanel(getActiveSceneViewportPanel());
    }

    std::shared_ptr<Canvas::Scene> UIMain::getTargetViewportScene() {
        if (const auto controller = getActiveSceneViewportController()) {
            return controller->getAttachedScene();
        }
        return attachedSceneForPanel(getTargetSceneViewportPanel());
    }

    std::shared_ptr<Core::Viewport::ViewportContext>
    UIMain::getActiveViewportContext() {
        if (const auto controller = getActiveSceneViewportController()) {
            return controller->getViewportContext();
        }
        if (const auto panel = getActiveSceneViewportPanel()) {
            return panel->getViewportContext();
        }
        return nullptr;
    }

    void UIMain::updateSceneViewportTargets() {
        std::shared_ptr<SceneViewportPanel> hoveredPanel = nullptr;
        std::shared_ptr<SceneViewportPanel> focusedPanel = nullptr;

        for (const auto &panel : getScenePanels()) {
            if (!isUsableSceneViewportPanel(panel)) {
                continue;
            }

            if (!hoveredPanel && panel->isHovered()) {
                hoveredPanel = panel;
            }
            if (!focusedPanel && panel->isFocused()) {
                focusedPanel = panel;
            }
        }

        hoveredSceneViewportPanelRef() = hoveredPanel;
        focusedSceneViewportPanelRef() = focusedPanel;

        if (focusedPanel) {
            activeSceneViewportPanelRef() = focusedPanel;
            targetSceneViewportPanelRef() = focusedPanel;
            return;
        }

        if (hoveredPanel) {
            activeSceneViewportPanelRef() = hoveredPanel;
            targetSceneViewportPanelRef() = hoveredPanel;
            return;
        }

        if (validSceneViewportPanel(targetSceneViewportPanelRef())) {
            return;
        }

        auto fallbackPanel =
            validSceneViewportPanel(activeSceneViewportPanelRef());
        if (!fallbackPanel) {
            fallbackPanel = firstUsableSceneViewportPanel();
        }

        if (fallbackPanel) {
            activeSceneViewportPanelRef() = fallbackPanel;
            targetSceneViewportPanelRef() = fallbackPanel;
        }
    }

    void UIMain::clearSceneViewportTargets() {
        hoveredSceneViewportPanelRef().reset();
        focusedSceneViewportPanelRef().reset();
        activeSceneViewportPanelRef().reset();
        targetSceneViewportPanelRef().reset();
    }

    void UIMain::regExtPanelDock(const std::string &panelName,
                                 const Dock &dock) {
        auto &map = getExtPanelsDockMap();

        if (map.contains(panelName) && map[panelName] == dock) {
            return;
        }

        getExtPanelsDockMap()[panelName] = dock;
        m_isDockSpaceDirty = true;
    }

    void UIMain::update(TimeMs ts) {
        for (auto &panel : getPanels()) {
            if (panel->getVisible()) {
                panel->update(ts);
            }
        }
        updateSceneViewportTargets();
    }

    UIState &UIMain::getState() {
        static UIState state{};
        return state;
    }
} // namespace Bess::UI
