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

#include "camera.h"

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

    float fontSize = 16.0f; // *2.0f;
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Bold.ttf",
                                 fontSize);
    io.FontDefault = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/Roboto/Roboto-Regular.ttf", fontSize);

    setDarkThemeColors();

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
    if (ImGui::Button("Save")) {
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

void UIMain::setCursorPointer() { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

void UIMain::setCursorReset() { ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow); }
} // namespace Bess
