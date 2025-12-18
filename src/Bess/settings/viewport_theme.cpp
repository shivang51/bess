#include "settings/viewport_theme.h"
#include "imgui.h"
#include <cstdint>

namespace Bess {
    SceneColors ViewportTheme::colors;
    SchematicViewColors ViewportTheme::schematicViewColors;

    std::unordered_map<std::string, glm::vec4> ViewportTheme::s_compHeaderColorMap;

    void ViewportTheme::updateColorsFromImGuiStyle() {
        ImGuiStyle &style = ImGui::GetStyle();
        const ImVec4 *imguiColors = style.Colors;

        // 1. Core Background
        // Professional viewports are often slightly darker/more neutral than the UI panels
        const ImVec4 windowBg = imguiColors[ImGuiCol_WindowBg];
        colors.background = glm::vec4(windowBg.x * 0.85f, windowBg.y * 0.85f, windowBg.z * 0.85f, 1.0f);

        // 2. Component/Node Body
        // Match the ImGui Frame color for consistency
        const ImVec4 frameBg = imguiColors[ImGuiCol_FrameBg];
        colors.componentBG = glm::vec4(frameBg.x, frameBg.y, frameBg.z, 0.95f);

        // 3. Borders & Headers
        // Use the ImGui Border color for a clean, non-distracting look
        const ImVec4 borderCol = imguiColors[ImGuiCol_Border];
        colors.componentBorder = glm::vec4(borderCol.x, borderCol.y, borderCol.z, 0.8f);

        // Blender-style "Header" (use the Header color from the UI theme)
        const ImVec4 headerCol = imguiColors[ImGuiCol_Header];
        colors.compHeader = glm::vec4(headerCol.x, headerCol.y, headerCol.z, 1.0f);

        // 4. Text
        const ImVec4 textCol = imguiColors[ImGuiCol_Text];
        colors.text = glm::vec4(textCol.x, textCol.y, textCol.z, textCol.w);

        // 5. Logic States & Connections
        // Digital Logic colors: High = Vibrant Green/Cyan, Low = Muted/Dark
        colors.stateHigh = glm::vec4(0.35f, 0.85f, 0.35f, 1.00f);
        colors.stateLow = glm::vec4(0.15f, 0.25f, 0.15f, 1.00f);

        // 6. Wires & Selection
        colors.wire = colors.componentBorder;
        colors.ghostWire = glm::vec4(textCol.x, textCol.y, textCol.z, 0.3f);

        // The "Blender Orange" for selection
        colors.selectedWire = glm::vec4(1.0f, 0.60f, 0.0f, 1.0f);
        colors.selectedComp = colors.selectedWire;

        // Clock signals - a distinct vibrant blue/magenta
        colors.clockConnectionHigh = glm::vec4(0.30f, 0.70f, 1.0f, 1.0f);
        colors.clockConnectionLow = glm::vec4(0.10f, 0.20f, 0.4f, 1.0f);

        // 7. Selection Box (Dashed or semi-transparent)
        colors.selectionBoxBorder = colors.selectedWire;
        colors.selectionBoxFill = glm::vec4(1.0f, 0.60f, 0.0f, 0.08f); // Very faint orange tint

        // 8. Grid System
        {
            const glm::vec4 base = colors.background;
            // Subtle additive grid (Blender uses very faint lines)
            float gridAlpha = 0.15f;

            colors.gridMinorColor = glm::vec4(base.r + 0.04f, base.g + 0.04f, base.b + 0.04f, gridAlpha);
            colors.gridMajorColor = glm::vec4(base.r + 0.08f, base.g + 0.08f, base.b + 0.08f, gridAlpha * 2.0f);

            // Axis colors (standard RGB for XYZ, but muted for 2D)
            colors.gridAxisXColor = glm::vec4(0.8f, 0.3f, 0.3f, 0.4f); // Muted Red
            colors.gridAxisYColor = glm::vec4(0.3f, 0.8f, 0.3f, 0.4f); // Muted Green
        }

        initCompColorMap();
    }

    glm::vec4 ViewportTheme::getCompHeaderColor(const std::string &group) {
        if (!s_compHeaderColorMap.contains(group))
            return colors.compHeader;
        return s_compHeaderColorMap.at(group);
    }

    void ViewportTheme::initCompColorMap() {
        // We use slightly desaturated "Pastel-Pro" colors.
        // They are bright enough to categorize but muted enough to not be distracting.

        const glm::vec4 logicColor = glm::vec4(0.26f, 0.39f, 0.55f, 0.90f);          // Blender Blue (Input/Logic)
        const glm::vec4 memoryColor = glm::vec4(0.48f, 0.35f, 0.58f, 0.90f);         // Muted Amethyst (State/Memory)
        const glm::vec4 arithmeticColor = glm::vec4(0.32f, 0.56f, 0.32f, 0.90f);     // Sage Green (Math/Numbers)
        const glm::vec4 routingColor = glm::vec4(0.72f, 0.45f, 0.25f, 0.90f);        // Ochre/Orange (Routing/Data Flow)
        const glm::vec4 encoderDecoderColor = glm::vec4(0.65f, 0.30f, 0.30f, 0.90f); // Muted Terracotta (Conversion)
        const glm::vec4 combinationalColor = glm::vec4(0.25f, 0.55f, 0.55f, 0.90f);  // Deep Cyan (Process)
        const glm::vec4 ioColor = glm::vec4(0.45f, 0.45f, 0.45f, 0.90f);             // Graphite (Hardware I/O)
        const glm::vec4 specialColor = glm::vec4(0.35f, 0.35f, 0.35f, 0.85f);        // Dark Grey

        s_compHeaderColorMap.clear();

        // Logic & Basic Gates
        s_compHeaderColorMap["Digital Gates"] = logicColor;
        s_compHeaderColorMap["IO"] = ioColor;

        // Sequential / State
        s_compHeaderColorMap["Flip Flop"] = memoryColor;
        s_compHeaderColorMap["Registers/Memory"] = memoryColor; // Added for future use

        // Math & Computation
        s_compHeaderColorMap["Adders/Subtractors"] = arithmeticColor;

        // Data Routing
        s_compHeaderColorMap["Multiplexers/Demultiplexers"] = routingColor;
        s_compHeaderColorMap["Encoders/Decoders"] = encoderDecoderColor;

        // Mixed/Combinational
        s_compHeaderColorMap["Combinational Circuits"] = combinationalColor;

        // System
        s_compHeaderColorMap["Miscellaneous"] = specialColor;
    }
} // namespace Bess
