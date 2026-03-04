#include "ui/ui_main/settings_window.h"
#include "imgui.h"

#include "settings/settings.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {
    SettingsWindow::SettingsWindow() : Panel("Settings Window") {
        m_defaultDock = Dock::none;
        m_showInMenuBar = false;
    }

    void SettingsWindow::onBeforeDraw() {
        ImGui::SetNextWindowSize(ImVec2(600, 600));
        m_flags = ImGuiWindowFlags_NoCollapse;
    }

    void SettingsWindow::onDraw() {
        auto &settings = Config::Settings::instance();
        auto currentTheme = settings.getCurrentTheme();

        if (drawSetting("Theme", "(Default: Bess Dark)", currentTheme, m_availableThemes)) {
            settings.applyTheme(currentTheme);
        }

        ImGui::NewLine();
        auto fontSize = settings.getFontSize();
        if (drawSetting("Font Size", "(Default: 18px)", fontSize, m_availableFontSizes)) {
            settings.setFontSize(fontSize);
        }

        ImGui::NewLine();
        auto scale = settings.getScale();
        if (drawSetting("Scale", "(Default: 1)", scale, m_availableScales)) {
            settings.setScale(scale);
        }

        ImGui::NewLine();
        auto fps = settings.getFps();
        if (drawSetting("FPS",
                        "(Default and Recommended: 60) Higher number gives smoothness but with high GPU consumption.",
                        fps, m_availableFps)) {
            settings.setFps(fps);
        }

        ImGui::NewLine();
        auto showStatsWindow = settings.getShowStatsWindow();
        if (Widgets::CheckboxWithLabel("Show Stats Window", &showStatsWindow)) {
            settings.setShowStatsWindow(showStatsWindow);
        }
    }

    void SettingsWindow::onShow() {
        // Populate available font sizes
        m_availableFontSizes = {10.f, 12.f, 14.f, 16.f,
                                18.f, 20.f, 22.f, 24.f};

        // Populate available scales
        m_availableScales.clear();
        for (float i = 1.f; i <= 2.0f; i += 0.1f)
            m_availableScales.emplace_back(i);

        // Populate available themes
        m_availableThemes.clear();
        auto &themes = Config::Settings::instance().getThemes();
        for (auto &ent : themes.getThemes())
            m_availableThemes.emplace_back(ent.first);

        // Populate available fps
        m_availableFps = {60, 90, 120, 144, 240};
    }

} // namespace Bess::UI
