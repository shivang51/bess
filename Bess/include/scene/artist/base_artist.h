#pragma once

#include "bess_uuid.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/components/components.h"
#include "scene/renderer/vulkan/vulkan_subtexture.h"
#include "types.h"

#include <memory>

// Forward declaration
namespace Bess::Canvas {
    class Scene;
}
using namespace Bess::Renderer2D;

namespace Bess::Canvas {

    struct ArtistTools {
        std::array<std::shared_ptr<Vulkan::VulkanSubTexture>, 8> sevenSegDispTexs;
    };

    struct ArtistInstructions {
        bool isSchematicView = false;
    };

    class BaseArtist {
      public:
        explicit BaseArtist(Scene *scene);
        virtual ~BaseArtist() = default;

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

        Scene* m_sceneRef;
        ArtistInstructions m_instructions;

        static ArtistTools m_artistTools;
    };

} // namespace Bess::Canvas
