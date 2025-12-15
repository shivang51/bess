#pragma once
#include "bess_uuid.h"
#include "entt/entity/entity.hpp"
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
} // namespace Bess::Canvas::Events
