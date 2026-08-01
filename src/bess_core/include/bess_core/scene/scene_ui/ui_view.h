#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/scene_ui/controls/button_comp.h"
#include "bess_core/scene/scene_ui/controls/checkbox_comp.h"
#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_ui/controls/context_menu_comp.h"
#include "bess_core/scene/scene_ui/controls/dropdown_comp.h"
#include "bess_core/scene/scene_ui/controls/editable_label_comp.h"
#include "bess_core/scene/scene_ui/controls/float_text_box_comp.h"
#include "bess_core/scene/scene_ui/controls/image_comp.h"
#include "bess_core/scene/scene_ui/controls/int_text_box_comp.h"
#include "bess_core/scene/scene_ui/controls/label_comp.h"
#include "bess_core/scene/scene_ui/controls/list_box_comp.h"
#include "bess_core/scene/scene_ui/controls/panel_comp.h"
#include "bess_core/scene/scene_ui/controls/progress_bar_comp.h"
#include "bess_core/scene/scene_ui/controls/scalar_input_comp.h"
#include "bess_core/scene/scene_ui/controls/segmented_button_comp.h"
#include "bess_core/scene/scene_ui/controls/selectable_button_comp.h"
#include "bess_core/scene/scene_ui/controls/slider_comp.h"
#include "bess_core/scene/scene_ui/controls/spacer_comp.h"
#include "bess_core/scene/scene_ui/controls/text_box_comp.h"
#include "bess_core/scene/scene_ui/controls/toggle_btn_comp.h"
#include "bess_core/scene/scene_ui/controls/tree_node_comp.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Bess::Canvas::UI {

    class BESS_API View {
      public:
        explicit View(SceneState &sceneState) noexcept
            : m_sceneState(&sceneState) {
        }

        explicit View(SceneState *sceneState) : m_sceneState(sceneState) {
            BESS_ASSERT(m_sceneState != nullptr,
                        "UI::View requires a valid SceneState.");
        }

        [[nodiscard]] SceneState *getSceneState() const noexcept {
            return m_sceneState;
        }

        [[nodiscard]] CompConfig
        config(CompConfig config = CompConfig{}) const {
            return bindConfig(std::move(config));
        }

        [[nodiscard]] CompConfig styled(UIElementStyle style) const {
            return config(styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ContainerComp>
        container(CompConfig config = CompConfig{}) const {
            return ContainerComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ContainerComp>
        container(UIElementStyle style) const {
            return container(styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ContainerComp>
        container(const LayoutDirection &direction,
                  CompConfig config = CompConfig{}) const {
            return ContainerComp::create(direction,
                                         bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ContainerComp>
        container(const LayoutDirection &direction,
                  UIElementStyle style) const {
            return container(direction, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ContainerComp>
        row(CompConfig config = CompConfig{}) const {
            return container(LayoutDirection::horizontal, std::move(config));
        }

        [[nodiscard]] std::shared_ptr<ContainerComp>
        row(UIElementStyle style) const {
            return row(styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ContainerComp>
        column(CompConfig config = CompConfig{}) const {
            return container(LayoutDirection::vertical, std::move(config));
        }

        [[nodiscard]] std::shared_ptr<ContainerComp>
        column(UIElementStyle style) const {
            return column(styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<SpacerComp>
        spacer(CompConfig config = CompConfig{}) const {
            return SpacerComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<SpacerComp>
        spacer(UIElementStyle style) const {
            return spacer(styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<SpacerComp>
        spacer(float grow, CompConfig config = CompConfig{}) const {
            return SpacerComp::create(grow, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<SpacerComp>
        spacer(float grow, UIElementStyle style) const {
            return spacer(grow, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<SpacerComp>
        fixedSpacer(float size, CompConfig config = CompConfig{}) const {
            return SpacerComp::createFixed(size, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<SpacerComp>
        fixedSpacer(float size, UIElementStyle style) const {
            return fixedSpacer(size, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<LabelComp>
        label(const std::string &text = "",
              CompConfig config = CompConfig{}) const {
            return LabelComp::create(text, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<LabelComp>
        label(const std::string &text, UIElementStyle style) const {
            return label(text, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ButtonComp>
        button(CompConfig config, const UIButtonCallback &cb) const {
            auto btn = ButtonComp::create(bindConfig(std::move(config)));
            btn->setCallback(cb);
            return btn;
        }

        [[nodiscard]] std::shared_ptr<ButtonComp>
        button(CompConfig config) const {
            return ButtonComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ButtonComp>
        button(const std::string &label = "",
               const UIButtonCallback &callback = nullptr,
               CompConfig config = CompConfig{}) const {
            return ButtonComp::create(
                label, callback, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ButtonComp>
        button(const std::string &label, UIElementStyle style) const {
            return button(label, nullptr, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ButtonComp>
        button(const std::string &label,
               const UIButtonCallback &callback,
               UIElementStyle style) const {
            return button(label, callback, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<CheckboxComp>
        checkbox(CompConfig config) const {
            return CheckboxComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<CheckboxComp>
        checkbox(const std::string &label = "",
                 const UICheckboxCallback &callback = nullptr,
                 bool checked = false,
                 CompConfig config = CompConfig{}) const {
            return CheckboxComp::create(
                label, callback, checked, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<CheckboxComp>
        checkbox(const std::string &label, UIElementStyle style) const {
            return checkbox(
                label, nullptr, false, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<CheckboxComp>
        checkbox(const std::string &label,
                 const UICheckboxCallback &callback,
                 bool checked,
                 UIElementStyle style) const {
            return checkbox(
                label, callback, checked, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ContextMenuComp>
        contextMenu(CompConfig config) const {
            return ContextMenuComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ContextMenuComp>
        contextMenu(const std::vector<UIContextMenuItem> &items = {},
                    const std::string &triggerLabel = "Right click",
                    CompConfig config = CompConfig{}) const {
            return ContextMenuComp::create(
                items, triggerLabel, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ContextMenuComp>
        contextMenu(const std::vector<UIContextMenuItem> &items,
                    const std::string &triggerLabel,
                    UIElementStyle style) const {
            return contextMenu(
                items, triggerLabel, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<DropdownComp>
        dropdown(CompConfig config) const {
            return DropdownComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<DropdownComp>
        dropdown(const std::vector<UIDropdownOption> &options = {},
                 size_t selectedIndex = 0,
                 const UIDropdownCallback &changedCallback = nullptr,
                 CompConfig config = CompConfig{}) const {
            return DropdownComp::create(options,
                                        selectedIndex,
                                        changedCallback,
                                        bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<DropdownComp>
        dropdown(const std::vector<UIDropdownOption> &options,
                 size_t selectedIndex,
                 const UIDropdownCallback &changedCallback,
                 UIElementStyle style) const {
            return dropdown(options,
                            selectedIndex,
                            changedCallback,
                            styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<EditableLabelComp>
        editableLabel(CompConfig config) const {
            return EditableLabelComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<EditableLabelComp>
        editableLabel(const std::string &value = "",
                      const UIEditableLabelCallback &changedCallback = nullptr,
                      CompConfig config = CompConfig{}) const {
            return EditableLabelComp::create(
                value, changedCallback, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<EditableLabelComp>
        editableLabel(const std::string &value, UIElementStyle style) const {
            return editableLabel(value, nullptr, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<EditableLabelComp>
        editableLabel(const std::string &value,
                      const UIEditableLabelCallback &changedCallback,
                      UIElementStyle style) const {
            return editableLabel(
                value, changedCallback, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ImageComp>
        image(CompConfig config = CompConfig{}) const {
            return ImageComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ImageComp>
        image(UIElementStyle style) const {
            return image(styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ImageComp>
        image(const glm::vec2 &imageSize,
              CompConfig config = CompConfig{}) const {
            return ImageComp::create(imageSize, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ImageComp>
        image(const glm::vec2 &imageSize, UIElementStyle style) const {
            return image(imageSize, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ImageComp>
        image(const std::shared_ptr<Core::Renderer::ITexture> &texture,
              const glm::vec2 &imageSize = {0.f, 0.f},
              CompConfig config = CompConfig{}) const {
            return ImageComp::create(
                texture, imageSize, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ImageComp>
        image(const std::shared_ptr<Core::Renderer::ITexture> &texture,
              const glm::vec2 &imageSize,
              UIElementStyle style) const {
            return image(texture, imageSize, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ImageComp>
        image(Core::Renderer::TextureHandle texture,
              const glm::vec2 &sourceSize,
              const glm::vec2 &imageSize = {0.f, 0.f},
              CompConfig config = CompConfig{}) const {
            return ImageComp::create(
                texture, sourceSize, imageSize, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ImageComp>
        image(Core::Renderer::TextureHandle texture,
              const glm::vec2 &sourceSize,
              const glm::vec2 &imageSize,
              UIElementStyle style) const {
            return image(
                texture, sourceSize, imageSize, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ListBoxComp>
        listBox(CompConfig config) const {
            return ListBoxComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ListBoxComp>
        listBox(const std::vector<UIListBoxItem> &items = {},
                size_t selectedIndex = ListBoxComp::noSelection,
                const UIListBoxCallback &changedCallback = nullptr,
                CompConfig config = CompConfig{}) const {
            return ListBoxComp::create(items,
                                       selectedIndex,
                                       changedCallback,
                                       bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ListBoxComp>
        listBox(const std::vector<UIListBoxItem> &items,
                size_t selectedIndex,
                const UIListBoxCallback &changedCallback,
                UIElementStyle style) const {
            return listBox(items,
                           selectedIndex,
                           changedCallback,
                           styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<PanelComp>
        panel(CompConfig config = CompConfig{}) const {
            return PanelComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<PanelComp>
        panel(UIElementStyle style) const {
            return panel(styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<PanelComp>
        panel(const std::string &title,
              CompConfig config = CompConfig{}) const {
            return PanelComp::create(title, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<PanelComp>
        panel(const std::string &title, UIElementStyle style) const {
            return panel(title, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<PanelComp>
        panel(const std::string &title,
              const glm::vec2 &panelSize,
              CompConfig config = CompConfig{}) const {
            return PanelComp::create(
                title, panelSize, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<PanelComp>
        panel(const std::string &title,
              const glm::vec2 &panelSize,
              UIElementStyle style) const {
            return panel(title, panelSize, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ProgressBarComp>
        progressBar(CompConfig config) const {
            return ProgressBarComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ProgressBarComp>
        progressBar(UIElementStyle style) const {
            return progressBar(styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ProgressBarComp>
        progressBar(const std::string &label,
                    float value,
                    float minValue = 0.f,
                    float maxValue = 1.f,
                    CompConfig config = CompConfig{}) const {
            return ProgressBarComp::create(label,
                                           value,
                                           minValue,
                                           maxValue,
                                           bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ProgressBarComp>
        progressBar(const std::string &label,
                    float value,
                    float minValue,
                    float maxValue,
                    UIElementStyle style) const {
            return progressBar(label,
                               value,
                               minValue,
                               maxValue,
                               styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<FloatTextBoxComp>
        floatTextBox(CompConfig config) const {
            return FloatTextBoxComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<FloatTextBoxComp>
        floatTextBox(float value = 0.f,
                     const UIFloatTextBoxCallback &changedCallback = nullptr,
                     CompConfig config = CompConfig{}) const {
            return FloatTextBoxComp::create(
                value, changedCallback, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<FloatTextBoxComp>
        floatTextBox(float value, UIElementStyle style) const {
            return floatTextBox(value, nullptr, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<FloatTextBoxComp>
        floatTextBox(float value,
                     const UIFloatTextBoxCallback &changedCallback,
                     UIElementStyle style) const {
            return floatTextBox(
                value, changedCallback, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<IntTextBoxComp>
        intTextBox(CompConfig config) const {
            return IntTextBoxComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<IntTextBoxComp>
        intTextBox(int value = 0,
                   const UIIntTextBoxCallback &changedCallback = nullptr,
                   CompConfig config = CompConfig{}) const {
            return IntTextBoxComp::create(
                value, changedCallback, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<IntTextBoxComp>
        intTextBox(int value, UIElementStyle style) const {
            return intTextBox(value, nullptr, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<IntTextBoxComp>
        intTextBox(int value,
                   const UIIntTextBoxCallback &changedCallback,
                   UIElementStyle style) const {
            return intTextBox(
                value, changedCallback, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ScalarInputComp>
        scalarInput(CompConfig config) const {
            return ScalarInputComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ScalarInputComp>
        scalarInput(double value = 0.0,
                    const UIScalarInputCallback &changedCallback = nullptr,
                    CompConfig config = CompConfig{}) const {
            return ScalarInputComp::create(
                value, changedCallback, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ScalarInputComp>
        scalarInput(double value, UIElementStyle style) const {
            return scalarInput(value, nullptr, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ScalarInputComp>
        scalarInput(double value,
                    const UIScalarInputCallback &changedCallback,
                    UIElementStyle style) const {
            return scalarInput(
                value, changedCallback, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<SegmentedButtonComp>
        segmentedButton(CompConfig config) const {
            return SegmentedButtonComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<SegmentedButtonComp> segmentedButton(
            const std::vector<UISegmentedButtonOption> &options = {},
            size_t selectedIndex = 0,
            const UISegmentedButtonCallback &callback = nullptr,
            CompConfig config = CompConfig{}) const {
            return SegmentedButtonComp::create(options,
                                               selectedIndex,
                                               callback,
                                               bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<SegmentedButtonComp>
        segmentedButton(const std::vector<UISegmentedButtonOption> &options,
                        size_t selectedIndex,
                        const UISegmentedButtonCallback &callback,
                        UIElementStyle style) const {
            return segmentedButton(options,
                                   selectedIndex,
                                   callback,
                                   styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<SelectableButtonComp>
        selectableButton(CompConfig config) const {
            return SelectableButtonComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<SelectableButtonComp>
        selectableButton(const std::string &label = "",
                         const UISelectableButtonCallback &callback = nullptr,
                         bool selected = false,
                         CompConfig config = CompConfig{}) const {
            return SelectableButtonComp::create(
                label, callback, selected, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<SelectableButtonComp>
        selectableButton(const std::string &label, UIElementStyle style) const {
            return selectableButton(
                label, nullptr, false, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<SelectableButtonComp>
        selectableButton(const std::string &label,
                         const UISelectableButtonCallback &callback,
                         bool selected,
                         UIElementStyle style) const {
            return selectableButton(
                label, callback, selected, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<SliderComp>
        slider(CompConfig config) const {
            return SliderComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<SliderComp>
        slider(const std::string &label,
               float value,
               float minValue,
               float maxValue,
               const UISliderCallback &changedCallback = nullptr,
               CompConfig config = CompConfig{}) const {
            return SliderComp::create(label,
                                      value,
                                      minValue,
                                      maxValue,
                                      changedCallback,
                                      bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<SliderComp>
        slider(const std::string &label,
               float value,
               float minValue,
               float maxValue,
               const UISliderCallback &changedCallback,
               UIElementStyle style) const {
            return slider(label,
                          value,
                          minValue,
                          maxValue,
                          changedCallback,
                          styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<TextBoxComp>
        textBox(CompConfig config) const {
            return TextBoxComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<TextBoxComp>
        textBox(const std::string &value = "",
                const UITextBoxCallback &changedCallback = nullptr,
                CompConfig config = CompConfig{}) const {
            return TextBoxComp::create(
                value, changedCallback, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<TextBoxComp>
        textBox(const std::string &value, UIElementStyle style) const {
            return textBox(value, nullptr, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<TextBoxComp>
        textBox(const std::string &value,
                const UITextBoxCallback &changedCallback,
                UIElementStyle style) const {
            return textBox(
                value, changedCallback, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ToggleBtnComp>
        toggleButton(CompConfig config) const {
            return ToggleBtnComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ToggleBtnComp>
        toggleButton(const std::string &label = "",
                     const ToggleBtnCallback &callback = nullptr,
                     bool toggled = false,
                     CompConfig config = CompConfig{}) const {
            return ToggleBtnComp::create(
                label, callback, toggled, bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<ToggleBtnComp>
        toggleButton(const std::string &label, UIElementStyle style) const {
            return toggleButton(
                label, nullptr, false, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ToggleBtnComp>
        toggleButton(const std::string &label,
                     const ToggleBtnCallback &callback,
                     bool toggled,
                     UIElementStyle style) const {
            return toggleButton(
                label, callback, toggled, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<ToggleBtnComp>
        toggleBtn(const std::string &label = "",
                  const ToggleBtnCallback &callback = nullptr,
                  bool toggled = false,
                  CompConfig config = CompConfig{}) const {
            return toggleButton(label, callback, toggled, std::move(config));
        }

        [[nodiscard]] std::shared_ptr<TreeNodeComp>
        treeNode(CompConfig config) const {
            return TreeNodeComp::create(bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<TreeNodeComp>
        treeNode(const std::string &label = "",
                 bool expanded = true,
                 const UITreeNodeCallback &toggledCallback = nullptr,
                 CompConfig config = CompConfig{}) const {
            return TreeNodeComp::create(label,
                                        expanded,
                                        toggledCallback,
                                        bindConfig(std::move(config)));
        }

        [[nodiscard]] std::shared_ptr<TreeNodeComp>
        treeNode(const std::string &label, UIElementStyle style) const {
            return treeNode(
                label, true, nullptr, styleConfig(std::move(style)));
        }

        [[nodiscard]] std::shared_ptr<TreeNodeComp>
        treeNode(const std::string &label,
                 bool expanded,
                 const UITreeNodeCallback &toggledCallback,
                 UIElementStyle style) const {
            return treeNode(label,
                            expanded,
                            toggledCallback,
                            styleConfig(std::move(style)));
        }

      private:
        [[nodiscard]] CompConfig bindConfig(CompConfig config) const {
            BESS_ASSERT(m_sceneState != nullptr,
                        "UI::View requires a valid SceneState.");
            if (config.sceneState != nullptr) {
                BESS_ASSERT(config.sceneState == m_sceneState,
                            "UI::View cannot use a config from another scene.");
            }
            config.sceneState = m_sceneState;
            return config;
        }

        [[nodiscard]] static CompConfig styleConfig(UIElementStyle style) {
            CompConfig config;
            config.style = std::move(style);
            return config;
        }

        SceneState *m_sceneState = nullptr;
    };
} // namespace Bess::Canvas::UI
