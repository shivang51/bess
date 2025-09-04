#pragma once

#include "asset_manager/asset_manager.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/scene.h"

#include <memory.h>

namespace Bess::Canvas {

    struct ArtistCompBoundInfo {
        float inpConnStart = 0.f;
        float outConnStart = 0.f;
        float inpPinStart = 0.f;
        float outPinStart = 0.f;
        float rb = 0.f;
    };

    struct ArtistTools {
        std::array<std::shared_ptr<Gl::SubTexture>, 10> sevenSegDispTexs;
    };

    class Artist {
      public:
        static void init();

        static Scene *sceneRef;
        static void drawSimEntity(entt::entity entity);
        static void drawSimEntity(
            entt::entity entity,
            Components::TagComponent &tagComp,
            Components::TransformComponent &transform,
            Components::SpriteComponent &spriteComp,
            Components::SimulationComponent &simComponent);

        static void drawNonSimEntity(entt::entity entity);

        static void drawInput(entt::entity entity);
        static void drawOutput(entt::entity entity);
        static void drawConnectionEntity(entt::entity entity);
        static glm::vec3 getSlotPos(const Components::SlotComponent &comp,
                                    const Components::TransformComponent &parentTransform);
        // use in schematic mode
        static glm::vec3 getPinPos(const Components::SlotComponent &comp);
        static void drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos);
        static void drawConnection(const UUID &id, entt::entity inputEntity, entt::entity outputEntity, bool isSelected);

      public:
        static void setSchematicMode(bool value);
        static bool getSchematicMode();
        static bool *getSchematicModePtr();

      private:
        static void paintSchematicView(entt::entity entity);

        static void paintSlot(uint64_t id, uint64_t parentId, const glm::vec3 &pos, float angle,
                              const std::string &label, float labelDx, bool isHigh, bool isConnected);

        static void drawSlots(const Components::SimulationComponent &comp, const Components::TransformComponent &transformComp);

        static void drawSevenSegDisplay(entt::entity entity,
                                        Components::TagComponent &tagComp,
                                        Components::TransformComponent &transform,
                                        Components::SpriteComponent &spriteComp,
                                        Components::SimulationComponent &simComponent);

        static ArtistCompBoundInfo getCompBoundInfo(SimEngine::ComponentType type, glm::vec2 pos, glm::vec2 scale);

        static bool m_isSchematicMode;

        static ArtistTools m_artistTools;
    };
} // namespace Bess::Canvas
