#include "ui/ui_main/ui_main.h"
#include "application_state.h"
#include "common/log.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/renderer/renderer.h"
#include "simulation_engine.h"
#include "stb_image_write.h"
#include "ui/m_widgets.h"
#include <string>

#include "camera.h"
#include "pages/main_page/main_page_state.h"
#include "scene/artist.h"
#include "scene/renderer/gl/gl_wrapper.h"
#include "scene/scene.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/dialogs.h"
#include "ui/ui_main/popups.h"
#include "ui/ui_main/project_explorer.h"
#include "ui/ui_main/project_settings_window.h"
#include "ui/ui_main/properties_panel.h"
#include "ui/ui_main/settings_window.h"
#include <filesystem>

namespace Bess::UI {
    UIState UIMain::state{};
    std::shared_ptr<Pages::MainPageState> UIMain::m_pageState;

    bool showExportWindow = false;

    void drawExportWindow() {
        if (!showExportWindow)
            return;
        ImGui::Begin("Export View", &showExportWindow);
        ImGui::Text("Export the scene as PNG");
        if (ImGui::Button("Start Export")) {
            const auto &reg = Canvas::Scene::instance().getEnttRegistry();
            const auto view = reg.view<Canvas::Components::TransformComponent>();

            glm::vec2 min, max;
            for (const auto &ent : view) {
                const auto &comp = view.get<Canvas::Components::TransformComponent>(ent);
                if (comp.position.x - comp.scale.x < min.x)
                    min.x = comp.position.x - comp.scale.x;
                if (comp.position.x + comp.scale.x > max.x)
                    max.x = comp.position.x + comp.scale.x;

                if (comp.position.y + comp.scale.y > max.y)
                    max.y = comp.position.y + comp.scale.y;
                if (comp.position.y - comp.scale.y < min.y)
                    min.y = comp.position.y - comp.scale.y;
            }

            auto size = Canvas::Scene::instance().getSize();
            std::shared_ptr<Camera> camera = std::make_shared<Camera>(size.x, size.y);
            std::vector<Gl::FBAttachmentType> attachments = {Gl::FBAttachmentType::RGBA_RGBA,
                                                             Gl::FBAttachmentType::R32I_REDI,
                                                             Gl::FBAttachmentType::RGBA_RGBA,
                                                             Gl::FBAttachmentType::DEPTH32F_STENCIL8};
            auto msaaFramebuffer = std::make_unique<Gl::FrameBuffer>(size.x, size.y, attachments, true);

            attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::R32I_REDI};
            auto normalFramebuffer = std::make_unique<Gl::FrameBuffer>(size.x, size.y, attachments);

            static constexpr int value = -1;

            const float zoom = 4.f;
            camera->setPos(min);
            camera->setZoom(zoom);
            auto span = camera->getSpan();

            glm::vec2 dist = max - min;

            glm::vec2 rem = {(int)dist.x % (int)span.x, (int)dist.y % (int)span.y};
            rem = span - rem;
            rem /= 2;
            min -= rem;
            max += rem;

            dist = max - min;

            glm::ivec2 snaps = glm::round(dist / span);

            std::vector<unsigned char> buffer(snaps.y * size.y * snaps.x * size.x * 4, 255);

            BESS_INFO("[ExportSceneView] Exporting between ({}, {}) and  ({}, {}) covering dist of ({}, {}) with sliding window of {}, {}",
                      min.x, min.y, max.x, max.y, dist.x, dist.y, span.x, span.y);
            BESS_INFO("[ExportSceneView] Snaps = {}, {} with size per snap {}, {}", snaps.x, snaps.y, size.x, size.y);
            BESS_INFO("[ExportSceneView] Generating image of size {}x{}", snaps.x * size.x, snaps.y * size.y);

            auto pos = min + span / 2.f;
            stbi_flip_vertically_on_write(1);

            for (int i = 0; i < snaps.y; i++) {
                pos.x = min.x + (span.x / 2.f);
                for (int j = 0; j < snaps.x; j++) {
                    camera->setPos(pos);

                    msaaFramebuffer->bind();
                    msaaFramebuffer->clearColorAttachment<GL_FLOAT>(0, glm::value_ptr(ViewportTheme::backgroundColor));
                    msaaFramebuffer->clearColorAttachment<GL_INT>(1, &value);
                    msaaFramebuffer->clearColorAttachment<GL_FLOAT>(2, glm::value_ptr(ViewportTheme::backgroundColor));

                    Gl::FrameBuffer::clearDepthStencilBuf();
                    Canvas::Scene::instance().drawScene(camera);

                    Gl::FrameBuffer::unbindAll();

                    for (int i_ = 0; i_ < 2; i_++) {
                        msaaFramebuffer->bindColorAttachmentForRead(i_);
                        normalFramebuffer->bindColorAttachmentForDraw(i_);
                        Gl::FrameBuffer::blitColorBuffer(size.x, size.y);
                    }
                    Gl::FrameBuffer::unbindAll();

                    pos.x += span.x;

                    auto data = normalFramebuffer->getPixelsFromColorAttachment(0);

                    const size_t sourceStride = size.x * 4;
                    const size_t destStride = snaps.x * size.x * 4;

                    for (int row = 0; row < size.y; row++) {
                        unsigned char *srcPtr = data.data() + row * sourceStride;
                        int destRow = (snaps.y - 1 - i) * size.y + row;
                        int destColOffset = j * size.x * 4;
                        unsigned char *destPtr = buffer.data() + destRow * destStride + destColOffset;
                        memcpy(destPtr, srcPtr, sourceStride);
                    }
                }
                pos.y += span.y;
            }

            std::string pathStr = "./save.png";
            int result = stbi_write_png(
                pathStr.c_str(),
                snaps.x * size.x,
                snaps.y * size.y,
                4,
                buffer.data(),
                snaps.x * size.x * 4);

            if (result == 0) {
                BESS_ERROR("[ExportSceneView] Failed to save file to {}", pathStr);
            } else {
                BESS_TRACE("[ExportSceneView] Successfully saved file to {}", pathStr);
            }
        }

        ImGui::End();
    }

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
        drawStatusbar();
        drawExternalWindows();
        drawExportWindow();
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

    ImVec2 getTextSize(const std::string &text, bool includePadding = true) {
        auto size = ImGui::CalcTextSize(text.c_str());
        if (!includePadding)
            return size;
        ImGuiContext &g = *ImGui::GetCurrentContext();
        auto style = g.Style;
        size.x += style.FramePadding.x * 2;
        size.y += style.FramePadding.y * 2;
        return size;
    }

    void UIMain::drawStatusbar() {
        ImGuiContext &g = *ImGui::GetCurrentContext();
        auto style = g.Style;
        ImGuiViewportP *viewport = (ImGuiViewportP *)(void *)ImGui::GetMainViewport();
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
        auto &simEngine = SimEngine::SimulationEngine::instance();
        float height = ImGui::GetFrameHeight();
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
                    // onExportSceneView();
                    showExportWindow = true;
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
            std::string icon = Icons::FontAwesomeIcons::FA_WRENCH;
            if (ImGui::MenuItem((icon + "  Project Settings").c_str(), "Ctrl+P")) {
                ProjectSettingsWindow::show();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Simulation")) {
            std::string text = m_pageState->isSimulationPaused() ? Icons::FontAwesomeIcons::FA_PLAY : Icons::FontAwesomeIcons::FA_PAUSE;
            text += m_pageState->isSimulationPaused() ? "  Play" : "  Pause";

            if (ImGui::MenuItem(text.c_str(), "Ctrl+Space")) {
                m_pageState->setSimulationPaused(!m_pageState->isSimulationPaused());
            }
            ImGui::EndMenu();
        }

        auto menubar_size = ImGui::GetWindowSize();

        // project name textbox - begin

        auto &style = ImGui::GetStyle();
        auto &name = Pages::MainPageState::getInstance()->getCurrentProjectFile()->getNameRef();
        auto fontSize = ImGui::CalcTextSize(name.c_str());
        auto width = fontSize.x + (style.FramePadding.x * 2);
        if (width < 150)
            width = 150;
        else if (width > 200)
            width = 200;

        ImGui::PushItemWidth(width);
        ImGui::SameLine(menubar_size.x / 2.f - width / 2.f); // Align to the right side
        ImGui::SetCursorPosY((menubar_size.y - ImGui::GetFontSize()) / 2.f - 2.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 2.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, style.Colors[ImGuiCol_WindowBg]);
        MWidgets::TextBox("", name, "Project Name");
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
        state.viewportPos = {pos.x - gPos.x + offset.x, pos.y - gPos.y + offset.y};

        if (!state._internalData.isTbFocused && ImGui::IsWindowHovered()) {
            ImGui::SetWindowFocus();
        }

        state.isViewportFocused = ImGui::IsWindowFocused();
        ImGui::End();
        ImGui::PopStyleVar();

        // top left actions
        {
            ImGuiContext &g = *ImGui::GetCurrentContext();
            auto colors = g.Style.Colors;
            auto &simEngine = SimEngine::SimulationEngine::instance();
            static float checkboxWidth = ImGui::CalcTextSize("W").x + g.Style.FramePadding.x + 2.f;
            static float size = ImGui::CalcTextSize("Schematic Mode").x + checkboxWidth + 12.f;
            ImGui::SetNextWindowPos({pos.x + g.Style.FramePadding.x, pos.y + g.Style.FramePadding.y});
            ImGui::SetNextWindowSize({size, 0});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

            auto col = colors[ImGuiCol_Header];
            col.w = 0.1f;
            ImGui::PushStyleColor(ImGuiCol_WindowBg, col);
            ImGui::Begin("TopLeftViewportActions", nullptr, flags);
            ImGui::Text("Schematic Mode");
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::Checkbox("##CheckBoxSchematicMode", Canvas::Artist::getSchematicModePtr());
            ImGui::PopStyleVar();
            state.isViewportFocused &= !ImGui::IsWindowHovered();
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(1);
        }

        // actions (on top right)
        {

            ImGuiContext &g = *ImGui::GetCurrentContext();
            auto colors = g.Style.Colors;
            auto &simEngine = SimEngine::SimulationEngine::instance();
            static int n = 4; // number of action buttons
            static float size = (32 * n) - (n - 1);
            ImGui::SetNextWindowPos(
                {pos.x + viewportPanelSize.x - size - g.Style.FramePadding.x, pos.y + g.Style.FramePadding.y});
            ImGui::SetNextWindowSize({size, 0});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

            auto col = colors[ImGuiCol_Header];
            col.w = 0.5;
            ImGui::PushStyleColor(ImGuiCol_WindowBg, col);
            ImGui::Begin("TopRightViewportActions", nullptr, flags);

            auto &scene = Canvas::Scene::instance();

            // scene modes
            {

                bool isGeneral = scene.getSceneMode() == Canvas::SceneMode::general;

                // general mode
                if (isGeneral) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, 0.667f, 0.f, 1.f});
                }

                auto icon = Icons::FontAwesomeIcons::FA_MOUSE_POINTER;
                if (ImGui::Button(icon)) {
                    scene.setSceneMode(Canvas::SceneMode::general);
                }
                if (isGeneral) {
                    ImGui::PopStyleColor();
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    auto msg = "General Mode";
                    ImGui::SetTooltip("%s", msg);
                }

                // move mode
                if (!isGeneral) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, 0.667f, 0.f, 1.f});
                }
                ImGui::SameLine();
                icon = Icons::FontAwesomeIcons::FA_ARROWS_ALT;
                if (ImGui::Button(icon)) {
                    scene.setSceneMode(Canvas::SceneMode::move);
                }

                if (!isGeneral) {
                    ImGui::PopStyleColor();
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    auto msg = "Move Mode";
                    ImGui::SetTooltip("%s", msg);
                }
            }

            auto isSimPaused = simEngine.getSimulationState() == SimEngine::SimulationState::paused;

            // Play / Pause
            {

                auto icon = Icons::FontAwesomeIcons::FA_PAUSE;
                if (isSimPaused) {
                    // #574735
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, 0.667f, 0.f, 1.f});
                    icon = Icons::FontAwesomeIcons::FA_PLAY;
                }

                ImGui::SameLine();
                if (ImGui::Button(icon)) {
                    simEngine.toggleSimState();
                }

                if (isSimPaused)
                    ImGui::PopStyleColor();

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    auto msg = isSimPaused ? "Resume Simulation" : "Pause Simulation";
                    ImGui::SetTooltip("%s", msg);
                }
            }

            ImGui::SameLine();

            // Step when paused
            {
                ImGui::BeginDisabled(!isSimPaused);

                if (ImGui::Button(Icons::MaterialIcons::REDO)) {
                    simEngine.stepSimulation();
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("%s", "Step");
                }
                ImGui::EndDisabled();
            }

            state.isViewportFocused &= !ImGui::IsWindowHovered();
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(1);
        }

        // Camera controls (on bottom right)
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
            auto camera = Canvas::Scene::instance().getCamera();
            if (ImGui::SliderFloat("Zoom", &camera->getZoomRef(), Camera::zoomMin,
                                   Camera::zoomMax, nullptr,
                                   ImGuiSliderFlags_AlwaysClamp)) {
                // step size logic
                float stepSize = 0.1f;
                float val = roundf(camera->getZoom() / stepSize) * stepSize;
                camera->setZoom(val);
            }
            state.isViewportFocused &= !ImGui::IsWindowHovered();
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

        ImGui::DockBuilderDockWindow(ComponentExplorer::windowName.c_str(), dock_id_left);
        ImGui::DockBuilderDockWindow("Viewport", mainDockspaceId);
        ImGui::DockBuilderDockWindow(ProjectExplorer::windowName.c_str(), dock_id_right);
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
        Canvas::Scene::instance().saveScenePNG(path);
    }

} // namespace Bess::UI
