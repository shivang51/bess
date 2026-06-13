#include "input_sub_system.h"
#include "sub_systems/input_sub_system_types.h"

namespace Bess {
    void InputSubSystem::onInit() {}

    void InputSubSystem::onDestroy() {
        m_keyStates.clear();
        m_mouseBtnStates.clear();
    }

    void InputSubSystem::onBeginFrame() {
        m_frameInputState = {};

        for (auto &[_, state] : m_keyStates) {
            if (state.action == KeyAction::press) {
                state.action = KeyAction::hold;
            }
        }
    }

    void InputSubSystem::onKeyEvent(KeyCode key, KeyAction action) {
        m_keyStates[key] = {key, action};
        m_frameInputState.hasKeyEvent = true;
        m_frameInputState.keyState = m_keyStates[key];
    }

    void InputSubSystem::onMouseButtonEvent(MouseButton button,
                                            MouseButtonAction action,
                                            const glm::vec2 &pos) {

        if (action == MouseButtonAction::press && !isMouseBtnPressed(button)) {
            const auto &state = m_mouseBtnStates[button];
            const float dis = glm::distance(pos, state.pos);
            const auto timeDif =
                std::chrono::steady_clock::now() - state.timestamp;

            if (dis <= 5.f && timeDif < TimeMs(500)) {
                action = MouseButtonAction::doubleClick;
            }
        }

        m_mouseBtnStates[button] = {button, action, pos,
                                    std::chrono::steady_clock::now()};

        m_frameInputState.hasMouseBtnEvent = true;
        m_frameInputState.mouseBtnState = m_mouseBtnStates[button];
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

        m_frameInputState.hasMouseMoved = true;
    }

    void InputSubSystem::onMouseWheelEvent(const glm::vec2 &offset) {
        m_mouseWheelState.offset = offset;
        m_mouseWheelState.pos = m_mouseMoveState.pos;
        m_frameInputState.hasMouseWheelScrolled = true;
    }

    bool InputSubSystem::isKeyPressed(KeyCode key) const {
        auto it = m_keyStates.find(key);
        return it != m_keyStates.end() && it->second.action == KeyAction::press;
    }

    bool InputSubSystem::isKeyHeld(KeyCode key) const {
        auto it = m_keyStates.find(key);
        return it != m_keyStates.end() && it->second.action == KeyAction::hold;
    }

    bool InputSubSystem::isMouseBtnPressed(MouseButton button) const {
        auto it = m_mouseBtnStates.find(button);
        return it != m_mouseBtnStates.end() &&
               it->second.action == MouseButtonAction::press;
    }

    bool InputSubSystem::isMouseBtnDoubleClicked(MouseButton button) const {
        auto it = m_mouseBtnStates.find(button);
        return it != m_mouseBtnStates.end() &&
               it->second.action == MouseButtonAction::doubleClick;
    }

    bool InputSubSystem::isCtrlPressed() const {
        return isKeyPressed(KeyCode::leftControl) ||
               isKeyPressed(KeyCode::rightControl) ||
               isKeyHeld(KeyCode::leftControl) ||
               isKeyHeld(KeyCode::rightControl);
    }

    bool InputSubSystem::isShiftPressed() const {
        return isKeyPressed(KeyCode::leftShift) ||
               isKeyPressed(KeyCode::rightShift) ||
               isKeyHeld(KeyCode::leftShift) || isKeyHeld(KeyCode::rightShift);
    }

    bool InputSubSystem::isAltPressed() const {
        return isKeyPressed(KeyCode::leftAlt) ||
               isKeyPressed(KeyCode::rightAlt) || isKeyHeld(KeyCode::leftAlt) ||
               isKeyHeld(KeyCode::rightAlt);
    }
} // namespace Bess
