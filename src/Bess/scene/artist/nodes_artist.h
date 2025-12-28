#pragma once

#include "application/types.h"
#include "base_artist.h"
#include "entt/entity/fwd.hpp"
#include "ext/vector_float3.hpp"
#include "scene/components/components.h"

namespace Bess::Canvas {
    constexpr struct NodeComponentStyles {
        float headerHeight = 18.f;
        float headerFontSize = 10.f;
        float paddingX = 8.f;
        float paddingY = 4.f;
        float slotRadius = 4.5f;
        float slotMargin = 2.f;
        float slotBorderSize = 1.f;
        float rowMargin = 4.f;
        float rowGap = 4.f;
        float slotLabelSize = 10.f;

        inline float getSlotColumnSize() const {
            return (slotRadius * 2.f) + slotMargin + slotLabelSize;
        }
    } componentStyles;

    constexpr float SLOT_DX = componentStyles.paddingX + componentStyles.slotRadius + componentStyles.slotMargin;
    constexpr float SLOT_START_Y = componentStyles.headerHeight;
    constexpr float SLOT_ROW_SIZE = (componentStyles.rowMargin * 2.f) + (componentStyles.slotRadius * 2.f) + componentStyles.rowGap;
    constexpr float SLOT_COLUMN_SIZE = (componentStyles.slotRadius + componentStyles.slotMargin + componentStyles.slotLabelSize) * 2;

    class NodesArtist : public BaseArtist {
      public:
        NodesArtist(const std::shared_ptr<Vulkan::VulkanDevice> &device,
                    const std::shared_ptr<Vulkan::VulkanOffscreenRenderPass> &renderPass,
                    VkExtent2D extent);
        virtual ~NodesArtist() = default;

        void drawSimEntity(
            entt::entity entity,
            const Components::TagComponent &tagComp,
            const Components::TransformComponent &transform,
            const Components::SpriteComponent &spriteComp,
            const Components::SimulationComponent &simComponent) override;

        glm::vec3 getSlotPos(const Components::SlotComponent &comp, const Components::TransformComponent &parentTransform) override;

        void drawSlots(const entt::entity parentEntt, const Components::SimulationComponent &comp, const Components::TransformComponent &transformComp) override;

      private:
        void drawSevenSegDisplay(entt::entity entity,
                                 const Components::TagComponent &tagComp,
                                 const Components::TransformComponent &transform,
                                 const Components::SpriteComponent &spriteComp,
                                 const Components::SimulationComponent &simComponent);

        void paintSlot(int id, int parentId, const glm::vec3 &pos,
                       float angle, const std::string &label, float labelDx,
                       SimEngine::LogicState state, bool isConnected) const;

        void drawHeaderLessComp(entt::entity entity,
                                const Components::TagComponent &tagComp,
                                const Components::TransformComponent &transform,
                                const Components::SpriteComponent &spriteComp,
                                const Components::SimulationComponent &simComp);
    };

} // namespace Bess::Canvas
