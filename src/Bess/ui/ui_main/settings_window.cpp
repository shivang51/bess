#include "ui/ui_main/settings_window.h"
#include "imgui.h"

#include "settings/settings.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {
    void SettingsWindow::draw() {
        if (!m_shown)
            return;

        if (m_isFirstDraw) {
            onFirstDraw();
            m_isFirstDraw = false;
        }

        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

        ImGui::SetNextWindowSize(ImVec2(600, 600));

        ImGui::Begin("Settings", &m_shown, flags);

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

        ImGui::End();
    }

    void SettingsWindow::hide() {
        m_shown = false;
    }

    void SettingsWindow::show() {
        m_shown = true;
    }

    bool SettingsWindow::isShown() {
        return m_shown;
    }

    void SettingsWindow::onFirstDraw() {
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

    bool SettingsWindow::m_shown = false;
    bool SettingsWindow::m_isFirstDraw = true;
    std::vector<float> SettingsWindow::m_availableScales = {};
    std::vector<float> SettingsWindow::m_availableFontSizes = {};
    std::vector<std::string> SettingsWindow::m_availableThemes = {};
    std::vector<int> SettingsWindow::m_availableFps = {};
} // namespace Bess::UI
