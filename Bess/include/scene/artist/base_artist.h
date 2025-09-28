#pragma once

#include "bess_uuid.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/components/components.h"
#include "scene/renderer/gl/subtexture.h"
#include "types.h"

#include <memory>

// Forward declaration
namespace Bess::Canvas {
    class Scene;
}

namespace Bess::Canvas {

    struct ArtistTools {
        std::array<std::shared_ptr<Gl::SubTexture>, 8> sevenSegDispTexs;
    };

    struct ArtistInstructions {
        bool isSchematicView = false;
    };

    class BaseArtist {
      public:
        BaseArtist(Scene *scene);

        void init();

        virtual ~BaseArtist() = default;

        virtual void drawSimEntity(entt::entity entity) = 0;

        virtual void drawSimEntity(
            entt::entity entity,
            Components::TagComponent &tagComp,
            Components::TransformComponent &transform,
            Components::SpriteComponent &spriteComp,
            Components::SimulationComponent &simComponent) = 0;

        virtual void drawNonSimEntity(entt::entity entity);

        virtual void drawConnectionEntity(entt::entity entity);

        virtual void drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos);

        virtual glm::vec3 getSlotPos(const Components::SlotComponent &comp,
                                     const Components::TransformComponent &parentTransform) = 0;

        void setInstructions(const ArtistInstructions &value);

      protected:
        virtual void drawSlots(const entt::entity parentEntt, const Components::SimulationComponent &comp, const Components::TransformComponent &transformComp) = 0;

        void drawSevenSegDisplay(entt::entity entity,
                                 Components::TagComponent &tagComp,
                                 Components::TransformComponent &transform,
                                 Components::SpriteComponent &spriteComp,
                                 Components::SimulationComponent &simComponent);

        virtual void drawConnection(const UUID &id, entt::entity inputEntity, entt::entity outputEntity, bool isSelected);

        std::shared_ptr<Scene> m_sceneRef;
        ArtistInstructions m_instructions;

        static ArtistTools m_artistTools;
    };

} // namespace Bess::Canvas
