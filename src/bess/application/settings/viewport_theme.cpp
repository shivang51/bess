#include "settings/viewport_theme.h"
#include "imgui.h"
#include <algorithm>

namespace Bess {
    SceneColors ViewportTheme::colors;
    SchematicViewColors ViewportTheme::schematicViewColors;
    SceneWidgetsColors ViewportTheme::sceneWidgetsColors;

    namespace {
        glm::vec4 toVec4(const ImVec4 &color, float alpha = -1.f) {
            return {color.x, color.y, color.z, alpha >= 0.f ? alpha : color.w};
        }

        glm::vec4 withAlpha(glm::vec4 color, float alpha) {
            color.a = alpha;
            return color;
        }

        glm::vec4 mixColor(const glm::vec4 &a, const glm::vec4 &b, float t) {
            t = std::clamp(t, 0.f, 1.f);
            return {
                a.r + ((b.r - a.r) * t),
                a.g + ((b.g - a.g) * t),
                a.b + ((b.b - a.b) * t),
                a.a + ((b.a - a.a) * t),
            };
        }
    } // namespace

    void ViewportTheme::updateColorsFromImGuiStyle() {
        ImGuiStyle &style = ImGui::GetStyle();
        const ImVec4 *imguiColors = style.Colors;

        const ImVec4 windowBg = imguiColors[ImGuiCol_WindowBg];
        colors.background = glm::vec4(
            windowBg.x * 0.85f, windowBg.y * 0.85f, windowBg.z * 0.85f, 1.0f);

        const ImVec4 frameBg = imguiColors[ImGuiCol_FrameBg];
        colors.componentBG = glm::vec4(frameBg.x, frameBg.y, frameBg.z, 0.95f);

        const ImVec4 borderCol = imguiColors[ImGuiCol_Border];
        colors.componentBorder =
            glm::vec4(borderCol.x, borderCol.y, borderCol.z, 0.8f);

        const ImVec4 headerCol = imguiColors[ImGuiCol_Header];
        colors.compHeader =
            glm::vec4(headerCol.x, headerCol.y, headerCol.z, 1.0f);

        const ImVec4 textCol = imguiColors[ImGuiCol_Text];
        colors.text = glm::vec4(textCol.x, textCol.y, textCol.z, textCol.w);

        colors.stateHigh = glm::vec4(0.35f, 0.85f, 0.35f, 1.00f);
        colors.stateLow = glm::vec4(0.15f, 0.25f, 0.15f, 1.00f);

        colors.wire = colors.componentBorder;
        colors.ghostWire = glm::vec4(textCol.x, textCol.y, textCol.z, 0.3f);

        colors.selectedWire = glm::vec4(1.0f, 0.60f, 0.0f, 1.0f);
        colors.selectedComp = colors.selectedWire;

        colors.clockConnectionHigh = glm::vec4(0.30f, 0.70f, 1.0f, 1.0f);
        colors.clockConnectionLow = glm::vec4(0.10f, 0.20f, 0.4f, 1.0f);

        colors.selectionBoxBorder = colors.selectedWire;
        colors.selectionBoxFill =
            glm::vec4(1.0f, 0.60f, 0.0f, 0.08f); // Very faint orange tint

        {
            const glm::vec4 base = colors.background;
            // subtle additive grid (uses very faint lines)
            float gridAlpha = 0.2f;

            colors.gridMinorColor = glm::vec4(
                base.r + 0.04f, base.g + 0.04f, base.b + 0.04f, gridAlpha);
            colors.gridMajorColor = glm::vec4(base.r + 0.08f,
                                              base.g + 0.08f,
                                              base.b + 0.08f,
                                              gridAlpha * 2.0f);

            // axis colors standard RG
            colors.gridAxisXColor =
                glm::vec4(0.8f, 0.3f, 0.3f, 0.1f); // Muted Red
            colors.gridAxisYColor =
                glm::vec4(0.3f, 0.8f, 0.3f, 0.1f); // Muted Green
        }

        initCompColorMap();

        colors.moduleColor = glm::vec4(0.49, 0.81, 0.99f, 1.f);

        const glm::vec4 frame = toVec4(imguiColors[ImGuiCol_FrameBg], 0.96f);
        const glm::vec4 frameHover =
            toVec4(imguiColors[ImGuiCol_FrameBgHovered], 0.98f);
        const glm::vec4 frameActive =
            toVec4(imguiColors[ImGuiCol_FrameBgActive], 1.0f);
        const glm::vec4 popup = toVec4(imguiColors[ImGuiCol_PopupBg], 0.99f);
        const glm::vec4 border = toVec4(imguiColors[ImGuiCol_Border], 0.82f);
        const glm::vec4 text = toVec4(imguiColors[ImGuiCol_Text]);
        const glm::vec4 textMuted = toVec4(imguiColors[ImGuiCol_TextDisabled]);
        const glm::vec4 checkMark =
            toVec4(imguiColors[ImGuiCol_CheckMark], 1.0f);
        const glm::vec4 sliderGrab =
            toVec4(imguiColors[ImGuiCol_SliderGrab], 1.0f);
        const glm::vec4 sliderGrabActive =
            toVec4(imguiColors[ImGuiCol_SliderGrabActive], 1.0f);

        sceneWidgetsColors.surface =
            withAlpha(mixColor(colors.background, frame, 0.70f), 0.96f);
        sceneWidgetsColors.surfaceHover =
            withAlpha(mixColor(frameHover, colors.background, 0.10f), 0.98f);
        sceneWidgetsColors.surfaceActive =
            withAlpha(mixColor(frameActive, colors.background, 0.08f), 1.0f);
        sceneWidgetsColors.popupSurface =
            withAlpha(mixColor(popup, frame, 0.20f), 0.99f);
        sceneWidgetsColors.border = border;
        sceneWidgetsColors.borderFocus =
            withAlpha(mixColor(sliderGrabActive, checkMark, 0.35f), 1.0f);
        sceneWidgetsColors.text = text;
        sceneWidgetsColors.textMuted = textMuted;
        sceneWidgetsColors.accent =
            withAlpha(mixColor(checkMark, sliderGrab, 0.35f), 1.0f);
        sceneWidgetsColors.accentStrong =
            withAlpha(mixColor(sliderGrabActive, checkMark, 0.20f), 1.0f);
        sceneWidgetsColors.track =
            withAlpha(mixColor(frame, border, 0.55f), 0.95f);
        sceneWidgetsColors.knob = withAlpha(mixColor(text, frame, 0.20f), 1.0f);
    }

    glm::vec4 ViewportTheme::getCompHeaderColor(const std::string &group) {
        const auto &s_compHeaderColorMap = getCompHeaderColorMap();
        if (!s_compHeaderColorMap.contains(group))
            return colors.compHeader;
        return s_compHeaderColorMap.at(group);
    }

    void ViewportTheme::initCompColorMap() {

        const glm::vec4 memoryColor = glm::vec4(
            0.48f, 0.35f, 0.58f, 0.90f); // Muted Amethyst (State/Memory)
        const glm::vec4 arithmeticColor =
            glm::vec4(0.32f, 0.56f, 0.32f, 0.90f); // Sage Green (Math/Numbers)
        const glm::vec4 routingColor = glm::vec4(
            0.72f, 0.45f, 0.25f, 0.90f); // Ochre/Orange (Routing/Data Flow)
        const glm::vec4 encoderDecoderColor = glm::vec4(
            0.65f, 0.30f, 0.30f, 0.90f); // Muted Terracotta (Conversion)
        const glm::vec4 combinationalColor =
            glm::vec4(0.25f, 0.55f, 0.55f, 0.90f); // Deep Cyan (Process)
        const glm::vec4 ioColor =
            glm::vec4(0.45f, 0.45f, 0.45f, 0.90f); // Graphite (Hardware I/O)
        const glm::vec4 specialColor =
            glm::vec4(0.35f, 0.35f, 0.35f, 0.85f); // Dark Grey
        const glm::vec4 logicColor = arithmeticColor;

        auto &s_compHeaderColorMap = getCompHeaderColorMap();
        s_compHeaderColorMap.clear();

        s_compHeaderColorMap["IO"] = ioColor;

        s_compHeaderColorMap["Flip Flops"] = memoryColor;
        s_compHeaderColorMap["Registers/Memory"] =
            memoryColor; // Added for future use

        s_compHeaderColorMap["Digital Gates"] = logicColor;

        s_compHeaderColorMap["Latches"] = encoderDecoderColor;

        s_compHeaderColorMap["Combinational Circuits"] = combinationalColor;

        s_compHeaderColorMap["Miscellaneous"] = specialColor;
    }

    void ViewportTheme::cleanup() {
        getCompHeaderColorMap().clear();
        schematicViewColors = {};
        sceneWidgetsColors = {};
        colors = {};
    }

    std::unordered_map<std::string, glm::vec4> &
    ViewportTheme::getCompHeaderColorMap() {
        static std::unordered_map<std::string, glm::vec4> s_compHeaderColorMap;
        return s_compHeaderColorMap;
    }
} // namespace Bess
