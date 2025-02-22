#include "settings/viewport_theme.h"
#include "imgui.h"

namespace Bess {
    glm::vec4 ViewportTheme::backgroundColor;
    glm::vec4 ViewportTheme::componentBGColor;
    glm::vec4 ViewportTheme::componentBorderColor;
    glm::vec4 ViewportTheme::stateHighColor;
    glm::vec4 ViewportTheme::stateLowColor;
    glm::vec4 ViewportTheme::wireColor;
    glm::vec4 ViewportTheme::selectedWireColor;
    glm::vec4 ViewportTheme::compHeaderColor;
    glm::vec4 ViewportTheme::selectedCompColor;
    glm::vec4 ViewportTheme::textColor;
    glm::vec4 ViewportTheme::gridColor;
    glm::vec4 ViewportTheme::selectionBoxBorderColor;
    glm::vec4 ViewportTheme::selectionBoxFillColor;

    void ViewportTheme::updateColorsFromImGuiStyle() {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *colors = style.Colors;

        ImVec4 windowBg = colors[ImGuiCol_WindowBg];
        float darkenFactor = 0.75f;
        backgroundColor = glm::vec4(windowBg.x * darkenFactor, windowBg.y * darkenFactor, windowBg.z * darkenFactor, windowBg.w);

        auto color = colors[ImGuiCol_WindowBg];
        componentBGColor = glm::vec4(color.x, color.y, color.z, 1.f);

        color = colors[ImGuiCol_Border];
        darkenFactor = 0.7f;
        componentBorderColor = glm::vec4(color.x * darkenFactor, color.y * darkenFactor, color.z * darkenFactor, 0.74f);

        color = colors[ImGuiCol_Text];
        wireColor = glm::vec4(color.x, color.y, color.z, color.w) * darkenFactor;
        wireColor.w = color.w;

        color = colors[ImGuiCol_WindowBg];
        glm::vec3 compHeaderTint = glm::vec3(1.1f, 1.1f, 0.7f);
        compHeaderColor = glm::vec4(glm::vec3(componentBGColor) * compHeaderTint, componentBGColor.w);
        compHeaderColor = componentBGColor;

        color = colors[ImGuiCol_Border];
        selectedCompColor = glm::vec4(color.x, color.y, color.z, 0.74f);

        color = colors[ImGuiCol_Text];
        textColor = glm::vec4(color.x, color.y, color.z, color.w) * 0.8f;
        textColor.w = color.w;

        gridColor = textColor * 0.3f;

        stateHighColor = glm::vec4(0.42f, 0.72f, 0.42f, 1.00f);
        stateLowColor = glm::vec4(0.82f, 0.2f, 0.2f, 1.00f);
        selectedWireColor = glm::vec4(1.0f, 0.64f, 0.0f, 1.0f);

        selectionBoxBorderColor = glm::vec4({0.3, 0.3, 8.0, 1.f});
        selectionBoxFillColor = glm::vec4({0.3, 0.3, .7, .5f});
    }
} // namespace Bess
