#include "settings/viewport_theme.h"
#include "imgui.h"

namespace Bess {
    glm::vec3 ViewportTheme::backgroundColor = {0.1f, 0.1f, 0.1f};
    glm::vec3 ViewportTheme::componentBGColor = {.22f, .22f, 0.22f};
    glm::vec4 ViewportTheme::componentBorderColor = {0.42f, 0.42f, 0.42f, 1.f};
    glm::vec4 ViewportTheme::stateHighColor = {0.42f, 0.72f, 0.42f, 1.f};
    glm::vec4 ViewportTheme::wireColor = {200.f / 255.f, 200.f / 255.f, 200.f / 255.f, 1.f};
    glm::vec3 ViewportTheme::selectedWireColor = {236.f / 255.f, 110.f / 255.f,
                                                  34.f / 255.f};
    glm::vec3 ViewportTheme::compHeaderColor = {81.f / 255.f, 74.f / 255.f,
                                                74.f / 255.f};
    glm::vec3 ViewportTheme::selectedCompColor = {200.f / 255.f, 200.f / 255.f,
                                                  200.f / 255.f};

    glm::vec4 ViewportTheme::textColor = {1.f, 1.f, 1.f, 1.f};

    void ViewportTheme::updateColorsFromImGuiStyle() {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *colors = style.Colors;

        ImVec4 windowBg = colors[ImGuiCol_WindowBg];
        float darkenFactor = 0.75f;
        backgroundColor = glm::vec3(windowBg.x * darkenFactor, windowBg.y * darkenFactor, windowBg.z * darkenFactor);

        componentBGColor = glm::vec3(colors[ImGuiCol_FrameBg].x, colors[ImGuiCol_FrameBg].y, colors[ImGuiCol_FrameBg].z);

        componentBorderColor = glm::vec4(colors[ImGuiCol_Border].x, colors[ImGuiCol_Border].y, colors[ImGuiCol_Border].z, colors[ImGuiCol_Border].w);

        stateHighColor = glm::vec4(0.42f, 0.72f, 0.42f, 1.00f);

        wireColor = glm::vec4(colors[ImGuiCol_Text].x, colors[ImGuiCol_Text].y, colors[ImGuiCol_Text].z, colors[ImGuiCol_Text].w);

        selectedWireColor = glm::vec3(1.0f, 0.64f, 0.0f);

        compHeaderColor = glm::vec3(colors[ImGuiCol_Header].x, colors[ImGuiCol_Header].y, colors[ImGuiCol_Header].z);

        selectedCompColor = glm::vec3(colors[ImGuiCol_Text].x, colors[ImGuiCol_Text].y, colors[ImGuiCol_Text].z);

        textColor = glm::vec4(colors[ImGuiCol_Text].x, colors[ImGuiCol_Text].y, colors[ImGuiCol_Text].z, colors[ImGuiCol_Text].w);
    }
} // namespace Bess
