#pragma once

#include "common/bess_api.h"

#include "bess_core/input/input_event.h"
#include "common/class_helpers.h"
#include "common/sub_system.h"
#include <unordered_map>
#include <vector>

namespace Bess {

    using KeyState = Input::KeyEvent;
    using TextInputState = Input::TextInputEvent;
    using TextCompositionState = Input::TextCompositionEvent;
    using KeyboardInputEvent = Input::KeyboardEvent;
    using MouseWheelState = Input::MouseWheelEvent;
    using MouseMoveState = Input::MouseMoveEvent;
    using MouseButtonState = Input::MouseButtonEvent;

    struct BESS_API FrameInputState {
        bool hasMouseMoved = false;
        bool hasMouseWheelScrolled = false;
        bool hasMouseBtnEvent = false;
        bool hasKeyEvent = false;
        bool hasTextInputEvent = false;
        bool hasTextCompositionEvent = false;

        MouseButtonState mouseBtnState =
            {}; // state of mouse button updated in current frame
        KeyState keyState = {};
        std::vector<KeyboardInputEvent> keyboardEvents;
    };

    class BESS_API InputSubSystem : public ISubSystem {
      public:
        void onInit() override;
        void onDestroy() override;

        void onBeginFrame() override;

        // Updates the persistent and per-frame input state. The returned event
        // contains normalized data, such as mouse deltas and double-click
        // actions, and can be forwarded to other input consumers.
        [[nodiscard]] Input::Event processEvent(Input::Event event);

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
        void processEvent(Input::KeyEvent &event);
        void processEvent(Input::TextInputEvent &event);
        void processEvent(Input::TextCompositionEvent &event);
        void processEvent(Input::MouseButtonEvent &event);
        void processEvent(Input::MouseMoveEvent &event);
        void processEvent(Input::MouseWheelEvent &event);

      private:
        std::unordered_map<KeyCode, KeyState> m_keyStates;
        std::unordered_map<MouseButton, MouseButtonState> m_mouseBtnStates;
        MouseWheelState m_mouseWheelState;
        MouseMoveState m_mouseMoveState;
        bool m_isFirstMouseMove = true;
        FrameInputState m_frameInputState;
    };
} // namespace Bess
