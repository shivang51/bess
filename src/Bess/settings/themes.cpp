#include "settings/themes.h"
#include "imgui.h"
#include "settings/viewport_theme.h"

namespace Bess::Config {
    Themes::Themes() {
        m_themes["Dark"] = [this]() { setDarkThemeColors(); };
        m_themes["Modern Dark"] = [this]() { setModernDarkColors(); };
        m_themes["Catpuccin Mocha"] = [this]() { setCatpuccinMochaColors(); };
        m_themes["Bess Dark"] = [this]() { setBessDarkColors(); };
        m_themes["Fluent UI"] = [this]() { setFluentUIColors(); };
    }

    void Themes::applyTheme(const std::string &theme) {
        if (m_themes.find(theme) == m_themes.end())
            return;

        m_themes[theme]();
        ViewportTheme::updateColorsFromImGuiStyle();
    }

    void Themes::addTheme(const std::string &name, const std::function<void()> &callback) {
        m_themes[name] = callback;
    }

    const std::unordered_map<std::string, std::function<void()>> &Themes::getThemes() const {
        return m_themes;
    }

    ImVec4 BlendColors(const ImVec4 &base, const ImVec4 &accent, float blendFactor) {
        return ImVec4(
            base.x * (1.0f - blendFactor) + accent.x * blendFactor,
            base.y * (1.0f - blendFactor) + accent.y * blendFactor,
            base.z * (1.0f - blendFactor) + accent.z * blendFactor,
            base.w);
    }

    void Themes::setBessDarkColors() {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *colors = style.Colors;

        const ImVec4 base0 = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
        const ImVec4 base1 = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);
        const ImVec4 base2 = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);
        const ImVec4 base3 = ImVec4(0.23f, 0.23f, 0.24f, 1.0f);

        const ImVec4 accent = ImVec4(0.70f, 0.55f, 0.20f, 1.0f);
        const ImVec4 accentMid = ImVec4(0.60f, 0.45f, 0.18f, 0.85f);
        const ImVec4 accentSoft = ImVec4(0.50f, 0.38f, 0.15f, 0.60f);

        const ImVec4 text0 = ImVec4(0.94f, 0.94f, 0.95f, 1.0f);
        const ImVec4 text1 = ImVec4(0.60f, 0.60f, 0.62f, 1.0f);

        colors[ImGuiCol_Text] = text0;
        colors[ImGuiCol_TextDisabled] = text1;

        colors[ImGuiCol_WindowBg] = base0;
        colors[ImGuiCol_ChildBg] = base0;
        colors[ImGuiCol_PopupBg] = base1;

        colors[ImGuiCol_Border] = ImVec4(0.28f, 0.28f, 0.30f, 1.0f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

        colors[ImGuiCol_FrameBg] = base2;
        colors[ImGuiCol_FrameBgHovered] = base3;
        colors[ImGuiCol_FrameBgActive] = base3;

        colors[ImGuiCol_Button] = base2;
        colors[ImGuiCol_ButtonHovered] = base3;
        colors[ImGuiCol_ButtonActive] = base1;

        colors[ImGuiCol_Header] = base2;
        colors[ImGuiCol_HeaderHovered] = base3;
        colors[ImGuiCol_HeaderActive] = base3;

        colors[ImGuiCol_Tab] = base2;
        colors[ImGuiCol_TabHovered] = ImVec4(accentSoft.x, accentSoft.y, accentSoft.z, 0.30f);
        colors[ImGuiCol_TabActive] = ImVec4(accent.x, accent.y, accent.z, 0.50f);
        colors[ImGuiCol_TabUnfocused] = base2;
        colors[ImGuiCol_TabUnfocusedActive] = base2;

        colors[ImGuiCol_ScrollbarBg] = base0;
        colors[ImGuiCol_ScrollbarGrab] = base3;
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

        colors[ImGuiCol_CheckMark] = ImVec4(accent.x, accent.y, accent.z, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(accent.x, accent.y, accent.z, 0.95f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(accentMid.x, accentMid.y, accentMid.z, 0.95f);

        colors[ImGuiCol_ResizeGrip] = ImVec4(1, 1, 1, 0.10f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(accentMid.x, accentMid.y, accentMid.z, 0.7f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(accentMid.x, accentMid.y, accentMid.z, 0.9f);

        colors[ImGuiCol_MenuBarBg] = base1;

        colors[ImGuiCol_Separator] = base3;
        colors[ImGuiCol_SeparatorHovered] = ImVec4(accentSoft.x, accentSoft.y, accentSoft.z, 0.45f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(accentMid.x, accentMid.y, accentMid.z, 0.65f);

        colors[ImGuiCol_TableHeaderBg] = base2;
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30f, 0.30f, 0.32f, 1.0f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);
        colors[ImGuiCol_TableRowBg] = base0;
        colors[ImGuiCol_TableRowBgAlt] = base1;

        colors[ImGuiCol_DragDropTarget] = ImVec4(accent.x, accent.y, accent.z, 0.65f);

        colors[ImGuiCol_NavHighlight] = ImVec4(accent.x, accent.y, accent.z, 0.25f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(accent.x, accent.y, accent.z, 0.20f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.5f);

        colors[ImGuiCol_TitleBg] = base1;
        colors[ImGuiCol_TitleBgActive] = base2;
        colors[ImGuiCol_TitleBgCollapsed] = base1;

        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.55f);

        style.WindowRounding = 6.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.ScrollbarRounding = 9.0f;
        style.PopupRounding = 6.0f;

        style.WindowPadding = ImVec2(10, 10);
        style.FramePadding = ImVec2(8, 6);
        style.ItemSpacing = ImVec2(8, 6);
        style.PopupBorderSize = 1.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
    }

    void Themes::setCatpuccinMochaColors() {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *colors = style.Colors;

        // Base colors inspired by Catppuccin Mocha
        colors[ImGuiCol_Text] = ImVec4(0.90f, 0.89f, 0.88f, 1.00f);         // Latte
        colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.56f, 0.52f, 1.00f); // Surface2
        colors[ImGuiCol_WindowBg] = ImVec4(0.17f, 0.14f, 0.20f, 1.00f);     // Base
        colors[ImGuiCol_ChildBg] = ImVec4(0.18f, 0.16f, 0.22f, 1.00f);      // Mantle
        colors[ImGuiCol_PopupBg] = ImVec4(0.17f, 0.14f, 0.20f, 1.00f);      // Base
        colors[ImGuiCol_Border] = ImVec4(0.27f, 0.23f, 0.29f, 1.00f);       // Overlay0
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f);              // Crust
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.20f, 0.29f, 1.00f);       // Overlay1
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.22f, 0.31f, 1.00f);        // Overlay2
        colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.12f, 0.18f, 1.00f);              // Mantle
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.17f, 0.15f, 0.21f, 1.00f);        // Mantle
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.12f, 0.18f, 1.00f);     // Mantle
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.17f, 0.15f, 0.22f, 1.00f);            // Base
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.17f, 0.14f, 0.20f, 1.00f);          // Base
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f);        // Crust
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.24f, 0.20f, 0.29f, 1.00f); // Overlay1
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.26f, 0.22f, 0.31f, 1.00f);  // Overlay2
        colors[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.66f, 0.47f, 1.00f);            // Peach
        colors[ImGuiCol_SliderGrab] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);           // Lavender
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f);     // Pink
        colors[ImGuiCol_Button] = ImVec4(0.65f, 0.34f, 0.46f, 1.00f);               // Maroon
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.71f, 0.40f, 0.52f, 1.00f);        // Red
        colors[ImGuiCol_ButtonActive] = ImVec4(0.76f, 0.46f, 0.58f, 1.00f);         // Pink
        colors[ImGuiCol_Header] = ImVec4(0.65f, 0.34f, 0.46f, 1.00f);               // Maroon
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.71f, 0.40f, 0.52f, 1.00f);        // Red
        colors[ImGuiCol_HeaderActive] = ImVec4(0.76f, 0.46f, 0.58f, 1.00f);         // Pink
        colors[ImGuiCol_Separator] = ImVec4(0.27f, 0.23f, 0.29f, 1.00f);            // Overlay0
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.95f, 0.66f, 0.47f, 1.00f);     // Peach
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.95f, 0.66f, 0.47f, 1.00f);      // Peach
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);           // Lavender
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f);    // Pink
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.92f, 0.61f, 0.85f, 1.00f);     // Mauve
        colors[ImGuiCol_Tab] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f);                  // Crust
        colors[ImGuiCol_TabHovered] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);           // Lavender
        colors[ImGuiCol_TabActive] = ImVec4(0.76f, 0.46f, 0.58f, 1.00f);            // Pink
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.16f, 0.22f, 1.00f);         // Mantle
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.21f, 0.18f, 0.25f, 1.00f);   // Crust
        colors[ImGuiCol_DockingPreview] = ImVec4(0.95f, 0.66f, 0.47f, 0.70f);       // Peach
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);       // Base
        colors[ImGuiCol_PlotLines] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);            // Lavender
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f);     // Pink
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);        // Lavender
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.89f, 0.54f, 0.79f, 1.00f); // Pink
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);        // Mantle
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.27f, 0.23f, 0.29f, 1.00f);    // Overlay0
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);     // Surface2
        colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);  // Surface0
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.82f, 0.61f, 0.85f, 0.35f); // Lavender
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.95f, 0.66f, 0.47f, 0.90f); // Peach
        colors[ImGuiCol_NavHighlight] = ImVec4(0.82f, 0.61f, 0.85f, 1.00f);   // Lavender
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

        // Style adjustments
        style.WindowRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 3.0f;
        style.ChildRounding = 4.0f;

        style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.FramePadding = ImVec2(5.0f, 4.0f);
        style.ItemSpacing = ImVec2(6.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
        style.IndentSpacing = 22.0f;

        style.ScrollbarSize = 14.0f;
        style.GrabMinSize = 10.0f;

        style.AntiAliasedLines = true;
        style.AntiAliasedFill = true;
    }

    void Themes::setModernDarkColors() {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *colors = style.Colors;

        // Base color scheme
        colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.11f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.19f, 0.20f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.20f, 0.21f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.28f, 0.28f, 0.29f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.33f, 0.34f, 0.35f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.41f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.35f, 0.40f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.30f, 0.35f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 0.80f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.30f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 0.80f);
        colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.33f, 0.67f, 1.00f, 1.00f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.33f, 0.67f, 1.00f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.38f, 0.48f, 0.69f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.38f, 0.59f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.28f, 0.56f, 1.00f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.28f, 0.56f, 1.00f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

        // Style adjustments
        style.WindowRounding = 5.3f;
        style.FrameRounding = 2.3f;
        style.ScrollbarRounding = 0;

        style.WindowTitleAlign = ImVec2(0.50f, 0.50f);
        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.FramePadding = ImVec2(5.0f, 5.0f);
        style.ItemSpacing = ImVec2(6.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
        style.IndentSpacing = 25.0f;
    }

    void Themes::setDarkThemeColors() {
        auto &colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_WindowBg] = ImVec4{0.1f, 0.105f, 0.11f, 1.0f};

        // Headers
        colors[ImGuiCol_Header] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
        colors[ImGuiCol_HeaderHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
        colors[ImGuiCol_HeaderActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

        // Buttons
        colors[ImGuiCol_Button] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
        colors[ImGuiCol_ButtonHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
        colors[ImGuiCol_ButtonActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

        // Frame BG
        colors[ImGuiCol_FrameBg] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
        colors[ImGuiCol_FrameBgHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
        colors[ImGuiCol_FrameBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

        // Tabs
        colors[ImGuiCol_Tab] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
        colors[ImGuiCol_TabHovered] = ImVec4{0.38f, 0.3805f, 0.381f, 1.0f};
        colors[ImGuiCol_TabActive] = ImVec4{0.28f, 0.2805f, 0.281f, 1.0f};
        colors[ImGuiCol_TabUnfocused] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};

        // Title
        colors[ImGuiCol_TitleBg] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
        colors[ImGuiCol_TitleBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
    }

    void Themes::setFluentUIColors() {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *colors = style.Colors;

        // General window settings
        style.WindowRounding = 5.0f;
        style.FrameRounding = 5.0f;
        style.ScrollbarRounding = 5.0f;
        style.GrabRounding = 5.0f;
        style.TabRounding = 5.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.PopupRounding = 5.0f;

        // Setting the colors
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.f);
        colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

        // Accent colors changed to darker olive-green/grey shades
        colors[ImGuiCol_CheckMark] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);        // Dark gray for check marks
        colors[ImGuiCol_SliderGrab] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);       // Dark gray for sliders
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f); // Slightly lighter gray when active
        colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);           // Button background (dark gray)
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);    // Button hover state
        colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);     // Button active state
        colors[ImGuiCol_Header] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);           // Dark gray for menu headers
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);    // Slightly lighter on hover
        colors[ImGuiCol_HeaderActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);     // Lighter gray when active
        colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);        // Separators in dark gray
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Resize grips in dark gray
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);        // Tabs background
        colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f); // Darker gray on hover
        colors[ImGuiCol_TabActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f); // Docking preview in gray
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f); // Empty dock background
        // Additional styles
        style.FramePadding = ImVec2(8.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 4.0f);
        style.IndentSpacing = 20.0f;
        style.ScrollbarSize = 16.0f;
    }

} // namespace Bess::Config
