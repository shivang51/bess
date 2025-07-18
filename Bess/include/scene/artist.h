#pragma once

#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/scene.h"

namespace Bess::Canvas {

    struct ArtistCompBoundInfo {
      float inpConnStart = 0.f;
      float outConnStart = 0.f;
      float inpPinStart = 0.f;
      float outPinStart = 0.f;
      float rb = 0.f;
    };

    class Artist {
      public:
        static Scene *sceneRef;
        static void drawSimEntity(entt::entity entity);
        static void drawInput(entt::entity entity);
        static void drawOutput(entt::entity entity);
        static void drawConnectionEntity(entt::entity entity);
        static glm::vec3 getSlotPos(const Components::SlotComponent &comp);
        // use in schematic mode
        static glm::vec3 getPinPos(const Components::SlotComponent &comp);
        static void drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos);
        static void drawConnection(const UUID &id, entt::entity inputEntity, entt::entity outputEntity, bool isSelected);

      public:
        static void setSchematicMode(bool value);
        static bool getSchematicMode();
        static bool* getSchematicModePtr();

      private:
        static void paintSchematicView(entt::entity entity);

        static void paintSlot(uint64_t id, int idx, uint64_t parentId, const glm::vec3 &pos, float angle,
                              const std::string &label, float labelDx, bool isHigh, bool isConnected);

        static void drawSlots(const Components::SimulationComponent &comp, const glm::vec3 &componentPos, float width, float angle);

        static ArtistCompBoundInfo getCompBoundInfo(SimEngine::ComponentType type, glm::vec2 pos, glm::vec2 scale);

        static bool m_isSchematicMode;
    };
} // namespace Bess::Canvas
