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
    };
} // namespace Bess::UI
