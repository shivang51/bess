#include "input_sub_system.h"

namespace Bess {
    void InputSubSystem::onInit() {}

    void InputSubSystem::onDestroy() {
        m_keyStates.clear();
        m_mouseBtnStates.clear();
    }

    void InputSubSystem::onKeyEvent(KeyCode key, KeyAction action) {
        m_keyStates[key] = action;
    }

    void InputSubSystem::onMouseButtonEvent(MouseButton button,
                                            MouseButtonAction action) {
        m_mouseBtnStates[button] = action;
    }

    void InputSubSystem::onMouseMoveEvent(const glm::vec2 &pos) {
        if (m_isFirstMouseMove) {
            m_mouseMoveState.pos = pos;
            m_mouseMoveState.delta = glm::vec2(0.0f);
            m_isFirstMouseMove = false;
            return;
        }

        m_mouseMoveState.delta = pos - m_mouseMoveState.pos;
        m_mouseMoveState.pos = pos;
    }

    void InputSubSystem::onMouseWheelEvent(const glm::vec2 &offset) {}

    bool InputSubSystem::isKeyPressed(KeyCode key) const {
        auto it = m_keyStates.find(key);
        return it != m_keyStates.end() && it->second == KeyAction::press;
    }

    bool InputSubSystem::isMouseBtnPressed(MouseButton button) const {
        auto it = m_mouseBtnStates.find(button);
        return it != m_mouseBtnStates.end() &&
               it->second == MouseButtonAction::press;
    }

    bool InputSubSystem::isCtrlPressed() const {
        return isKeyPressed(KeyCode::leftControl) ||
               isKeyPressed(KeyCode::rightControl);
    }

    bool InputSubSystem::isShiftPressed() const {
        return isKeyPressed(KeyCode::leftShift) ||
               isKeyPressed(KeyCode::rightShift);
    }

    bool InputSubSystem::isAltPressed() const {
        return isKeyPressed(KeyCode::leftAlt) ||
               isKeyPressed(KeyCode::rightAlt);
    }
} // namespace Bess
