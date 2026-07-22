#include "bess_core/settings/viewport_theme.h"
#include "imgui.h"

namespace Bess {
    bool ViewportTheme::isDark = true;
    SceneColors ViewportTheme::colors;
    SchematicViewColors ViewportTheme::schematicViewColors;
    SceneWidgetsColors ViewportTheme::sceneWidgetsColors;
    NodeHeaderColors ViewportTheme::headerColors;

    namespace {
        Color toVec4(const ImVec4 &color, float alpha = -1.f) {
            return {color.x, color.y, color.z, alpha >= 0.f ? alpha : color.w};
        }

        inline ImVec4 getImVec4(const Bess::Core::Renderer::Color &color) {
            return {color.r, color.g, color.b, color.a};
        }
    } // namespace

    void ViewportTheme::updateFromBessTheme(
        const std::shared_ptr<Core::Style::BessTheme> &theme) {
        if (!theme) {
            return;
        }

        const auto &colorScheme = theme->getColorScheme();
        const bool isDark = colorScheme.isDark();
        ViewportTheme::isDark = isDark;

        const auto &themeColors = colorScheme.getColors();

        const auto &windowBg = themeColors.surface;
        const auto &textCol = themeColors.onSurface;
        const auto &borderCol = themeColors.outlineVariant;
        const auto &frameBg = themeColors.surfaceContainerHighest;
        const auto &headerCol = themeColors.surfaceContainerHigh;
        const auto &sliderGrabActive = themeColors.inversePrimary;

        // 1. Core Viewport Canvas Background
        if (isDark) {
            colors.background = Color(windowBg.r * 0.82f,
                                      windowBg.g * 0.82f,
                                      windowBg.b * 0.85f,
                                      1.0f);
        } else {
            colors.background = Color(windowBg.r * 0.97f,
                                      windowBg.g * 0.97f,
                                      windowBg.b * 0.95f,
                                      1.0f);
        }

        // 2. Structural Node/Component Properties
        colors.componentBG = Color(frameBg.r, frameBg.g, frameBg.b, 0.95f);
        colors.componentBorder =
            Color(borderCol.r, borderCol.g, borderCol.b, 0.8f);
        colors.compHeader = Color(headerCol.r, headerCol.g, headerCol.b, 1.0f);
        colors.text = textCol;

        // 3. Responsive Logic Signals (Green)
        if (isDark) {
            colors.stateHigh =
                Color(0.30f, 0.80f, 0.40f, 1.00f); // Bright electric emerald
            colors.stateLow =
                Color(0.12f, 0.22f, 0.14f, 1.00f); // Deep forest shadow
        } else {
            colors.stateHigh =
                Color(0.08f, 0.7f, 0.20f, 1.00f); // Rich legible jade green
            colors.stateLow = Color(0.35f, 0.42f, 0.37f, 1.00f);
        }

        // 4. Responsive Clock Connections (Blue)
        if (isDark) {
            colors.clockConnectionHigh =
                Color(0.25f, 0.65f, 1.00f, 1.00f); // Electric sky blue
            colors.clockConnectionLow =
                Color(0.10f, 0.18f, 0.32f, 1.00f); // Midnight blue tint
        } else {
            colors.clockConnectionHigh =
                Color(0.00f, 0.45f, 0.85f, 1.00f); // Strong deep corporate blue
            colors.clockConnectionLow =
                Color(0.88f, 0.93f, 0.98f, 1.00f); // Airy powder blue sheet
        }

        // 5. Wires & Interactive Selection (Vibrant Orange Accent)
        colors.wire = colors.componentBorder;
        colors.ghostWire = Color(textCol.r, textCol.g, textCol.b, 0.40f);

        if (isDark) {
            colors.selectedWire =
                Color(1.00f, 0.55f, 0.05f, 1.0f); // Neon glowing orange
            colors.selectionBoxFill = Color(1.00f, 0.55f, 0.05f, 0.06f);
        } else {
            colors.selectedWire = Color(
                0.90f, 0.42f, 0.00f, 1.0f); // Deeper burnt amber for visibility
            colors.selectionBoxFill = Color(0.90f, 0.42f, 0.00f, 0.08f);
        }
        colors.selectedComp = colors.selectedWire;
        colors.selectionBoxBorder = colors.selectedWire;

        // 6. Responsive Schematic Grid Generation
        {
            const Color base = colors.background;
            if (isDark) {
                colors.gridMinorColor =
                    Color(1.0f, 1.0f, 1.0f, 0.015f); // 4% white overlay
                colors.gridMajorColor =
                    Color(1.0f, 1.0f, 1.0f, 0.02f); // 9% white overlay

                // FIXED: Restored your exact, beautiful dark theme layout
                // constraints
                colors.gridAxisXColor =
                    Color(0.80f, 0.30f, 0.30f, 0.10f); // Muted Red
                colors.gridAxisYColor =
                    Color(0.30f, 0.80f, 0.30f, 0.10f); // Muted Green
            } else {
                colors.gridMinorColor = Color(
                    base.r * 0.94f, base.g * 0.94f, base.b * 0.92f, 0.60f);
                colors.gridMajorColor = Color(
                    base.r * 0.86f, base.g * 0.86f, base.b * 0.83f, 0.85f);

                // Balanced light theme axis profile so they remain legible but
                // clean
                colors.gridAxisXColor = Color(0.75f, 0.25f, 0.25f, 0.15f);
                colors.gridAxisYColor = Color(0.25f, 0.70f, 0.25f, 0.15f);
            }
        }

        // 7. Base Component Module Fill
        colors.moduleColor = isDark ? Color(0.38f, 0.72f, 0.95f, 1.0f)
                                    : Color(0.05f, 0.50f, 0.80f, 1.0f);

        // // 8. Scene Widgets Sync Mapping
        // const Color frame = toVec4(imguiColors[ImGuiCol_Button], 0.96f);
        // const Color frameHover =
        //     toVec4(imguiColors[ImGuiCol_ButtonHovered], 0.98f);
        // const Color frameActive =
        //     toVec4(imguiColors[ImGuiCol_ButtonActive], 1.0f);
        // const Color popup = toVec4(imguiColors[ImGuiCol_PopupBg], 0.99f);
        // const Color border = toVec4(imguiColors[ImGuiCol_Border], 0.82f);
        // const Color text = toVec4(imguiColors[ImGuiCol_Text]);
        // const Color textMuted = toVec4(imguiColors[ImGuiCol_TextDisabled]);
        // const Color headerHovered =
        //     toVec4(imguiColors[ImGuiCol_HeaderHovered], 1.0f);
        // const Color headerActive =
        //     toVec4(imguiColors[ImGuiCol_HeaderActive], 1.0f);
        // const Color sliderGrab =
        // toVec4(imguiColors[ImGuiCol_SliderGrab], 1.0f);
        //
        // sceneWidgetsColors.surface =
        //     themeColors.surfaceContainerHigh.withAlpha(0.96f);
        // sceneWidgetsColors.surfaceHover = frameHover;
        // sceneWidgetsColors.surfaceActive = frameActive;
        // sceneWidgetsColors.popupSurface = popup;
        // sceneWidgetsColors.border = border;
        // sceneWidgetsColors.borderFocus = toVec4(sliderGrabActive, 1.0f);
        // sceneWidgetsColors.text = text;
        // sceneWidgetsColors.textMuted = textMuted;
        // sceneWidgetsColors.accent = toVec4(sliderGrabActive, 1.0f);
        // sceneWidgetsColors.accentStrong = headerActive;
        // sceneWidgetsColors.itemHover = headerHovered;
        // sceneWidgetsColors.track = frame;
        // sceneWidgetsColors.knob = sliderGrab;

        if (isDark) {
            schematicViewColors.componentFill =
                Color(0.13f, 0.14f, 0.17f, 1.0f);

            schematicViewColors.componentStroke =
                Color(0.26f, 0.30f, 0.36f, 1.0f);

            schematicViewColors.text = Color(0.86f, 0.88f, 0.92f, 1.0f);

            schematicViewColors.pin =
                Color(0.22f, 0.78f, 0.88f, 1.0f); // Vivid Electric Cyan
            schematicViewColors.connection =
                Color(0.00f, 0.85f, 0.45f, 1.0f); // Cyberpunk Emerald Green

            schematicViewColors.activeSignal = Color(1.00f, 0.84f, 0.10f, 1.0f);
        } else {
            schematicViewColors.pin = Color(0.02f, 0.48f, 0.62f, 1.0f);

            schematicViewColors.text = Color(0.12f, 0.14f, 0.16f, 1.0f);

            schematicViewColors.connection = Color(0.00f, 0.52f, 0.26f, 1.0f);

            schematicViewColors.componentFill =
                Color(1.00f, 1.00f, 1.00f, 1.0f);

            schematicViewColors.componentStroke =
                Color(0.25f, 0.28f, 0.35f, 1.0f);

            schematicViewColors.activeSignal = Color(0.92f, 0.45f, 0.00f, 1.0f);
        }
    }

    void ViewportTheme::updateColorsFromImGuiStyle(bool isDark) {
        ViewportTheme::isDark = isDark;
        ImGuiStyle &style = ImGui::GetStyle();
        const ImVec4 *imguiColors = style.Colors;

        const ImVec4 windowBg = imguiColors[ImGuiCol_WindowBg];
        const ImVec4 textCol = imguiColors[ImGuiCol_Text];
        const ImVec4 borderCol = imguiColors[ImGuiCol_Border];
        const ImVec4 frameBg = imguiColors[ImGuiCol_FrameBg];
        const ImVec4 headerCol = imguiColors[ImGuiCol_Header];
        const ImVec4 sliderGrabActive = imguiColors[ImGuiCol_SliderGrabActive];

        // 1. Core Viewport Canvas Background
        if (isDark) {
            colors.background = Color(windowBg.x * 0.82f,
                                      windowBg.y * 0.82f,
                                      windowBg.z * 0.85f,
                                      1.0f);
        } else {
            colors.background = Color(windowBg.x * 0.97f,
                                      windowBg.y * 0.97f,
                                      windowBg.z * 0.95f,
                                      1.0f);
        }

        // 2. Structural Node/Component Properties
        colors.componentBG = Color(frameBg.x, frameBg.y, frameBg.z, 0.95f);
        colors.componentBorder =
            Color(borderCol.x, borderCol.y, borderCol.z, 0.8f);
        colors.compHeader = Color(headerCol.x, headerCol.y, headerCol.z, 1.0f);
        colors.text = Color(textCol.x, textCol.y, textCol.z, textCol.w);

        // 3. Responsive Logic Signals (Green)
        if (isDark) {
            colors.stateHigh =
                Color(0.30f, 0.80f, 0.40f, 1.00f); // Bright electric emerald
            colors.stateLow =
                Color(0.12f, 0.22f, 0.14f, 1.00f); // Deep forest shadow
        } else {
            colors.stateHigh =
                Color(0.08f, 0.7f, 0.20f, 1.00f); // Rich legible jade green
            colors.stateLow = Color(0.35f, 0.42f, 0.37f, 1.00f);
        }

        // 4. Responsive Clock Connections (Blue)
        if (isDark) {
            colors.clockConnectionHigh =
                Color(0.25f, 0.65f, 1.00f, 1.00f); // Electric sky blue
            colors.clockConnectionLow =
                Color(0.10f, 0.18f, 0.32f, 1.00f); // Midnight blue tint
        } else {
            colors.clockConnectionHigh =
                Color(0.00f, 0.45f, 0.85f, 1.00f); // Strong deep corporate blue
            colors.clockConnectionLow =
                Color(0.88f, 0.93f, 0.98f, 1.00f); // Airy powder blue sheet
        }

        // 5. Wires & Interactive Selection (Vibrant Orange Accent)
        colors.wire = colors.componentBorder;
        colors.ghostWire = Color(textCol.x, textCol.y, textCol.z, 0.40f);

        if (isDark) {
            colors.selectedWire =
                Color(1.00f, 0.55f, 0.05f, 1.0f); // Neon glowing orange
            colors.selectionBoxFill = Color(1.00f, 0.55f, 0.05f, 0.06f);
        } else {
            colors.selectedWire = Color(
                0.90f, 0.42f, 0.00f, 1.0f); // Deeper burnt amber for visibility
            colors.selectionBoxFill = Color(0.90f, 0.42f, 0.00f, 0.08f);
        }
        colors.selectedComp = colors.selectedWire;
        colors.selectionBoxBorder = colors.selectedWire;

        // 6. Responsive Schematic Grid Generation
        {
            const Color base = colors.background;
            if (isDark) {
                colors.gridMinorColor =
                    Color(1.0f, 1.0f, 1.0f, 0.015f); // 4% white overlay
                colors.gridMajorColor =
                    Color(1.0f, 1.0f, 1.0f, 0.02f); // 9% white overlay

                // FIXED: Restored your exact, beautiful dark theme layout
                // constraints
                colors.gridAxisXColor =
                    Color(0.80f, 0.30f, 0.30f, 0.10f); // Muted Red
                colors.gridAxisYColor =
                    Color(0.30f, 0.80f, 0.30f, 0.10f); // Muted Green
            } else {
                colors.gridMinorColor = Color(
                    base.r * 0.94f, base.g * 0.94f, base.b * 0.92f, 0.60f);
                colors.gridMajorColor = Color(
                    base.r * 0.86f, base.g * 0.86f, base.b * 0.83f, 0.85f);

                // Balanced light theme axis profile so they remain legible but
                // clean
                colors.gridAxisXColor = Color(0.75f, 0.25f, 0.25f, 0.15f);
                colors.gridAxisYColor = Color(0.25f, 0.70f, 0.25f, 0.15f);
            }
        }

        // 7. Base Component Module Fill
        colors.moduleColor = isDark ? Color(0.38f, 0.72f, 0.95f, 1.0f)
                                    : Color(0.05f, 0.50f, 0.80f, 1.0f);

        // 8. Scene Widgets Sync Mapping
        const Color frame = toVec4(imguiColors[ImGuiCol_Button], 0.96f);
        const Color frameHover =
            toVec4(imguiColors[ImGuiCol_ButtonHovered], 0.98f);
        const Color frameActive =
            toVec4(imguiColors[ImGuiCol_ButtonActive], 1.0f);
        const Color popup = toVec4(imguiColors[ImGuiCol_PopupBg], 0.99f);
        const Color border = toVec4(imguiColors[ImGuiCol_Border], 0.82f);
        const Color text = toVec4(imguiColors[ImGuiCol_Text]);
        const Color textMuted = toVec4(imguiColors[ImGuiCol_TextDisabled]);
        const Color headerHovered =
            toVec4(imguiColors[ImGuiCol_HeaderHovered], 1.0f);
        const Color headerActive =
            toVec4(imguiColors[ImGuiCol_HeaderActive], 1.0f);
        const Color sliderGrab = toVec4(imguiColors[ImGuiCol_SliderGrab], 1.0f);

        sceneWidgetsColors.surface = frame;
        sceneWidgetsColors.surfaceHover = frameHover;
        sceneWidgetsColors.surfaceActive = frameActive;
        sceneWidgetsColors.popupSurface = popup;
        sceneWidgetsColors.border = border;
        sceneWidgetsColors.borderFocus = toVec4(sliderGrabActive, 1.0f);
        sceneWidgetsColors.text = text;
        sceneWidgetsColors.textMuted = textMuted;
        sceneWidgetsColors.accent = toVec4(sliderGrabActive, 1.0f);
        sceneWidgetsColors.accentStrong = headerActive;
        sceneWidgetsColors.itemHover = headerHovered;
        sceneWidgetsColors.track = frame;
        sceneWidgetsColors.knob = sliderGrab;

        if (isDark) {
            schematicViewColors.componentFill =
                Color(0.13f, 0.14f, 0.17f, 1.0f);

            schematicViewColors.componentStroke =
                Color(0.26f, 0.30f, 0.36f, 1.0f);

            schematicViewColors.text = Color(0.86f, 0.88f, 0.92f, 1.0f);

            schematicViewColors.pin =
                Color(0.22f, 0.78f, 0.88f, 1.0f); // Vivid Electric Cyan
            schematicViewColors.connection =
                Color(0.00f, 0.85f, 0.45f, 1.0f); // Cyberpunk Emerald Green

            schematicViewColors.activeSignal = Color(1.00f, 0.84f, 0.10f, 1.0f);
        } else {
            schematicViewColors.pin = Color(0.02f, 0.48f, 0.62f, 1.0f);

            schematicViewColors.text = Color(0.12f, 0.14f, 0.16f, 1.0f);

            schematicViewColors.connection = Color(0.00f, 0.52f, 0.26f, 1.0f);

            schematicViewColors.componentFill =
                Color(1.00f, 1.00f, 1.00f, 1.0f);

            schematicViewColors.componentStroke =
                Color(0.25f, 0.28f, 0.35f, 1.0f);

            schematicViewColors.activeSignal = Color(0.92f, 0.45f, 0.00f, 1.0f);
        }
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
