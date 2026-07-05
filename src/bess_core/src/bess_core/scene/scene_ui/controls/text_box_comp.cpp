#include "bess_core/scene/scene_ui/controls/text_box_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include <algorithm>

namespace Bess::Canvas::UI {
    std::shared_ptr<TextBoxComp>
    TextBoxComp::create(const std::string &value,
                        const UITextBoxCallback &changedCallback) {
        auto textBox = std::make_shared<TextBoxComp>();
        textBox->setValue(value);
        textBox->setChangedCallback(changedCallback);
        return textBox;
    }

    void TextBoxComp::draw(SceneDrawContext &state) {
        const auto id = PickingId{
            .runtimeId = m_runtimeId,
            .info = 0,
        };

        const SceneWidgets::TextBoxOptions options{
            .placeholder = m_placeholder,
            .maxLength = m_maxLength,
            .fontSize = m_style.textStyle.fontSize,
            .padding = stylePadding(),
            .backgroundColor = m_style.backgroundColor,
            .hoverBackgroundColor = m_style.hoverColor,
            .focusedBackgroundColor = m_style.backgroundColor,
            .borderColor = m_style.borderColor,
            .focusedBorderColor = m_style.activeColor,
            .textColor = m_style.textStyle.textColor,
            .placeholderColor = m_style.textStyle.textColor.withAlpha(0.55f),
            .cursorColor = m_style.activeColor,
        };

        const auto result = SceneWidgets::textBox(id,
                                                  &m_value,
                                                  m_node->getDrawPos(),
                                                  m_node->getDrawSize(),
                                                  state,
                                                  options);

        m_focused = result.focused;
        if (result.changed) {
            makeUIDirty();
            if (m_changedCallback) {
                m_changedCallback(m_value);
            }
        }
        if (result.submitted && m_submittedCallback) {
            m_submittedCallback(m_value);
        }
        if (result.canceled && m_canceledCallback) {
            m_canceledCallback(m_value);
        }
    }

    void TextBoxComp::prepareUI(SceneUIPrepareCtx &state) {
        initNode(state.sceneState->getUINodeRegistry());
        prepStyle(state.theme);

        m_node->setSize(resolveBoxSize(state));
        m_node->setSizeUnit(Unit::pixel);
        m_node->setSizeConstraint(SizeContraint::fixed);
        m_node->setPadding(0.f);
        m_node->setMargin(m_style.metrics.margin);

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }
    }

    glm::vec2 TextBoxComp::resolveBoxSize(SceneUIPrepareCtx &state) const {
        auto size = m_textBoxSize;
        const auto padding = stylePadding();
        const auto fontProps = Core::Renderer::FontProps{
            .fontSize = m_style.textStyle.fontSize,
        };
        const auto referenceTextSize =
            state.renderer->measureText("M", fontProps);

        if (size.y == 0.f) {
            size.y = referenceTextSize.y + (padding.y * 2.f);
        }

        if (size.x == 0.f) {
            const auto &displayText = m_value.empty() ? m_placeholder : m_value;
            const auto measuredText =
                state.renderer->measureText(displayText, fontProps);
            size.x = std::max(48.f, measuredText.x + (padding.x * 2.f));
        }

        return size;
    }

    glm::vec2 TextBoxComp::stylePadding() const {
        const auto &padding = m_style.metrics.padding;
        return {
            std::max(padding.left, padding.right),
            std::max(padding.top, padding.bottom),
        };
    }
} // namespace Bess::Canvas::UI
