#pragma once

#include "imgui.h"
#include <format>
#include <string>
#include <vector>

namespace Bess::UI {
    class MWidgets {
      public:
        static bool TextBox(const std::string &label, std::string &value, const std::string &hintText = "");

        template <class TValue>
        static bool ComboBox(const std::string &label, TValue &currentValue, const std::vector<TValue> &predefinedValues) {
            bool valueChanged = false;

            if (ImGui::BeginCombo(label.c_str(), std::format("{}", currentValue).c_str())) {
                for (auto &value : predefinedValues) {
                    bool isSelected = (currentValue == value);
                    if (ImGui::Selectable(std::format("{}", value).c_str(), isSelected)) {
                        currentValue = value;
                        valueChanged = true;
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            return valueChanged;
        }

        static bool CheckboxWithLabel(const char *label, bool *value) {
            ImGui::Text("%s", label);
            auto style = ImGui::GetStyle();
            float availWidth = ImGui::GetContentRegionAvail().x;
            ImGui::SameLine();
            float checkboxWidth = ImGui::CalcTextSize("X").x + style.FramePadding.x;
            ImGui::SetCursorPosX(availWidth - checkboxWidth);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            bool changed = ImGui::Checkbox(("##" + std::string(label)).c_str(), value);
            ImGui::PopStyleVar();
            return changed;
        }
    };
} // namespace Bess::UI
