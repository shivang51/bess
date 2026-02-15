#pragma once
#include "bess_uuid.h"
#include <cstdint>
#include <glm.hpp>

namespace Bess::Canvas {
    class SceneState;
    enum class SceneComponentType : int8_t;
} // namespace Bess::Canvas

namespace Bess::Canvas::Events {
    enum class MouseClickAction : uint8_t {
        release = 0,
        press = 1,
        repeat = 2,
        doubleClick = 3
    };

    enum class MouseButton : uint8_t {
        left = 0,
        right = 1,
        middle = 2,
        button4 = 3,
        button5 = 4,
        button6 = 5,
        button7 = 6,
        button8 = 7
    };

    struct ComponentAddedEvent {
        UUID uuid;
        Canvas::SceneComponentType type;
    };

    struct ComponentRemovedEvent {
        UUID uuid;
        Canvas::SceneComponentType type;
    };

    struct EntityReparentedEvent {
        UUID entityUuid;
        UUID newParentUuid;
    };

    struct EntityHoveredEvent {
        UUID entityUuid;
        glm::vec2 mousePos;
    };

    struct MouseDraggedEvent {
        glm::vec2 mousePos;
        glm::vec2 delta;
        uint32_t details;
        bool isMultiDrag;
        Canvas::SceneState *sceneState;
    };

    struct MouseHoveredEvent {
        glm::vec2 mousePos;
        uint32_t details;
    };

    struct MouseEnterEvent {
        glm::vec2 mousePos;
        uint32_t details;
    };

    struct MouseLeaveEvent {
        glm::vec2 mousePos;
        uint32_t details;
    };

    struct MouseButtonEvent {
        glm::vec2 mousePos;
        MouseButton button;
        MouseClickAction action;
        uint32_t details;
        Canvas::SceneState *sceneState;
    };

    struct ConnectionRemovedEvent {
        UUID slotAId;
        UUID slotBId;
    };

    struct EntityMovedEvent {
        UUID entityUuid;
        glm::vec3 oldPos;
        glm::vec3 newPos;
    };
} // namespace Bess::Canvas::Events
