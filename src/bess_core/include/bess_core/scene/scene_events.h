#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
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

    struct BESS_API ComponentAddedEvent {
        UUID uuid;
        Canvas::SceneComponentType type;
        UUID sceneId;
        Canvas::SceneState *state;
    };

    struct BESS_API ComponentRemovedEvent {
        UUID uuid;
        Canvas::SceneComponentType type;
        UUID sceneId;
        Canvas::SceneState *state;
    };

    struct BESS_API EntityReparentedEvent {
        UUID entityUuid;
        UUID newParentUuid;
        UUID prevParent;
        UUID sceneId;
        Canvas::SceneState *state;
    };

    struct BESS_API EntityHoveredEvent {
        UUID entityUuid;
        glm::vec2 mousePos;
    };

    struct BESS_API MouseDraggedEvent {
        glm::vec2 mousePos;
        glm::vec2 delta;
        uint32_t details;
        bool isMultiDrag;
        Canvas::SceneState *sceneState;
        bool isSchematicMode;
    };

    struct BESS_API MouseHoveredEvent {
        glm::vec2 mousePos;
        uint32_t details;
    };

    struct BESS_API MouseEnterEvent {
        glm::vec2 mousePos;
        uint32_t details;
    };

    struct BESS_API MouseLeaveEvent {
        glm::vec2 mousePos;
        uint32_t details;
    };

    struct BESS_API MouseButtonEvent {
        glm::vec2 mousePos;
        MouseButton button;
        MouseClickAction action;
        uint32_t details;
        Canvas::SceneState *sceneState;
    };

    struct BESS_API MouseWheelEvent {
        glm::vec2 mousePos;
        glm::vec2 delta;
        uint32_t details;
        Canvas::SceneState *sceneState;
    };

    struct BESS_API MouseMoveEvent {
        glm::vec2 mousePos;
        uint32_t details;
        Canvas::SceneState *sceneState;
    };

    struct BESS_API FocusEvent {
        UUID entityUuid;
        glm::vec2 mousePos{0.f};
        uint32_t details = 0;
        Canvas::SceneState *sceneState = nullptr;
    };

    struct BESS_API ConnectionRemovedEvent {
        UUID slotAId;
        UUID slotBId;
    };

    struct BESS_API EntityMovedEvent {
        UUID entityUuid;
        glm::vec3 oldPos;
        glm::vec3 newPos;
        Canvas::SceneState *state;
    };
} // namespace Bess::Canvas::Events
