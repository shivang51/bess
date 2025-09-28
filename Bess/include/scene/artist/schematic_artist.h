#pragma once

#include "base_artist.h"
#include "bess_uuid.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/components/components.h"
#include "types.h"

namespace Bess::Canvas {

    struct ArtistCompSchematicInfo {
        float inpConnStart = 0.f;
        float outConnStart = 0.f;
        float inpPinStart = 0.f;
        float outPinStart = 0.f;
        float rb = 0.f;
        float width, height = 0.f;
        bool shouldDraw = true;
    };

    class SchematicArtist : public BaseArtist {
      public:
        SchematicArtist(Scene *scene);
        virtual ~SchematicArtist() = default;

        void drawSimEntity(entt::entity entity) override;
        void drawSimEntity(
            entt::entity entity,
            Components::TagComponent &tagComp,
            Components::TransformComponent &transform,
            Components::SpriteComponent &spriteComp,
            Components::SimulationComponent &simComponent) override;

        glm::vec3 getSlotPos(const Components::SlotComponent &comp,
                             const Components::TransformComponent &parentTransform) override;

        void drawSlots(const entt::entity parentEntt, const Components::SimulationComponent &comp, const Components::TransformComponent &transformComp) override;

      private:
        // Schematic-specific methods
        void paintSchematicView(entt::entity entity);
        ArtistCompSchematicInfo getCompSchematicInfo(entt::entity ent);
        ArtistCompSchematicInfo getCompSchematicInfo(UUID uuid);
    };

} // namespace Bess::Canvas
