#include "common/theme.h"
#include "imgui.h"

namespace Bess {
    glm::vec3 Theme::backgroundColor = { 0.1f, 0.1f, 0.1f };
    glm::vec3 Theme::componentBGColor = { .22f, .22f, 0.22f };
    glm::vec4 Theme::componentBorderColor = { 0.42f, 0.42f, 0.42f, 1.f };
    glm::vec4 Theme::stateHighColor = { 0.42f, 0.72f, 0.42f, 1.f };
    glm::vec4 Theme::wireColor = { 200.f / 255.f, 200.f / 255.f, 200.f / 255.f, 1.f };
    glm::vec3 Theme::selectedWireColor = { 236.f / 255.f, 110.f / 255.f,
                                          34.f / 255.f };
    glm::vec3 Theme::compHeaderColor = { 81.f / 255.f, 74.f / 255.f,
                                        74.f / 255.f };
    glm::vec3 Theme::selectedCompColor = { 200.f / 255.f, 200.f / 255.f,
                                                200.f / 255.f };
    void Theme::updateColorsFromImGuiStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // Background color of the viewport (matches general dark theme)
        ImVec4 windowBg = colors[ImGuiCol_WindowBg];
        float darkenFactor = 0.75f; // Adjust this value to make it darker (0.85 makes it 15% darker)
        backgroundColor = glm::vec3(windowBg.x * darkenFactor, windowBg.y * darkenFactor, windowBg.z * darkenFactor);


        // Component background color (neutral gray)
        componentBGColor = glm::vec3(colors[ImGuiCol_FrameBg].x, colors[ImGuiCol_FrameBg].y, colors[ImGuiCol_FrameBg].z);

        // Component border color (soft gray)
        componentBorderColor = glm::vec4(colors[ImGuiCol_Border].x, colors[ImGuiCol_Border].y, colors[ImGuiCol_Border].z, colors[ImGuiCol_Border].w);

        // High state color (green to represent "on" or active state)
        stateHighColor = glm::vec4(0.42f, 0.72f, 0.42f, 1.00f); // Bright green

        // Wire color (neutral light gray for clarity)
        wireColor = glm::vec4(colors[ImGuiCol_Text].x, colors[ImGuiCol_Text].y, colors[ImGuiCol_Text].z, colors[ImGuiCol_Text].w);

        // Selected wire color (bright orange for visibility)
        selectedWireColor = glm::vec3(1.0f, 0.64f, 0.0f); // Orange

        // Component header color (blue to denote section headers)
        compHeaderColor = glm::vec3(colors[ImGuiCol_Header].x, colors[ImGuiCol_Header].y, colors[ImGuiCol_Header].z);

        // Selected component color (bright gray to stand out against the background)
        selectedCompColor = glm::vec3(colors[ImGuiCol_Text].x, colors[ImGuiCol_Text].y, colors[ImGuiCol_Text].z);
    }
} // namespace Bess
