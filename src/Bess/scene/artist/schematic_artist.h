#pragma once

#include "base_artist.h"
#include "bess_uuid.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/components/components.h"
#include "application/types.h"
#include <unordered_map>

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
        SchematicArtist(const std::shared_ptr<Vulkan::VulkanDevice> &device,
                        const std::shared_ptr<Vulkan::VulkanOffscreenRenderPass> &renderPass,
                        VkExtent2D extent);
        virtual ~SchematicArtist() = default;

        void drawSimEntity(
            entt::entity entity,
            const Components::TagComponent &tagComp,
            const Components::TransformComponent &transform,
            const Components::SpriteComponent &spriteComp,
            const Components::SimulationComponent &simComponent) override;

        glm::vec3 getSlotPos(const Components::SlotComponent &comp,
                             const Components::TransformComponent &parentTransform) override;

        void drawSlots(const entt::entity parentEntt, const Components::SimulationComponent &simComp, const Components::TransformComponent &transformComp) override;

      private:
        void paintSchematicView(entt::entity entity,
                                const Components::TagComponent &tagComp,
                                const Components::TransformComponent &transform,
                                const Components::SpriteComponent &spriteComp,
                                const Components::SimulationComponent &simComponent);

        void drawSevenSegDisplay(entt::entity entity,
                                 const Components::TagComponent &tagComp,
                                 const Components::TransformComponent &transform,
                                 const Components::SpriteComponent &spriteComp,
                                 const Components::SimulationComponent &simComponent) const;

        ArtistCompSchematicInfo getCompSchematicInfo(entt::entity ent, float width = -1, float height = -1) const;
        ArtistCompSchematicInfo getCompSchematicInfo(UUID uuid) const;

        std::unordered_map<int, glm::vec2> m_diagramSize;
    };

} // namespace Bess::Canvas
