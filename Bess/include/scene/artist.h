#pragma once

#include "entt/entity/fwd.hpp"
#include "scene/scene.h"

namespace Bess::Canvas {
    class Artist {
      public:
        static Scene *sceneRef;
        static void drawSimEntity(entt::entity entity);
        static void drawConnectionEntity(entt::entity entity);
        static glm::vec3 getSlotPos(const Components::SlotComponent &comp);
        static void drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos);
        static void drawConnection(entt::entity startEntity, entt::entity endEntity);

      private:
        static void drawSlots(const Components::SimulationComponent &comp, const glm::vec3 &componentPos, float width);
    };
} // namespace Bess::Canvas
