#include "settings/viewport_theme.h"
#include "imgui.h"

namespace Bess {
    glm::vec4 ViewportTheme::backgroundColor;
    glm::vec4 ViewportTheme::componentBGColor;
    glm::vec4 ViewportTheme::componentBorderColor;
    glm::vec4 ViewportTheme::stateHighColor;
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

        auto color = colors[ImGuiCol_FrameBg];
        componentBGColor = glm::vec4(color.x, color.y, color.z, color.w);

        color = colors[ImGuiCol_Border];
        componentBorderColor = glm::vec4(color.x, color.y, color.z, color.w);

        color = colors[ImGuiCol_Text];
        wireColor = glm::vec4(color.x, color.y, color.z, color.w);

        color = colors[ImGuiCol_Header];
        compHeaderColor = glm::vec4(color.x, color.y, color.z, color.w);

        color = colors[ImGuiCol_Text];
        selectedCompColor = glm::vec4(color.x, color.y, color.z, color.w);

        color = colors[ImGuiCol_Text];
        textColor = glm::vec4(color.x, color.y, color.z, color.w);

        gridColor = textColor * 0.3f;

        stateHighColor = glm::vec4(0.42f, 0.72f, 0.42f, 1.00f);
        selectedWireColor = glm::vec4(1.0f, 0.64f, 0.0f, 1.0f);

        selectionBoxBorderColor = glm::vec4({0.0, 0.3, 1.0, 1.f});
        selectionBoxFillColor = glm::vec4({0.0, 0.3, .7, .5f});
    }
} // namespace Bess
