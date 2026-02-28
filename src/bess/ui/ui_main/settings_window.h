#pragma once

#include "imgui.h"
#include "ui/widgets/m_widgets.h"
#include "ui_panel.h"
#include <string>
#include <vector>

namespace Bess::UI {
    class SettingsWindow : public Panel {

      public:
        SettingsWindow();

      private:
        void onBeforeDraw() override;
        void onDraw() override;
        void onShow() override;

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

        std::vector<float> m_availableScales;
        std::vector<float> m_availableFontSizes;
        std::vector<std::string> m_availableThemes;
        std::vector<int> m_availableFps;
    };
} // namespace Bess::UI
