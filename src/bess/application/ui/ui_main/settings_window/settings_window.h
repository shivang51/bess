#pragma once

#include "common/bess_api.h"

#include "imgui.h"
#include "ui/ui_panel.h"
#include "ui/widgets/m_widgets.h"
#include <string>
#include <vector>

namespace Bess::UI {
    class BESS_API SettingsWindow : public Panel {

      public:
        SettingsWindow();

      private:
        void onBeforeDraw() override;
        void onDraw() override;
        void onShow() override;

        void drawLeftPanel();
        void drawRightPanel();

        void drawGeneralSettings();
        void drawViewportColorsSettings();
        void drawPluginSettings();

        template <std::ranges::input_range Range,
                  class TValue = std::ranges::range_value_t<Range>>
        static bool drawSetting(const std::string &label,
                                const std::string &hintText,
                                TValue &currentValue,
                                Range &&predefinedValues) {
            ImGui::Text("%s", label.c_str());
            ImGui::SameLine();
            ImGui::PushStyleColor(
                ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", hintText.c_str());
            ImGui::PopStyleColor();
            ImGui::Indent();
            auto changed =
                Widgets::ComboBox("##" + label,
                                  currentValue,
                                  std::forward<Range>(predefinedValues));
            ImGui::Unindent();
            return changed;
        }

        std::vector<float> m_availableScales;
        std::vector<float> m_availableFontSizes;
        std::vector<std::string> m_availableThemes;
        std::vector<int> m_availableFps;

        // Map of Category name and draw function callbacks
        // General -> drawGeneralSettings
        // ViewportColors -> drawViewportColorsSettings
        std::map<std::string, std::function<void()>> m_settingsCallbacks;

        std::string m_currentCategory;
    };
} // namespace Bess::UI
