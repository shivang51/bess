#include "behaviors/pressable.h"

namespace Bess::UI {
    PressableResult Pressable::handle(WidgetEventContext &context,
                                      const UIEvent &event) {
        PressableResult result;
        if (context.phase != UIEventPhase::target) {
            return result;
        }

        auto changed = [this, &result](bool &member, bool value) {
            if (member != value) {
                member = value;
                result.stateChanged = true;
            }
        };

        if (!context.enabled) {
            const bool hadState =
                m_hovered || m_pointerPressed || m_keyboardPressed;
            reset();
            result.stateChanged = hadState;
            if (hadState) {
                result.reply.invalidate = WidgetInvalidation::paint;
            }
            return result;
        }

        if (const auto *crossing = event.getIf<UIPointerCrossingEvent>()) {
            changed(m_hovered, crossing->entered);
        } else if (event.is<Input::MouseMoveEvent>()) {
            changed(m_hovered, context.pointerInside());
        } else if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                   button != nullptr && button->button == MouseButton::left) {
            if (button->action == MouseButtonAction::press &&
                context.pointerInside()) {
                changed(m_pointerPressed, true);
                changed(m_hovered, true);
                result.reply.handled = true;
                result.reply.stopPropagation = true;
                result.reply.requestFocus = true;
                result.reply.capturePointer = true;
            } else if (button->action == MouseButtonAction::release &&
                       m_pointerPressed) {
                result.activated = context.pointerInside();
                changed(m_pointerPressed, false);
                changed(m_hovered, context.pointerInside());
                result.reply.handled = true;
                result.reply.stopPropagation = true;
                result.reply.releasePointer = true;
            } else if (button->action == MouseButtonAction::doubleClick &&
                       context.pointerInside()) {
                result.activated = true;
                result.reply.handled = true;
                result.reply.stopPropagation = true;
                result.reply.requestFocus = true;
            }
        } else if (const auto *key = event.getIf<Input::KeyEvent>();
                   key != nullptr && context.focused &&
                   (key->key == KeyCode::enter || key->key == KeyCode::space)) {
            if (key->action == KeyAction::press) {
                changed(m_keyboardPressed, true);
                result.reply.handled = true;
                result.reply.stopPropagation = true;
            } else if (key->action == KeyAction::release && m_keyboardPressed) {
                result.activated = true;
                changed(m_keyboardPressed, false);
                result.reply.handled = true;
                result.reply.stopPropagation = true;
            }
        } else if (const auto *focus = event.getIf<UIFocusChangedEvent>();
                   focus != nullptr && !focus->focused) {
            changed(m_pointerPressed, false);
            changed(m_keyboardPressed, false);
        }

        if (result.stateChanged) {
            result.reply.invalidate |= WidgetInvalidation::paint;
        }
        return result;
    }

    void Pressable::reset() noexcept {
        m_hovered = false;
        m_pointerPressed = false;
        m_keyboardPressed = false;
    }

    bool Pressable::isHovered() const noexcept {
        return m_hovered;
    }

    bool Pressable::isPressed() const noexcept {
        return m_pointerPressed || m_keyboardPressed;
    }
} // namespace Bess::UI
