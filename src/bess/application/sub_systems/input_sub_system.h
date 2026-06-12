#pragma once

#include "common/class_helpers.h"
#include "common/sub_system.h"
#include "ext/vector_float2.hpp"
#include "input_sub_system_types.h"
#include <unordered_map>

namespace Bess {

    struct KeyState {
        KeyCode key = KeyCode::unknown;
        KeyAction action = KeyAction::release;
    };

    struct MouseWheelState {
        glm::vec2 pos;
        glm::vec2 offset;
    };

    struct MouseMoveState {
        glm::vec2 pos;
        glm::vec2 delta;
    };

    struct MouseButtonState {
        MouseButton button = MouseButton::unknown;
        MouseButtonAction action = MouseButtonAction::press;
        glm::vec2 pos = {0.f, 0.f};
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
    };

    struct FrameInputState {
        bool hasMouseMoved = false;
        bool hasMouseWheelScrolled = false;
        bool hasMouseBtnEvent = false;

        MouseButtonState mouseBtnState =
            {}; // state of mouse button updated in current frame
    };

    class InputSubSystem : public ISubSystem {
      public:
        void onInit() override;
        void onDestroy() override;

        void onBeginFrame() override;

        void onKeyEvent(KeyCode key, KeyAction action);
        void onMouseButtonEvent(MouseButton button, MouseButtonAction action,
                                const glm::vec2 &pos);
        void onMouseMoveEvent(const glm::vec2 &pos);
        void onMouseWheelEvent(const glm::vec2 &offset);

        bool isKeyPressed(KeyCode key) const;

        bool isKeyHeld(KeyCode key) const;

        bool isMouseBtnPressed(MouseButton button) const;

        bool isMouseBtnDoubleClicked(MouseButton button) const;

        bool isCtrlPressed() const;

        bool isShiftPressed() const;

        bool isAltPressed() const;

        MAKE_GETTER(FrameInputState, FrameInpState, m_frameInputState);
        MAKE_GETTER(MouseWheelState, MouseWheelState, m_mouseWheelState);
        MAKE_GETTER(MouseMoveState, MouseMoveState, m_mouseMoveState);

      private:
        std::unordered_map<KeyCode, KeyState> m_keyStates;
        std::unordered_map<MouseButton, MouseButtonState> m_mouseBtnStates;
        MouseWheelState m_mouseWheelState;
        MouseMoveState m_mouseMoveState;
        bool m_isFirstMouseMove = true;
        FrameInputState m_frameInputState;
    };
} // namespace Bess
