#pragma once
#include "bess_uuid.h"
#include <cstdint>
#include <glm.hpp>

namespace Bess::Canvas::Events {
    struct EntityCreatedEvent {
        UUID uuid;
        entt::entity entity;
        bool isSimEntity;
    };

    struct EntityDestroyedEvent {
        UUID uuid;
        entt::entity entity;
    };

    struct EntityReparentedEvent {
        UUID entityUuid;
        UUID newParentUuid;
    };

    struct EntityHoveredEvent {
        UUID entityUuid;
        glm::vec2 mousePos;
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
} // namespace Bess::Canvas::Events
