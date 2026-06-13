#pragma once
#include "common/types.h"
#include "sub_systems/input_sub_system_types.h"
#include <cstdint>
#include <glm.hpp>

namespace Bess::Canvas {
    class SceneState;
    enum class SceneComponentType : int8_t;
} // namespace Bess::Canvas

namespace Bess::Canvas {
    struct SceneEvent {
        enum class Type : uint8_t {
            none,
            mouseMove,
            mouseButton,
            mouseWheel,
            key,
            textInput,
        } type;

        union Data {
            struct MouseMoveData {
                glm::vec2 pos;
                glm::vec2 delta;
                glm::vec2 viewportPos;
            } mouseMove, mouseWheel;

            struct MouseButtonData {
                MouseButton button;
                MouseButtonAction action;
                glm::vec2 pos;
            } mouseButton;

            struct KeyData {
                KeyCode keycode;
                KeyAction action;
            } keyPress;

            struct TextInputData {
                char32_t codepoint;
            } textInput;

            Data() {}
            ~Data() {}
        } data;

        bool isCtrlPressed = false;
        bool isShiftPressed = false;
        bool isAltPressed = false;

        PickingId pickingId = PickingId::invalid();
    };
} // namespace Bess::Canvas
