#include "ui/ui_main/settings_window/settings_window.h"
#include "bess_core/g_app_context.h"
#include "imgui.h"

#include "settings/settings.h"
#include "settings/viewport_theme.h"
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
        m_visible = true;
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
        // Scene Colors
        {
            auto &sceneColors = ViewportTheme::colors;
            ImGui::Text("Scene Colors");

            ImGui::Indent();
            ImGui::ColorEdit4("Text Color", sceneColors.text.data());

            ImGui::ColorEdit4("Compnent Header Default",
                              sceneColors.compHeader.data());
            ImGui::ColorEdit4("Component Background",
                              sceneColors.componentBG.data());
            ImGui::ColorEdit4("Component Border",
                              sceneColors.componentBorder.data());
            ImGui::ColorEdit4("Selected Component",
                              sceneColors.selectedComp.data());
            ImGui::ColorEdit4("Module Color", sceneColors.moduleColor.data());

            ImGui::ColorEdit4("State High", sceneColors.stateHigh.data());
            ImGui::ColorEdit4("State Low", sceneColors.stateLow.data());
            ImGui::ColorEdit4("State High Z", sceneColors.stateHighZ.data());
            ImGui::ColorEdit4("State Unknow", sceneColors.stateUnknow.data());

            ImGui::ColorEdit4("Wire Color", sceneColors.wire.data());
            ImGui::ColorEdit4("Ghost Wire Color", sceneColors.ghostWire.data());
            ImGui::ColorEdit4("Selected Wire Color",
                              sceneColors.selectedWire.data());
            ImGui::ColorEdit4("Clock Connection Low",
                              sceneColors.clockConnectionLow.data());
            ImGui::ColorEdit4("Clock Connection High",
                              sceneColors.clockConnectionHigh.data());

            ImGui::ColorEdit4("Selection Box Border",
                              sceneColors.selectionBoxBorder.data());
            ImGui::ColorEdit4("Selection Box Fill",
                              sceneColors.selectionBoxFill.data());

            ImGui::ColorEdit4("Grid Minor Color",
                              sceneColors.gridMinorColor.data());
            ImGui::ColorEdit4("Grid Major Color",
                              sceneColors.gridMajorColor.data());
            ImGui::ColorEdit4("Grid Axis X Color",
                              sceneColors.gridAxisXColor.data());
            ImGui::ColorEdit4("Grid Axis Y Color",
                              sceneColors.gridAxisYColor.data());
            ImGui::ColorEdit4("Error Color", sceneColors.error.data());

            ImGui::Unindent();
        }

        // Schematic View Colors
        {
            auto &schematicColors = ViewportTheme::schematicViewColors;

            ImGui::Text("Schematic View Colors");

            ImGui::Indent();
            ImGui::ColorEdit4("Pin Color", schematicColors.pin.data());
            ImGui::ColorEdit4("Text Color", schematicColors.text.data());
            ImGui::ColorEdit4("Connection Color",
                              schematicColors.connection.data());
            ImGui::ColorEdit4("Component Fill Color",
                              schematicColors.componentFill.data());
            ImGui::ColorEdit4("Component Stroke Color",
                              schematicColors.componentStroke.data());
            ImGui::ColorEdit4("Active Signal Color",
                              schematicColors.activeSignal.data());
            ImGui::Unindent();
        }

        // Widget Colors
        {
            auto &widgetColors = ViewportTheme::sceneWidgetsColors;

            ImGui::Text("Scene Widgets Colors");

            ImGui::Indent();
            ImGui::ColorEdit4("Surface Color", widgetColors.surface.data());
            ImGui::ColorEdit4("Surface Hover Color",
                              widgetColors.surfaceHover.data());
            ImGui::ColorEdit4("Surface Active Color",
                              widgetColors.surfaceActive.data());
            ImGui::ColorEdit4("Popup Surface Color",
                              widgetColors.popupSurface.data());

            ImGui::ColorEdit4("Border Color", widgetColors.border.data());
            ImGui::ColorEdit4("Border Focus Color",
                              widgetColors.borderFocus.data());

            ImGui::ColorEdit4("Text Color", widgetColors.text.data());
            ImGui::ColorEdit4("Text Muted Color",
                              widgetColors.textMuted.data());

            ImGui::ColorEdit4("Accent Color", widgetColors.accent.data());
            ImGui::ColorEdit4("Accent Strong Color",
                              widgetColors.accentStrong.data());
            ImGui::ColorEdit4("Item Hover Color",
                              widgetColors.itemHover.data());
            ImGui::ColorEdit4("Track Color", widgetColors.track.data());
            ImGui::ColorEdit4("Knob Color", widgetColors.knob.data());

            ImGui::Unindent();
        }
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
