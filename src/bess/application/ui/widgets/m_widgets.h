#pragma once

#include "common/bess_api.h"

#include "glm.hpp"
#include "imgui.h"
#include <format>
#include <ranges>
#include <string>

namespace Bess::UI::Widgets {

    BESS_API void SelectableText(const std::string &id,
                                 const std::string &text,
                                 const glm::vec2 &size = glm::vec2(0, 800));

    BESS_API bool TextBox(const std::string &label,
                          std::string &value,
                          const std::string &hintText = "");

    BESS_API bool TextBoxMultiline(const std::string &label,
                                   std::string &value,
                                   const glm::vec2 &size = glm::vec2(0, 800));

    template <typename T> auto UnpackValue(const T &item) {
        if constexpr (std::is_pointer_v<T>) {
            if (item == nullptr) {
                return std::string("(null)");
            }
            return std::format("{}", *item);
        } else {
            return std::format("{}", item);
        }
    }

    template <std::ranges::input_range Range,
              class TValue = std::ranges::range_value_t<Range>>
    bool ComboBox(const std::string &label,
                  TValue &currentValue,
                  Range &&predefinedValues) {
        bool valueChanged = false;

        if (ImGui::BeginCombo(label.c_str(),
                              UnpackValue(currentValue).c_str())) {

            for (auto &&value : std::forward<Range>(predefinedValues)) {
                bool isSelected = (currentValue == value);

                if (ImGui::Selectable(
                        std::format("{}", UnpackValue(value)).c_str(),
                        isSelected)) {
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

    BESS_API bool CheckboxWithLabel(const char *label,
                                    bool *value,
                                    bool expandToFullWidth = true,
                                    bool alignToFramePadding = false);

    BESS_API bool TreeNode(int key,
                           const std::string &name,
                           ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None,
                           const std::string &icon = "",
                           glm::vec4 iconColor = glm::vec4(-1.0f));

    BESS_API bool ButtonWithPopup(const std::string &label,
                                  const std::string &popupName,
                                  bool showMenuButton = true);

    BESS_API std::pair<bool, bool>
    EditableTreeNode(uint64_t key,
                     std::string &name,
                     bool selected,
                     ImGuiTreeNodeFlags treeFlags,
                     const std::string &icon,
                     glm::vec4 iconColor,
                     const std::string &popupName,
                     uint64_t payloadId);

} // namespace Bess::UI::Widgets
