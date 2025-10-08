#pragma once

#include "bess_uuid.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/components/components.h"
#include "scene/renderer/vulkan/vulkan_subtexture.h"
#include "types.h"

#include <memory>

namespace Bess::Canvas {
    class Viewport;
}
using namespace Bess::Renderer2D;

namespace Bess::Canvas {

    struct ArtistTools {
        std::array<std::shared_ptr<Vulkan::SubTexture>, 8> sevenSegDispTexs;
    };

    struct ArtistInstructions {
        bool isSchematicView = false;
    };

    class BaseArtist {
      public:
        explicit BaseArtist(std::shared_ptr<Viewport> viewport);
        virtual ~BaseArtist() = default;

        static void destroyTools();
        static void init();

        static glm::vec2 calcCompSize(entt::entity ent,
                                      const Components::SimulationComponent &simComp,
                                      const std::string &name);

        static bool isHeaderLessComp(const Components::SimulationComponent &simComp);

        virtual void drawSimEntity(entt::entity entity);

        virtual void drawSimEntity(
            entt::entity entity,
            const Components::TagComponent &tagComp,
            const Components::TransformComponent &transform,
            const Components::SpriteComponent &spriteComp,
            const Components::SimulationComponent &simComponent) = 0;

        virtual void drawNonSimEntity(entt::entity entity);

        virtual void drawConnectionEntity(entt::entity entity);

        virtual void drawGhostConnection(const entt::entity &startEntity, const glm::vec2 pos);

        virtual glm::vec3 getSlotPos(const Components::SlotComponent &comp,
                                     const Components::TransformComponent &parentTransform) = 0;

        void setInstructions(const ArtistInstructions &value);

      protected:
        virtual void drawSlots(const entt::entity parentEntt, const Components::SimulationComponent &comp, const Components::TransformComponent &transformComp) = 0;

        virtual void drawConnection(const UUID &id, entt::entity inputEntity, entt::entity outputEntity, bool isSelected);

        std::shared_ptr<Viewport> m_viewportRef = nullptr;
        ArtistInstructions m_instructions = {};

        static ArtistTools m_artistTools;
    };

} // namespace Bess::Canvas
