#include "bess_core/scene/scene_ui/controls/text_box_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_events.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>

namespace Bess::Canvas::UI {
    std::shared_ptr<TextBoxComp> TextBoxComp::create(const CompConfig &config) {
        return create("", nullptr, config);
    }

    std::shared_ptr<TextBoxComp>
    TextBoxComp::create(const std::string &value,
                        const UITextBoxCallback &changedCallback,
                        const CompConfig &config) {
        auto textBox = std::make_shared<TextBoxComp>();
        textBox->setValue(value);
        textBox->setChangedCallback(changedCallback);
        applyCompConfig(textBox, config);
        return textBox;
    }

    void TextBoxComp::update(TimeMs ts, SceneState &state) {
        if (m_clearFocus) {
            m_clearFocus = false;
            if (m_focused) {
                state.clearUIFocus();
            }
        }
    }

    void TextBoxComp::onDraw(SceneDrawContext &state) {
        const auto id = PickingId{
            .runtimeId = m_runtimeId,
            .info = 0,
        };

        m_textInput.syncExternalValue(m_value, m_maxLength);
        const auto padding = stylePadding();
        const TextBoxContextDrawOptions options{
            .placeholder = m_placeholder,
            .fontSize = m_style.textStyle.fontSize,
            .padding = padding,
            .backgroundColor = m_style.backgroundColor,
            .hoverBackgroundColor = m_style.hoverColor,
            .focusedBackgroundColor = m_style.backgroundColor,
            .borderColor = m_style.borderColor,
            .focusedBorderColor = m_style.activeColor,
            .textColor = m_style.textStyle.textColor,
            .placeholderColor = m_style.textStyle.textColor.withAlpha(0.55f),
            .selectionColor = m_style.activeColor.withAlpha(0.45f),
            .cursorColor = m_style.activeColor,
            .cursorHeight = m_node->getDrawSize().y - (padding.y * 2.f),
            .hovered = m_hovered,
        };

        drawTextBoxContext(id,
                           m_textInput,
                           m_node->getDrawPos(),
                           m_node->getDrawSize(),
                           state,
                           options);
    }

    void TextBoxComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        const auto size = resolveBoxSize(state);
        m_node->setWidth(size.x);
        m_node->setHeight(size.y);
        m_node->setPadding(0.f); // padding is resolved in resolveBoxSize
        m_node->setMargin(m_style.metrics.margin);
        applyCustomLayoutStyle();

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
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

    bool TextBoxComp::isFocusable() const {
        return true;
    }

    bool TextBoxComp::wantsKeyboardInput() const {
        return m_focused;
    }

    void TextBoxComp::onFocusGained(const Events::FocusEvent &e) {
        UISceneComponent::onFocusGained(e);
        m_textInput.focus(m_value, m_maxLength);
    }

    void TextBoxComp::onFocusLost(const Events::FocusEvent &e) {
        UISceneComponent::onFocusLost(e);
        m_textInput.blur();
    }

    bool TextBoxComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button != Events::MouseButton::left) {
            return false;
        }

        if (e.action == Events::MouseClickAction::press ||
            e.action == Events::MouseClickAction::doubleClick) {
            m_textInput.queuePointerPress(e.mousePos);
            return true;
        }

        if (e.action == Events::MouseClickAction::release) {
            m_textInput.queuePointerRelease(e.mousePos);
            return true;
        }

        return false;
    }

    bool TextBoxComp::onPointerMove(const Events::MouseMoveEvent &e) {
        m_textInput.queuePointerMove(e.mousePos);
        return true;
    }

    bool TextBoxComp::onKeyEvent(const SceneEvent &evt) {
        const auto result = m_textInput.handleEvent(evt);
        applyTextInputResult(result);
        return result.handled;
    }

    bool TextBoxComp::hasPointerCapture() const {
        return m_textInput.hasPointerCapture();
    }

    Core::Viewport::SceneCursor TextBoxComp::getCursor() const {
        return Core::Viewport::SceneCursor::text;
    }

    void TextBoxComp::applyTextInputResult(const TextBoxContextResult &result) {
        if (result.changed) {
            m_value = m_textInput.text();
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

        if (result.submitted || result.canceled) {
            m_clearFocus = true;
        }
    }
} // namespace Bess::Canvas::UI
