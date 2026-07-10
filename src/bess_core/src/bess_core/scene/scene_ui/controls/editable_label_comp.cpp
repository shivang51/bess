#include "bess_core/scene/scene_ui/controls/editable_label_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_events.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>

namespace Bess::Canvas::UI {
    namespace {
        constexpr float kDefaultMinTextBoxWidth = 48.f;
        constexpr uint32_t kEditableLabelTextBoxInfo = 1u;
    } // namespace

    std::shared_ptr<EditableLabelComp>
    EditableLabelComp::create(const std::string &value,
                              const UIEditableLabelCallback &changedCallback) {
        auto label = std::make_shared<EditableLabelComp>();
        label->setName(value);
        label->setChangedCallback(changedCallback);
        return label;
    }

    void EditableLabelComp::onDraw(SceneDrawContext &state) {
        if (m_node == nullptr) {
            return;
        }

        if (!m_editing) {
            drawText(state, displayText(), m_node);
            return;
        }

        const auto id = textBoxPickingId();
        m_textInput.syncExternalValue(m_editValue, m_maxLength);
        const TextBoxContextDrawOptions options{
            .placeholder = m_placeholder,
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
            .hovered = m_hovered,
        };

        drawTextBoxContext(id,
                           m_textInput,
                           m_node->getDrawPos(),
                           m_node->getDrawSize(),
                           state,
                           options);
    }

    void EditableLabelComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        const auto fontProps = Core::Renderer::FontProps{
            .fontSize = m_style.textStyle.fontSize,
        };

        if (m_editing) {
            const auto size = resolveTextBoxSize(state);
            m_node->setWidth(size.x);
            m_node->setHeight(size.y);
            m_node->setPadding(0.f);
        } else {
            const auto size =
                state.renderer->measureText(displayText(), fontProps);
            m_node->setWidth(size.x);
            m_node->setHeight(size.y);
            m_node->setPadding(m_style.metrics.padding);
        }

        m_node->setMargin(m_style.metrics.margin);

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    bool EditableLabelComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button == Events::MouseButton::left &&
            e.action == Events::MouseClickAction::doubleClick) {
            beginEditAt(e.mousePos);
            if (e.sceneState != nullptr &&
                e.sceneState->isComponentValid(m_uuid)) {
                e.sceneState->focusUIComponent(m_uuid,
                                               {
                                                   .entityUuid = m_uuid,
                                                   .mousePos = e.mousePos,
                                                   .details = e.details,
                                                   .sceneState = e.sceneState,
                                               });
            }
            return true;
        }

        if (m_editing && e.button == Events::MouseButton::left) {
            if (e.action == Events::MouseClickAction::press) {
                m_textInput.queuePointerPress(e.mousePos);
                return true;
            }
            if (e.action == Events::MouseClickAction::release) {
                m_textInput.queuePointerRelease(e.mousePos);
                return true;
            }
            return true;
        }

        return false;
    }

    void EditableLabelComp::beginEdit() {
        beginEditAt(std::nullopt);
    }

    void EditableLabelComp::beginEditAt(std::optional<glm::vec2> focusPos) {
        if (m_editing) {
            return;
        }

        m_originalValue = m_name;
        m_editValue = m_name;
        m_editing = true;
        m_pendingTextBoxFocusPos = focusPos;
        makeUIDirty();
    }

    void EditableLabelComp::commitEdit() {
        finishEdit(true);
    }

    void EditableLabelComp::cancelEdit() {
        finishEdit(false);
    }

    std::string EditableLabelComp::displayText() const {
        return m_name.empty() ? m_placeholder : m_name;
    }

    glm::vec2
    EditableLabelComp::resolveTextBoxSize(SceneUIPrepareCtx &state) const {
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
            const auto &text =
                m_editValue.empty() ? m_placeholder : m_editValue;
            const auto measuredText =
                state.renderer->measureText(text, fontProps);
            size.x = std::max(kDefaultMinTextBoxWidth,
                              measuredText.x + (padding.x * 2.f));
        }

        return size;
    }

    glm::vec2 EditableLabelComp::stylePadding() const {
        const auto &padding = m_style.metrics.padding;
        return {
            std::max(padding.left, padding.right),
            std::max(padding.top, padding.bottom),
        };
    }

    PickingId EditableLabelComp::textBoxPickingId() const {
        return {
            .runtimeId = m_runtimeId,
            .info = kEditableLabelTextBoxInfo,
        };
    }

    void EditableLabelComp::finishEdit(bool commit) {
        if (!m_editing) {
            return;
        }

        const auto editedValue = m_editValue;
        const auto originalValue = m_originalValue;

        m_editing = false;
        m_pendingTextBoxFocusPos = std::nullopt;
        m_textInput.blur();
        m_editValue.clear();
        m_originalValue.clear();

        if (commit) {
            if (editedValue != m_name) {
                setName(editedValue);
                if (m_changedCallback) {
                    m_changedCallback(m_name);
                }
            } else {
                makeUIDirty();
            }

            if (m_submittedCallback) {
                m_submittedCallback(m_name);
            }
            return;
        }

        makeUIDirty();
        if (m_canceledCallback) {
            m_canceledCallback(originalValue);
        }
    }

    bool EditableLabelComp::isFocusable() const {
        return m_editing;
    }

    bool EditableLabelComp::wantsKeyboardInput() const {
        return m_editing && m_focused;
    }

    void EditableLabelComp::onFocusGained(const Events::FocusEvent &e) {
        UISceneComponent::onFocusGained(e);
        if (!m_editing) {
            return;
        }

        m_textInput.focus(m_editValue, m_maxLength, m_selectTextOnEdit);
        if (!m_selectTextOnEdit && m_pendingTextBoxFocusPos.has_value()) {
            m_textInput.queuePointerPress(*m_pendingTextBoxFocusPos);
            m_textInput.queuePointerRelease(*m_pendingTextBoxFocusPos);
        }
        m_pendingTextBoxFocusPos = std::nullopt;
    }

    void EditableLabelComp::onFocusLost(const Events::FocusEvent &e) {
        UISceneComponent::onFocusLost(e);
        if (m_editing) {
            commitEdit();
            return;
        }

        m_textInput.blur();
    }

    bool EditableLabelComp::onPointerMove(const Events::MouseMoveEvent &e) {
        if (!m_editing) {
            return false;
        }

        m_textInput.queuePointerMove(e.mousePos);
        return true;
    }

    bool EditableLabelComp::onKeyEvent(const SceneEvent &evt) {
        if (!m_editing) {
            return false;
        }

        const auto result = m_textInput.handleEvent(evt);
        if (result.changed) {
            m_editValue = m_textInput.text();
            makeUIDirty();
        }

        if (result.canceled) {
            cancelEdit();
            return result.handled;
        }

        if (result.submitted) {
            commitEdit();
            return result.handled;
        }

        return result.handled;
    }

    bool EditableLabelComp::hasPointerCapture() const {
        return m_textInput.hasPointerCapture();
    }

    Core::Viewport::SceneCursor EditableLabelComp::getCursor() const {
        return m_editing ? Core::Viewport::SceneCursor::text
                         : Core::Viewport::SceneCursor::pointer;
    }
} // namespace Bess::Canvas::UI
