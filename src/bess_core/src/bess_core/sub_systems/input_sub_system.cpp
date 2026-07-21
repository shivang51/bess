#include "bess_core/sub_systems/input_sub_system.h"

namespace Bess {
    void InputSubSystem::onInit() {
    }

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

    Input::Event InputSubSystem::processEvent(Input::Event event) {
        std::visit([this](auto &inputEvent) { processEvent(inputEvent); },
                   event.data);
        return event;
    }

    void InputSubSystem::processEvent(Input::KeyEvent &event) {
        m_keyStates[event.key] = event;
        m_frameInputState.hasKeyEvent = true;
        m_frameInputState.keyState = event;
        m_frameInputState.keyboardEvents.emplace_back(event);
    }

    void InputSubSystem::processEvent(Input::TextInputEvent &event) {
        m_frameInputState.hasTextInputEvent = true;
        m_frameInputState.keyboardEvents.emplace_back(event);
    }

    void InputSubSystem::processEvent(Input::MouseButtonEvent &event) {
        const auto now = std::chrono::steady_clock::now();

        if (event.action == MouseButtonAction::press &&
            !isMouseBtnPressed(event.button)) {
            const auto &state = m_mouseBtnStates[event.button];
            const float dis = glm::distance(event.pos, state.pos);
            const auto timeDif = now - state.timestamp;

            if (dis <= 5.f && timeDif < TimeMs(500)) {
                event.action = MouseButtonAction::doubleClick;
            }
        }

        event.timestamp = now;
        m_mouseBtnStates[event.button] = event;

        m_frameInputState.hasMouseBtnEvent = true;
        m_frameInputState.mouseBtnState = event;
    }

    void InputSubSystem::processEvent(Input::MouseMoveEvent &event) {
        if (m_isFirstMouseMove) {
            event.delta = glm::vec2(0.0f);
            m_mouseMoveState = event;
            m_isFirstMouseMove = false;
            return;
        }

        event.delta = event.pos - m_mouseMoveState.pos;
        m_mouseMoveState = event;

        m_frameInputState.hasMouseMoved = true;
    }

    void InputSubSystem::processEvent(Input::MouseWheelEvent &event) {
        m_mouseWheelState = event;
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
