#include "bess_core/scene/scene_ui/controls/text_box_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_events.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>
#include <string_view>

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

    namespace {
        constexpr float kPlaceHolderTextOpacity = 0.7f;

        glm::vec2 xy(const glm::vec3 &value) {
            return {value.x, value.y};
        }

    } // namespace

    void TextBoxComp::drawBackground(SceneDrawContext &state) {
        Core::Renderer::QuadProps props;
        props.position = m_node->getDrawPos();
        props.size = m_node->getDrawSize();
        props.zIndex = m_node->getDrawPos().z;

        props.color = m_style.backgroundColor;

        if (m_hovered) {
            props.color = m_style.hoverColor;
        }

        props.id = PickingId{
            .runtimeId = m_runtimeId,
            .info = 0,
        };
        props.borderColor = m_textInput.isFocused()
                                ? m_style.activeColor.withAlpha(0.6)
                                : m_style.borderColor;
        props.radius = m_style.metrics.borderRadius;
        props.thickness = m_style.metrics.borderSize.toVec4();

        state.renderer->drawQuad(props);
    }

    void TextBoxComp::drawText(SceneDrawContext &state) {
        const auto &boxPos = m_node->getDrawPos();
        const auto &boxSize = m_node->getDrawSize();
        const auto paddingX = stylePadding().x;

        const float left = boxPos.x - (boxSize.x * 0.5f) + paddingX;

        Core::Renderer::FontProps props;
        props.fontSize = m_style.textStyle.fontSize;
        props.position = {left, boxPos.y};
        props.zIndex = boxPos.z + 0.0002f;
        props.id = PickingId{
            .runtimeId = m_runtimeId,
            .info = 0,
        };
        props.color =
            m_value.empty()
                ? m_style.textStyle.textColor.withAlpha(kPlaceHolderTextOpacity)
                : m_style.textStyle.textColor;

        std::string_view visibleText;

        if (m_textInput.text().empty()) {
            visibleText = m_placeholder;
        } else {
            const float contentWidth =
                std::max(1.f, boxSize.x - (paddingX * 2.f));

            const auto [visibleStart, visibleEnd] =
                m_textInput.visibleRangeForCursor(
                    state.renderer, contentWidth, props);

            visibleText = std::string_view(m_textInput.text())
                              .substr(visibleStart, visibleEnd - visibleStart);
        }

        const auto offsetY =
            state.renderer->textCenterOffsetY(visibleText, props);
        props.position.y += offsetY;

        state.renderer->drawFont(visibleText, props);
    }

    void TextBoxComp::drawCursor(SceneDrawContext &state) {
        const auto &boxPos = m_node->getDrawPos();
        const auto &boxSize = m_node->getDrawSize();
        const auto paddingX = stylePadding().x;

        const float left = boxPos.x - (boxSize.x * 0.5f) + paddingX;

        const float contentWidth = std::max(1.f, boxSize.x - (paddingX * 2.f));

        const auto fontProps = Core::Renderer::FontProps{
            .fontSize = m_style.textStyle.fontSize,
        };

        m_textInput.updatePointerSelection(
            state.renderer, left, contentWidth, fontProps);

        const auto [visibleStart, visibleEnd] =
            m_textInput.visibleRangeForCursor(
                state.renderer, contentWidth, fontProps);

        std::string_view visibleText =
            std::string_view(m_textInput.text())
                .substr(visibleStart, visibleEnd - visibleStart);

        const size_t visibleCursor =
            std::clamp(m_textInput.cursorPos(), visibleStart, visibleEnd);
        const auto preCursorText =
            visibleText.substr(0, visibleCursor - visibleStart);

        const auto textSize =
            state.renderer->measureText(preCursorText, fontProps);

        Core::Renderer::QuadProps props;
        props.position = {left + textSize.x, boxPos.y};
        props.size = {kTextBoxCursorWidth, boxSize.y};
        props.zIndex = boxPos.z + 0.0005f;
        props.color = m_style.activeColor;

        state.renderer->drawQuad(props);
    }

    void TextBoxComp::drawSel(SceneDrawContext &state) {
        const auto &boxPos = m_node->getDrawPos();
        const auto &boxSize = m_node->getDrawSize();
        const auto padding = stylePadding();

        const float left = boxPos.x - (boxSize.x * 0.5f) + padding.x;

        const glm::vec2 contentSize =
            glm::max(glm::vec2(1.f), boxSize - (padding * 2.f));

        const auto [selStart, selEnd] = m_textInput.selectionRange();

        const auto fontProps = Core::Renderer::FontProps{
            .fontSize = m_style.textStyle.fontSize,
        };

        const auto [visibleStart, visibleEnd] =
            m_textInput.visibleRangeForCursor(
                state.renderer, contentSize.x, fontProps);

        const size_t visibleSelStart = std::max(selStart, visibleStart);
        const size_t visibleSelEnd = std::min(selEnd, visibleEnd);

        if (visibleSelStart > visibleSelEnd) {
            return;
        }

        const auto text = std::string_view(m_textInput.text());
        const auto prefix =
            text.substr(visibleStart, visibleSelStart - visibleStart);
        const auto selected =
            text.substr(visibleSelStart, visibleSelEnd - visibleSelStart);
        const float selectionX =
            left + state.renderer->measureText(prefix, fontProps).x;
        const float selectionWidth =
            std::max(1.f, state.renderer->measureText(selected, fontProps).x);

        Core::Renderer::QuadProps props;
        props.position = {selectionX + (selectionWidth * 0.5f), boxPos.y};
        props.size = {selectionWidth, contentSize.y};
        props.zIndex = boxPos.z + 0.0003f;
        props.color = m_style.activeColor.withAlpha(0.5f);
        props.radius = glm::vec4(2.f);

        state.renderer->drawQuad(props);
    }

    void TextBoxComp::onDraw(SceneDrawContext &state) {
        m_textInput.syncExternalValue(m_value, m_maxLength);

        drawBackground(state);
        drawText(state);

        if (m_textInput.isFocused()) {
            drawCursor(state);
        }

        if (m_textInput.hasSelection()) {
            drawSel(state);
        }
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
            state.renderer->measureText("MyiJjg?|", fontProps);

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
