#pragma once

#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/scene.h"

namespace Bess::Canvas {
    class Artist {
      public:
        static Scene *sceneRef;
        static void drawSimEntity(entt::entity entity);
        static void drawInput(entt::entity entity);
        static void drawOutput(entt::entity entity);
        static void drawConnectionEntity(entt::entity entity);
        static glm::vec3 getSlotPos(const Components::SlotComponent &comp);
        static void drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos);
        static void drawConnection(entt::entity inputEntity, entt::entity outputEntity);

      private:
        static void paintSlot(uint64_t id, int idx, uint64_t parentId, const glm::vec3 &pos, const std::string &label, float labelDx, bool isHigh);
        static void drawSlots(const Components::SimulationComponent &comp, const glm::vec3 &componentPos, float width);
    };
} // namespace Bess::Canvas
