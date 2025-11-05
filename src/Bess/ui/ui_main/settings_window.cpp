#include "ui/ui_main/settings_window.h"
#include "imgui.h"

#include "settings/settings.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {
    void SettingsWindow::draw() {
        if (!m_shown)
            return;

        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

        ImGui::SetNextWindowSize(ImVec2(600, 600));

        ImGui::Begin("Settings", &m_shown, flags);

        auto currentTheme = Config::Settings::getCurrentTheme();
        auto &themes = Config::Settings::getThemes();

        std::vector<std::string> availableThemes = {};
        for (auto &ent : themes.getThemes())
            availableThemes.emplace_back(ent.first);

        if (Widgets::ComboBox("Theme", currentTheme, availableThemes)) {
            Config::Settings::applyTheme(currentTheme);
        }

        auto fontSize = Config::Settings::getFontSize();
        static std::vector<float> availableFontSizes = {10.f, 12.f, 14.f, 16.f, 18.f, 20.f, 22.f, 24.f};
        if (Widgets::ComboBox("Font Size", fontSize, availableFontSizes)) {
            Config::Settings::setFontSize(fontSize);
        }

        auto scale = Config::Settings::getScale();
        std::vector<float> availableScales = {};
        for (float i = 1.f; i <= 2.0f; i += 0.1f)
            availableScales.emplace_back(i);
        if (Widgets::ComboBox("Scale", scale, availableScales)) {
            Config::Settings::setScale(scale);
        }

        ImGui::End();
    }

    bool SettingsWindow::m_shown = false;

    void SettingsWindow::hide() {
        m_shown = false;
    }

    void SettingsWindow::show() {
        m_shown = true;
    }

    bool SettingsWindow::isShown() {
        return m_shown;
    }

} // namespace Bess::UI
