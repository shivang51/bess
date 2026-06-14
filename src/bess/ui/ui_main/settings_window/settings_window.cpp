#include "ui/ui_main/settings_window/settings_window.h"
#include "bess_core/g_app_context.h"
#include "imgui.h"

#include "settings/settings.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {
    SettingsWindow::SettingsWindow()
        : Panel("Settings Window"),
          m_currentCategory("General") {

        m_defaultDock = Dock::none;
        m_showInMenuBar = false;

        m_settingsCallbacks["General"] = [this]() { drawGeneralSettings(); };
        m_settingsCallbacks["Viewport Colors"] = [this]() {
            drawViewportColorsSettings();
        };
    }

    void SettingsWindow::onBeforeDraw() {
        ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
        m_flags = ImGuiWindowFlags_NoCollapse;
    }

    void SettingsWindow::onDraw() {
        // No Border Child to separate the settings visually
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::BeginChild("##SettingsLeftPanel", ImVec2(200, 0), true);
        ImGui::PopStyleVar();
        drawLeftPanel();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::BeginChild("##SettingsRightPanel", ImVec2(0, 0), true);
        ImGui::PopStyleVar();
        drawRightPanel();
        ImGui::EndChild();
    }

    void SettingsWindow::onShow() {
        // Populate available font sizes
        m_availableFontSizes = {10.f, 12.f, 14.f, 16.f, 18.f, 20.f, 22.f, 24.f};

        // Populate available scales
        m_availableScales.clear();
        for (float i = 1.f; i <= 2.0f; i += 0.1f)
            m_availableScales.emplace_back(i);

        // Populate available themes
        m_availableThemes.clear();
        const auto &themes = GAppContext::getInstance()
                                 .getSubSystem<Config::Settings>()
                                 ->getThemes();
        for (auto &ent : themes.getThemes())
            m_availableThemes.emplace_back(ent.first);

        // Populate available fps
        m_availableFps = {60, 90, 120, 144, 240};

#if DEBUG
        m_availableFps.push_back(0); // Unlimited FPS for debugging
#endif
    }

    void SettingsWindow::drawRightPanel() {
        auto it = m_settingsCallbacks.find(m_currentCategory);
        BESS_ASSERT(it != m_settingsCallbacks.end(),
                    "Settings category not found: {}",
                    m_currentCategory);
        it->second();
    }

    void SettingsWindow::drawGeneralSettings() {
        const auto &settings =
            GAppContext::getInstance().getSubSystem<Config::Settings>();
        auto currentTheme = settings->getCurrentTheme();

        if (drawSetting("Theme",
                        "(Default: Bess Minimal Dark)",
                        currentTheme,
                        m_availableThemes)) {
            settings->applyTheme(currentTheme);
        }

        ImGui::NewLine();
        auto fontSize = settings->getFontSize();
        if (drawSetting("Font Size",
                        "(Default: 18px)",
                        fontSize,
                        m_availableFontSizes)) {
            settings->setFontSize(fontSize);
        }

        ImGui::NewLine();
        auto scale = settings->getScale();
        if (drawSetting("Scale", "(Default: 1)", scale, m_availableScales)) {
            settings->setScale(scale);
        }

        ImGui::NewLine();
        auto fps = settings->getFps();
        if (drawSetting("FPS",
                        "(Default and Recommended: 60) Higher number gives "
                        "smoothness but with high GPU consumption.",
                        fps,
                        m_availableFps)) {
            settings->setFps(fps);
        }

        ImGui::NewLine();
        auto showStatsWindow = settings->getShowStatsWindow();
        if (Widgets::CheckboxWithLabel("Show Stats Window", &showStatsWindow)) {
            settings->setShowStatsWindow(showStatsWindow);
        }
    }

    void SettingsWindow::drawViewportColorsSettings() {
        ImGui::TextWrapped(
            "Here you can customize the colors used in the viewport. This "
            "allows you to "
            "tailor the visual appearance of the viewport to your preferences, "
            "making it easier to work with different lighting conditions or to "
            "simply match your aesthetic preferences. Adjusting these colors "
            "can enhance visibility and reduce eye strain during long sessions "
            "of work in the viewport.");
        ImGui::NewLine();
    }

    void SettingsWindow::drawLeftPanel() {
        for (const auto &[category, callback] : m_settingsCallbacks) {
            if (ImGui::Selectable(category.c_str(),
                                  m_currentCategory == category)) {
                m_currentCategory = category;
            }
        }
    }

} // namespace Bess::UI
