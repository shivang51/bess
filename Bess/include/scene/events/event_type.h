#pragma once

#include "ext/vector_float2.hpp"
#include "glm.hpp"

namespace Bess::Scene::Events {
    enum class MouseButton {
        left,
        middle,
        right
    };

    enum class EventType {
        mouseMove,
        mouseEnter,
        mouseLeave,
        mouseButton,
        mouseWheel,
        keyPress,
        keyRelease,
        resize
    };

    struct MouseMoveEventData {
        EventType type = EventType::mouseMove;
        glm::vec2 position;
    };

    struct MouseEnterEventData {
        EventType type = EventType::mouseEnter;
    };

    struct MouseLeaveEventData {
        EventType type = EventType::mouseLeave;
    };

    struct MouseClickEventData {
        EventType type = EventType::mouseButton;
        MouseButton button;
        glm::vec2 position;
        bool pressed;
    };

    struct MouseWheelEventData {
        EventType type = EventType::mouseWheel;
        glm::vec2 offset;
    };

    struct KeyPressEventData {
        EventType type = EventType::keyPress;
        int key;
    };

    struct KeyReleaseEventData {
        EventType type = EventType::keyRelease;
        int key;
    };

    struct ResizeEventData {
        EventType type = EventType::resize;
        glm::vec2 size;
    };
} // namespace Bess::Scene::Events
