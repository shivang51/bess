#include "settings/viewport_theme.h"
#include "imgui.h"

namespace Bess {
    SceneColors ViewportTheme::colors;
    SchematicViewColors ViewportTheme::schematicViewColors;
    SceneWidgetsColors ViewportTheme::sceneWidgetsColors;
    NodeHeaderColors ViewportTheme::headerColors;

    namespace {
        Color toVec4(const ImVec4 &color, float alpha = -1.f) {
            return {color.x, color.y, color.z, alpha >= 0.f ? alpha : color.w};
        }

    } // namespace

    void ViewportTheme::updateColorsFromImGuiStyle() {
        ImGuiStyle &style = ImGui::GetStyle();
        const ImVec4 *imguiColors = style.Colors;

        const ImVec4 windowBg = imguiColors[ImGuiCol_WindowBg];
        colors.background = Color(
            windowBg.x * 0.85f, windowBg.y * 0.85f, windowBg.z * 0.85f, 1.0f);

        const ImVec4 frameBg = imguiColors[ImGuiCol_FrameBg];
        colors.componentBG = Color(frameBg.x, frameBg.y, frameBg.z, 0.95f);

        const ImVec4 borderCol = imguiColors[ImGuiCol_Border];
        colors.componentBorder =
            Color(borderCol.x, borderCol.y, borderCol.z, 0.8f);

        const ImVec4 headerCol = imguiColors[ImGuiCol_Header];
        colors.compHeader = Color(headerCol.x, headerCol.y, headerCol.z, 1.0f);

        const ImVec4 textCol = imguiColors[ImGuiCol_Text];
        colors.text = Color(textCol.x, textCol.y, textCol.z, textCol.w);

        colors.stateHigh = Color(0.35f, 0.85f, 0.35f, 1.00f);
        colors.stateLow = Color(0.15f, 0.25f, 0.15f, 1.00f);

        colors.wire = colors.componentBorder;
        colors.ghostWire = Color(textCol.x, textCol.y, textCol.z, 1.f);

        colors.selectedWire = Color(1.0f, 0.60f, 0.0f, 1.0f);
        colors.selectedComp = colors.selectedWire;

        colors.clockConnectionHigh = Color(0.30f, 0.70f, 1.0f, 1.0f);
        colors.clockConnectionLow = Color(0.10f, 0.20f, 0.4f, 1.0f);

        colors.selectionBoxBorder = colors.selectedWire;
        colors.selectionBoxFill =
            Color(1.0f, 0.60f, 0.0f, 0.08f); // Very faint orange tint

        {
            const Color base = colors.background;
            // subtle additive grid (uses very faint lines)
            float gridAlpha = 0.2f;

            colors.gridMinorColor = Color(
                base.r + 0.04f, base.g + 0.04f, base.b + 0.04f, gridAlpha);
            colors.gridMajorColor = Color(base.r + 0.08f,
                                          base.g + 0.08f,
                                          base.b + 0.08f,
                                          gridAlpha * 2.0f);

            // axis colors standard RG
            colors.gridAxisXColor = Color(0.8f, 0.3f, 0.3f, 0.1f); // Muted Red
            colors.gridAxisYColor =
                Color(0.3f, 0.8f, 0.3f, 0.1f); // Muted Green
        }

        colors.moduleColor = Color(0.49, 0.81, 0.99f, 1.f);

        const Color frame = toVec4(imguiColors[ImGuiCol_FrameBg], 0.96f);
        const Color frameHover =
            toVec4(imguiColors[ImGuiCol_FrameBgHovered], 0.98f);
        const Color frameActive =
            toVec4(imguiColors[ImGuiCol_FrameBgActive], 1.0f);
        const Color popup = toVec4(imguiColors[ImGuiCol_PopupBg], 0.99f);
        const Color border = toVec4(imguiColors[ImGuiCol_Border], 0.82f);
        const Color text = toVec4(imguiColors[ImGuiCol_Text]);
        const Color textMuted = toVec4(imguiColors[ImGuiCol_TextDisabled]);
        const Color headerHovered =
            toVec4(imguiColors[ImGuiCol_HeaderHovered], 1.0f);
        const Color headerActive =
            toVec4(imguiColors[ImGuiCol_HeaderActive], 1.0f);
        const Color sliderGrab = toVec4(imguiColors[ImGuiCol_SliderGrab], 1.0f);
        const Color sliderGrabActive =
            toVec4(imguiColors[ImGuiCol_SliderGrabActive], 1.0f);

        sceneWidgetsColors.surface = frame;
        sceneWidgetsColors.surfaceHover = frameHover;
        sceneWidgetsColors.surfaceActive = frameActive;
        sceneWidgetsColors.popupSurface = popup;
        sceneWidgetsColors.border = border;
        sceneWidgetsColors.borderFocus = sliderGrabActive;
        sceneWidgetsColors.text = text;
        sceneWidgetsColors.textMuted = textMuted;
        sceneWidgetsColors.accent = sliderGrabActive;
        sceneWidgetsColors.accentStrong = headerActive;
        sceneWidgetsColors.itemHover = headerHovered;
        sceneWidgetsColors.track = frame;
        sceneWidgetsColors.knob = sliderGrab;
    }

    Color ViewportTheme::getCompHeaderColor(const std::string &group) {
        if (group == "IO")
            return headerColors.io;
        else if (group == "Flip Flops")
            return headerColors.flipFlops;
        else if (group == "Digital Gates")
            return headerColors.digitalGates;
        else if (group == "Latches")
            return headerColors.latches;
        else if (group == "Combinational Circuits")
            return headerColors.combinationalCircuits;
        else if (group == "Registers" || group == "Memory" ||
                 group == "Register/Memory")
            return headerColors.registersMemory;
        else
            return headerColors.default_;
    }

    void ViewportTheme::cleanup() {
        headerColors = {};
        schematicViewColors = {};
        sceneWidgetsColors = {};
        colors = {};
    }

} // namespace Bess
