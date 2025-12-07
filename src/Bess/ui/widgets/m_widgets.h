#pragma once

#include "glm.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/icons/FontAwesomeIcons.h"
#include <format>
#include <string>
#include <vector>

namespace Bess::UI::Widgets {
    bool TextBox(const std::string &label, std::string &value, const std::string &hintText = "");

    template <class TValue>
    bool ComboBox(const std::string &label, TValue &currentValue, const std::vector<TValue> &predefinedValues) {
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

    bool CheckboxWithLabel(const char *label, bool *value);

    bool TreeNode(
        int key, // unique key to id the tree node uniquely in the UI
        const std::string &name,
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None,
        const std::string &icon = "",
        glm::vec4 iconColor = glm::vec4(-1.0f));

    bool ButtonWithPopup(const std::string &label, const std::string &popupName, bool showMenuButton = true);

    std::pair<bool, bool> EditableTreeNode(
        uint64_t key,
        std::string &name,
        bool &selected,
        ImGuiTreeNodeFlags treeFlags,
        const std::string &icon,
        glm::vec4 iconColor,
        const std::string &popupName,
        uint64_t payloadId);

} // namespace Bess::UI::Widgets
