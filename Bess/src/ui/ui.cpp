#include "ui/ui.h"

#include "glad/glad.h"
#include "gtc/type_ptr.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "GLFW/glfw3.h"
#include <string>

#include "components_manager/components_manager.h"
#include "components_manager/component_bank.h"
#include "ui/component_explorer.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/icons/MaterialIcons.h"
#include "ui/dialogs.h"
#include "ui/popups.h"

#include "camera.h"

#include "common/theme.h"

namespace Bess::UI {
UIState UIMain::state{};

void UIMain::init(GLFWwindow *window) {

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable

    io.IniFilename = "bess.ini";

    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    float fontSize = 16.0f * 1.2f;
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Bold.ttf", fontSize);
    io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Regular.ttf", fontSize);

    ImFontConfig config;
    config.MergeMode = true;
    //static const ImWchar mat_icon_ranges[] = { Icons::MaterialIcons::ICON_MIN_MD, Icons::MaterialIcons::ICON_MAX_MD, 0 };
    //io.Fonts->AddFontFromFileTTF("assets/icons/MaterialIcons-Regular.ttf", 16.0f, &config, mat_icon_ranges);

    const ImWchar fa_icon_ranges[] = { Icons::FontAwesomeIcons::SIZE_MIN_FAB, Icons::FontAwesomeIcons::SIZE_MAX_FAB, 0 };
    io.Fonts->AddFontFromFileTTF("assets/icons/fa-brands-400.ttf", 16.0f, &config, fa_icon_ranges);

    static const ImWchar fa_icon_ranges_r[] = { Icons::FontAwesomeIcons::SIZE_MIN_FA, Icons::FontAwesomeIcons::SIZE_MAX_FA, 0 };
    io.Fonts->AddFontFromFileTTF("assets/icons/fa-solid-900.ttf", 16.0f, &config, fa_icon_ranges_r);

    //setDarkThemeColors();
    //setModernColors();
    //setMaterialYouColors();
    setCatpuccinMochaColors();
    //setBessDarkThemeColors();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");
}

void UIMain::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UIMain::draw() {
    begin();
    drawMenubar();
    drawProjectExplorer();
    drawViewport();
    ComponentExplorer::draw();
    drawPropertiesPanel();
    //ImGui::ShowDemoWindow();
    end();
}

void UIMain::setViewportTexture(GLuint64 texture) {
    state.viewportTexture = texture;
}

void UIMain::drawPropertiesPanel() {
    ImGui::Begin("Properties");
    ImGui::Text("Hovered Id: %d", ApplicationState::hoveredId);
    if (ApplicationState::getSelectedId() !=
        Simulator::ComponentsManager::emptyId) {
        auto &selectedEnt = Simulator::ComponentsManager::components
            [ApplicationState::getSelectedId()];
        ImGui::Text("Position: %f, %f, %f", selectedEnt->getPosition().x,
                    selectedEnt->getPosition().y, selectedEnt->getPosition().z);
    }
    ImGui::End();
}

void UIMain::drawProjectExplorer() {
    ImGui::Begin("Project Explorer");

    std::string temp = Icons::FontAwesomeIcons::FA_SAVE;
    temp += " Save";

    if (ImGui::Button(temp.c_str())) {
        ApplicationState::currentProject->update(Simulator::ComponentsManager::components);
        ApplicationState::currentProject->save();
    }

    for (auto &id : Simulator::ComponentsManager::renderComponenets) {
        auto& entity = Simulator::ComponentsManager::components[id];
        if (ImGui::Selectable(entity->getRenderName().c_str(),
                              entity->getId() ==
                                  ApplicationState::getSelectedId())) {
            ApplicationState::setSelectedId(entity->getId());
        }
    }
    ImGui::End();
}

void UIMain::drawMenubar()
{
    bool newFileClicked = false, openFileClicked = false;
    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("File"))
    {
        // New File
        std::string temp_name = Icons::FontAwesomeIcons::FA_FOLDER_PLUS;
        temp_name += " New";
        if (ImGui::MenuItem(temp_name.c_str()))
        {
            newFileClicked = true;
        };

        // Open File
        temp_name = Icons::FontAwesomeIcons::FA_FOLDER_OPEN;
        temp_name += " Open";
        if (ImGui::MenuItem(temp_name.c_str()))
        {
            openFileClicked = true;
        };
        ImGui::EndMenu();

    }

    auto menubar_size = ImGui::GetWindowSize();
    ImGui::EndMainMenuBar();


    if (newFileClicked) {
        onNewProject();
    }
    else if (openFileClicked) {
        onOpenProject();
    }

    Popups::PopupRes res;
    if ((res = Popups::handleUnsavedProjectWarning()) != Popups::PopupRes::none) {
        if(res == Popups::PopupRes::yes) {
            ApplicationState::saveCurrentProject();
            if (!ApplicationState::currentProject->isSaved()) {
                state._internalData.newFileClicked = false;
                state._internalData.openFileClicked = false;
                return;
            }
        }

        if (res != Popups::PopupRes::cancel) {
            if (state._internalData.newFileClicked) {
                ApplicationState::createNewProject();
                state._internalData.newFileClicked = false;
            }
            else if (state._internalData.openFileClicked) {
                ApplicationState::loadProject(state._internalData.path);
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
    ImGui::SetNextWindowSizeConstraints({400.f, -1.f}, {-1.f, -1.f});

    ImGui::Begin("Viewport", nullptr, flags);
    auto offset = ImGui::GetCursorPos();

    auto viewportPanelSize = ImGui::GetContentRegionAvail();
    state.viewportSize = {viewportPanelSize.x, viewportPanelSize.y};

    ImGui::Image((void *)state.viewportTexture,
                 ImVec2(viewportPanelSize.x, viewportPanelSize.y), ImVec2(0, 1),
                 ImVec2(1, 0));

    auto pos = ImGui::GetWindowPos();
    auto gPos = ImGui::GetMainViewport()->Pos;
    state.viewportPos = {pos.x - gPos.x + offset.x, pos.y - gPos.y + offset.y};

    ImGui::End();
    ImGui::PopStyleVar();

    // Camera controls
    {
        ImGui::SetNextWindowPos(
            { pos.x + viewportPanelSize.x - 208, pos.y + viewportPanelSize.y - 40 });
        ImGui::SetNextWindowSize({ 208, 0 });
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

        ImGui::Begin("Camera", nullptr, flags);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 8);
        if (ImGui::SliderFloat("Zoom", &state.cameraZoom, Camera::zoomMin, Camera::zoomMax, nullptr,
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

void UIMain::begin() {

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
    const ImGuiViewport *viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |=
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    static bool p_open = true;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    auto mainDockspaceId = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(mainDockspaceId);

    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        resetDockspace();
    }
}

void UIMain::resetDockspace() {
    auto mainDockspaceId = ImGui::GetID("MainDockspace");

    ImGui::DockBuilderRemoveNode(mainDockspaceId);
    ImGui::DockBuilderAddNode(mainDockspaceId, ImGuiDockNodeFlags_NoTabBar);

    auto dock_id_left = ImGui::DockBuilderSplitNode(
        mainDockspaceId, ImGuiDir_Left, 0.15f, nullptr, &mainDockspaceId);
    auto dock_id_right = ImGui::DockBuilderSplitNode(
        mainDockspaceId, ImGuiDir_Right, 0.15f, nullptr, &mainDockspaceId);

    auto dock_id_right_bot = ImGui::DockBuilderSplitNode(
        dock_id_right, ImGuiDir_Down, 0.15f, nullptr, &dock_id_right);

    ImGui::DockBuilderDockWindow("Component Explorer", dock_id_left);
    ImGui::DockBuilderDockWindow("Viewport", mainDockspaceId);
    ImGui::DockBuilderDockWindow("Project Explorer", dock_id_right);
    ImGui::DockBuilderDockWindow("Properties", dock_id_right_bot);

    ImGui::DockBuilderFinish(mainDockspaceId);
}

void UIMain::onNewProject()
{
    if (!ApplicationState::currentProject->isSaved()) {
        state._internalData.newFileClicked = true;
        ImGui::OpenPopup(Popups::PopupIds::unsavedProjectWarning.c_str());
    }
    else {
        ApplicationState::createNewProject();
    }
}

void UIMain::onOpenProject()
{

    auto filepath = Dialogs::showOpenFileDialog("Open BESS Project File", "*.bproj|");

    if (filepath == "" || !std::filesystem::exists(filepath)) return;

    if (!ApplicationState::currentProject->isSaved()) {
        state._internalData.openFileClicked = true;
        state._internalData.path = filepath;
        ImGui::OpenPopup(Popups::PopupIds::unsavedProjectWarning.c_str());
    }
    else {
        ApplicationState::loadProject(filepath);
    }
}

void UIMain::onSaveProject()
{
}

void UIMain::end() {
    ImGui::End();
    ImGuiIO &io = ImGui::GetIO();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow *backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}

void UIMain::setCatpuccinMochaColors() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Base colors inspired by Catppuccin Mocha
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.89f, 0.88f, 1.00f); // Latte
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.56f, 0.52f, 1.00f); // Surface2
    colors[ImGuiCol_WindowBg] = ImVec4(0.17f, 0.14f, 0.20f, 1.00f); // Base
    colors[ImGuiCol_ChildBg] = ImVec4(0.18f, 0.16f, 0.22f, 1.00f); // Mantle
    colors[ImGuiCol_PopupBg] = ImVec4(0.17f, 0.14f, 0.20f, 1.00f); // Base
    colors[ImGuiCol_Border] = ImVec4(0.27f, 0.23f, 0.29f, 1.00f); // Overlay0
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f); // Crust
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.20f, 0.29f, 1.00f); // Overlay1
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.22f, 0.31f, 1.00f); // Overlay2
    colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.12f, 0.18f, 1.00f); // Mantle
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.17f, 0.15f, 0.21f, 1.00f); // Mantle
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.12f, 0.18f, 1.00f); // Mantle
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.17f, 0.15f, 0.22f, 1.00f); // Base
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.17f, 0.14f, 0.20f, 1.00f); // Base
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f); // Crust
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.24f, 0.20f, 0.29f, 1.00f); // Overlay1
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.26f, 0.22f, 0.31f, 1.00f); // Overlay2
    colors[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.66f, 0.47f, 1.00f); // Peach
    colors[ImGuiCol_SliderGrab] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f); // Lavender
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f); // Pink
    colors[ImGuiCol_Button] = ImVec4(0.65f, 0.34f, 0.46f, 1.00f); // Maroon
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.71f, 0.40f, 0.52f, 1.00f); // Red
    colors[ImGuiCol_ButtonActive] = ImVec4(0.76f, 0.46f, 0.58f, 1.00f); // Pink
    colors[ImGuiCol_Header] = ImVec4(0.65f, 0.34f, 0.46f, 1.00f); // Maroon
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.71f, 0.40f, 0.52f, 1.00f); // Red
    colors[ImGuiCol_HeaderActive] = ImVec4(0.76f, 0.46f, 0.58f, 1.00f); // Pink
    colors[ImGuiCol_Separator] = ImVec4(0.27f, 0.23f, 0.29f, 1.00f); // Overlay0
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.95f, 0.66f, 0.47f, 1.00f); // Peach
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.95f, 0.66f, 0.47f, 1.00f); // Peach
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f); // Lavender
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f); // Pink
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.92f, 0.61f, 0.85f, 1.00f); // Mauve
    colors[ImGuiCol_Tab] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f); // Crust
    colors[ImGuiCol_TabHovered] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f); // Lavender
    colors[ImGuiCol_TabActive] = ImVec4(0.76f, 0.46f, 0.58f, 1.00f); // Pink
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.16f, 0.22f, 1.00f); // Mantle
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f); // Crust
    colors[ImGuiCol_DockingPreview] = ImVec4(0.95f, 0.66f, 0.47f, 0.70f); // Peach
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); // Base
    colors[ImGuiCol_PlotLines] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f); // Lavender
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f); // Pink
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f); // Lavender
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f); // Pink
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f); // Mantle
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.27f, 0.23f, 0.29f, 1.00f); // Overlay0
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f); // Surface2
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f); // Surface0
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.82f, 0.61f, 0.85f, 0.35f); // Lavender
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.95f, 0.66f, 0.47f, 0.90f); // Peach
    colors[ImGuiCol_NavHighlight] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f); // Lavender
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    // Style adjustments
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.ChildRounding = 4.0f;

    style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(5.0f, 4.0f);
    style.ItemSpacing = ImVec2(6.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.IndentSpacing = 22.0f;

    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;


    Theme::backgroundColor = glm::vec3({ colors[ImGuiCol_WindowBg].x - 0.1f, colors[ImGuiCol_WindowBg].y - 0.1f, colors[ImGuiCol_WindowBg].z - 0.1f });
}

void UIMain::setModernColors()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Base color scheme
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.11f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.20f, 0.21f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.28f, 0.28f, 0.29f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.33f, 0.34f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.41f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 0.80f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.30f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 0.80f);
    colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.33f, 0.67f, 1.00f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.33f, 0.67f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.38f, 0.48f, 0.69f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.38f, 0.59f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.28f, 0.56f, 1.00f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.28f, 0.56f, 1.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    // Style adjustments
    style.WindowRounding = 5.3f;
    style.FrameRounding = 2.3f;
    style.ScrollbarRounding = 0;

    style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(5.0f, 5.0f);
    style.ItemSpacing = ImVec2(6.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.IndentSpacing = 25.0f;
}

void UIMain::setMaterialYouColors()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Base colors inspired by Material You (dark mode)
    colors[ImGuiCol_Text] = ImVec4(0.93f, 0.93f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.45f, 0.76f, 0.29f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.29f, 0.66f, 0.91f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.47f, 0.91f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.52f, 0.91f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.47f, 0.91f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.29f, 0.66f, 0.91f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.29f, 0.66f, 0.91f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.29f, 0.66f, 0.91f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.29f, 0.70f, 0.91f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.47f, 0.91f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.47f, 0.91f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.29f, 0.62f, 0.91f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.29f, 0.66f, 0.91f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.29f, 0.62f, 0.91f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.29f, 0.62f, 0.91f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.29f, 0.62f, 0.91f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    // Style adjustments
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.ChildRounding = 4.0f;

    style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 22.0f;

    style.ScrollbarSize = 16.0f;
    style.GrabMinSize = 10.0f;

    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
}

void UIMain::setDarkThemeColors() {
    auto &colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg] = ImVec4{0.1f, 0.105f, 0.11f, 1.0f};

    // Headers
    colors[ImGuiCol_Header] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
    colors[ImGuiCol_HeaderHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
    colors[ImGuiCol_HeaderActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

    // Buttons
    colors[ImGuiCol_Button] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
    colors[ImGuiCol_ButtonHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
    colors[ImGuiCol_ButtonActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

    // Frame BG
    colors[ImGuiCol_FrameBg] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
    colors[ImGuiCol_FrameBgHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
    colors[ImGuiCol_FrameBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
    colors[ImGuiCol_TabHovered] = ImVec4{0.38f, 0.3805f, 0.381f, 1.0f};
    colors[ImGuiCol_TabActive] = ImVec4{0.28f, 0.2805f, 0.281f, 1.0f};
    colors[ImGuiCol_TabUnfocused] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};

    // Title
    colors[ImGuiCol_TitleBg] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
    colors[ImGuiCol_TitleBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
}

void UIMain::setBessDarkThemeColors()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Base colors for a pleasant and modern dark theme with dark accents
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.93f, 0.94f, 1.00f); // Light grey text for readability
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.54f, 1.00f); // Subtle grey for disabled text
    colors[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // Dark background with a hint of blue
    colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // Slightly lighter for child elements
    colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f); // Popup background
    colors[ImGuiCol_Border] = ImVec4(0.28f, 0.29f, 0.30f, 0.60f); // Soft border color
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f); // No border shadow
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Frame background
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.24f, 0.26f, 1.00f); // Frame hover effect
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // Active frame background
    colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // Title background
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // Active title background
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // Collapsed title background
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f); // Menu bar background
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // Scrollbar background
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // Dark accent for scrollbar grab
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.30f, 0.32f, 1.00f); // Scrollbar grab hover
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.32f, 0.34f, 0.36f, 1.00f); // Scrollbar grab active
    colors[ImGuiCol_CheckMark] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Dark blue checkmark
    colors[ImGuiCol_SliderGrab] = ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // Dark blue slider grab
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // Active slider grab
    colors[ImGuiCol_Button] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Dark blue button
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // Button hover effect
    colors[ImGuiCol_ButtonActive] = ImVec4(0.32f, 0.42f, 0.52f, 1.00f); // Active button
    colors[ImGuiCol_Header] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Header color similar to button
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // Header hover effect
    colors[ImGuiCol_HeaderActive] = ImVec4(0.32f, 0.42f, 0.52f, 1.00f); // Active header
    colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.29f, 0.30f, 1.00f); // Separator color
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Hover effect for separator
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Active separator
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // Resize grip
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // Hover effect for resize grip
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.44f, 0.54f, 0.64f, 1.00f); // Active resize grip
    colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Inactive tab
    colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // Hover effect for tab
    colors[ImGuiCol_TabActive] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Active tab color
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Unfocused tab
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Active but unfocused tab
    colors[ImGuiCol_DockingPreview] = ImVec4(0.24f, 0.34f, 0.44f, 0.70f); // Docking preview
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // Empty docking background
    colors[ImGuiCol_PlotLines] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Plot lines
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Hover effect for plot lines
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // Histogram color
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // Hover effect for histogram
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Table header background
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.28f, 0.29f, 0.30f, 1.00f); // Strong border for tables
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.24f, 0.25f, 0.26f, 1.00f); // Light border for tables
    colors[ImGuiCol_TableRowBg] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Table row background
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.22f, 0.24f, 0.26f, 1.00f); // Alternate row background
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.24f, 0.34f, 0.44f, 0.35f); // Selected text background
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.46f, 0.56f, 0.66f, 0.90f); // Drag and drop target
    colors[ImGuiCol_NavHighlight] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Navigation highlight
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f); // Windowing highlight
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f); // Dim background for windowing
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f); // Dim background for modal windows

    // Style adjustments
    style.WindowRounding = 8.0f;   // Softer rounded corners for windows
    style.FrameRounding = 4.0f;   // Rounded corners for frames
    style.ScrollbarRounding = 6.0f;   // Rounded corners for scrollbars
    style.GrabRounding = 4.0f;   // Rounded corners for grab elements
    style.ChildRounding = 4.0f;   // Rounded corners for child windows

    style.WindowTitleAlign = ImVec2(0.50f, 0.50f); // Centered window title
    style.WindowPadding = ImVec2(10.0f, 10.0f); // Comfortable padding
    style.FramePadding = ImVec2(6.0f, 4.0f);   // Frame padding
    style.ItemSpacing = ImVec2(8.0f, 8.0f);   // Item spacing
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);   // Inner item spacing
    style.IndentSpacing = 22.0f;                // Indentation spacing

    style.ScrollbarSize = 16.0f;  // Scrollbar size
    style.GrabMinSize = 10.0f;  // Minimum grab size

    style.AntiAliasedLines = true;   // Enable anti-aliased lines
    style.AntiAliasedFill = true;   // Enable anti-aliased fill

}

void UIMain::setCursorPointer() { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

void UIMain::setCursorReset() { ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow); }
} // namespace Bess
