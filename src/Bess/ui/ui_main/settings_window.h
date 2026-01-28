#pragma once

#include "imgui.h"
#include "ui/widgets/m_widgets.h"
#include <string>
#include <vector>
namespace Bess::UI {
    class SettingsWindow {
      public:
        static void hide();
        static void show();
        static void draw();

        static bool isShown();

      private:
        static void onFirstDraw();

        template <std::ranges::input_range Range, class TValue = std::ranges::range_value_t<Range>>
        static bool drawSetting(const std::string &label,
                                const std::string &hintText,
                                TValue &currentValue, Range &&predefinedValues) {
            ImGui::Text("%s", label.c_str());
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", hintText.c_str());
            ImGui::PopStyleColor();
            ImGui::Indent();
            auto changed = Widgets::ComboBox("##" + label, currentValue, std::forward<Range>(predefinedValues));
            ImGui::Unindent();
            return changed;
        }

        static bool m_shown, m_isFirstDraw;
        static std::vector<float> m_availableScales;
        static std::vector<float> m_availableFontSizes;
        static std::vector<std::string> m_availableThemes;
        static std::vector<int> m_availableFps;
    };
} // namespace Bess::UI
