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
        componentBGColor = glm::vec4(color.x + 0.05f, color.y + 0.05f, color.z + 0.05f, 0.8f);
        componentBGColor = glm::clamp(componentBGColor, 0.0f, 1.0f); // Ensure values stay in [0, 1]

        // Component Border: Subtle darkening of Border
        auto color_ = 1.f - componentBGColor;
        float darkenFactor = 0.5f;
        componentBorderColor = glm::vec4(color_.x * darkenFactor, color_.y * darkenFactor, color_.z * darkenFactor, 0.8f);


        // Component Header: Slightly lighter than componentBGColor
        compHeaderColor = glm::vec4(0.8f, 0.1f, 0.1f, 0.8f);

        // Text: Full brightness for readability
        color = colors[ImGuiCol_Text];
        darkenFactor = 1.f;
        textColor = glm::vec4(color.x * darkenFactor, color.y * darkenFactor, color.z * darkenFactor, color.w);

        // Grid: Subtle but visible with brighter textColor
        gridColor = textColor * 0.45f;

        // Fixed colors (unchanged except for correction)
        stateHighColor = glm::vec4(0.6f, 0.8f, 0.4f, 1.00f);         // Greenish
        stateLowColor = glm::vec4(0.2f, 0.3f, 0.1f, 1.00f);          // Dark Greenish
        selectedWireColor = glm::vec4(1.0f, 0.64f, 0.0f, 1.0f);      // Orange
        selectionBoxBorderColor = glm::vec4(0.3f, 0.3f, 0.8f, 1.0f); // Blueish (corrected from 8.0)
        selectionBoxFillColor = glm::vec4(0.3f, 0.3f, 0.7f, 0.5f);   // Semi-transparent blue
        selectedCompColor = selectedWireColor;                       // Orange
        wireColor = componentBorderColor;
    }
} // namespace Bess
