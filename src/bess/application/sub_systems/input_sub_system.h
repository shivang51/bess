#pragma once

#include "common/sub_system.h"
#include "ext/vector_float2.hpp"
#include "input_sub_system_types.h"
#include <unordered_map>

namespace Bess {

    struct MouseWheelState {
        glm::vec2 offset;
    };

    struct MouseMoveState {
        glm::vec2 pos;
        glm::vec2 delta;
    };

    class InputSubSystem : public ISubSystem {
      public:
        void onInit() override;
        void onDestroy() override;

        void onKeyEvent(KeyCode key, KeyAction action);
        void onMouseButtonEvent(MouseButton button, MouseButtonAction action);
        void onMouseMoveEvent(const glm::vec2 &pos);
        void onMouseWheelEvent(const glm::vec2 &offset);

        bool isKeyPressed(KeyCode key) const;

        bool isMouseBtnPressed(MouseButton button) const;

        bool isCtrlPressed() const;

        bool isShiftPressed() const;

        bool isAltPressed() const;

      private:
        std::unordered_map<KeyCode, KeyAction> m_keyStates;
        std::unordered_map<MouseButton, MouseButtonAction> m_mouseBtnStates;
        MouseWheelState m_mouseWheelState;
        MouseMoveState m_mouseMoveState;
        bool m_isFirstMouseMove = true;
    };
} // namespace Bess
