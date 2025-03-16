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

        // Background: Use WindowBg directly
        ImVec4 windowBg = colors[ImGuiCol_WindowBg];
        backgroundColor = glm::vec4(windowBg.x, windowBg.y, windowBg.z, windowBg.w);

        // Component Background: Slightly lighter than WindowBg
        auto color = colors[ImGuiCol_WindowBg];
        componentBGColor = glm::vec4(color.x + 0.05f, color.y + 0.05f, color.z + 0.05f, 1.0f);
        componentBGColor = glm::clamp(componentBGColor, 0.0f, 1.0f); // Ensure values stay in [0, 1]

        // Component Border: Subtle darkening of Border
        color = colors[ImGuiCol_Border];
        float darkenFactor = 0.9f;
        componentBorderColor = glm::vec4(color.x * darkenFactor, color.y * darkenFactor, color.z * darkenFactor, 0.74f);

        // Wire Color: Bright but slightly softened Text color
        color = colors[ImGuiCol_Text];
        wireColor = glm::vec4(color.x * 0.9f, color.y * 0.9f, color.z * 0.9f, color.w);

        // Component Header: Slightly lighter than componentBGColor
        compHeaderColor = glm::vec4(componentBGColor.x + 0.03f, componentBGColor.y + 0.03f, componentBGColor.z + 0.03f, componentBGColor.w);
        compHeaderColor = glm::clamp(compHeaderColor, 0.0f, 1.0f);

        // Text: Full brightness for readability
        color = colors[ImGuiCol_Text];
        textColor = glm::vec4(color.x, color.y, color.z, color.w);

        // Grid: Subtle but visible with brighter textColor
        gridColor = textColor * 0.3f;

        // Fixed colors (unchanged except for correction)
        stateHighColor = glm::vec4(0.6f, 0.8f, 0.4f, 1.00f);         // Greenish
        stateLowColor = glm::vec4(0.82f, 0.2f, 0.2f, 1.00f);         // Reddish
        selectedWireColor = glm::vec4(1.0f, 0.64f, 0.0f, 1.0f);      // Orange
        selectionBoxBorderColor = glm::vec4(0.3f, 0.3f, 0.8f, 1.0f); // Blueish (corrected from 8.0)
        selectionBoxFillColor = glm::vec4(0.3f, 0.3f, 0.7f, 0.5f);   // Semi-transparent blue
        selectedCompColor = selectedWireColor;                       // Orange
    }
} // namespace Bess
