#pragma once

#include "events/scene_events.h"
#include "scene/renderer/material_renderer.h"
#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"

namespace Bess::Canvas {
    enum class SlotType : uint8_t {
        none,
        digitalInput,
        digitalOutput,
    };

    class SlotSceneComponent : public SceneComponent {
      public:
        SlotSceneComponent() = default;
        SlotSceneComponent(const SlotSceneComponent &other) = default;
        SlotSceneComponent(UUID uuid);

        SlotSceneComponent(UUID uuid, const Transform &transform);
        ~SlotSceneComponent() override = default;

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void onMouseEnter(const Events::MouseEnterEvent &e) override;
        void onMouseLeave(const Events::MouseLeaveEvent &e) override;

        void onMouseButton(const Events::MouseButtonEvent &e) override;

        REG_SCENE_COMP(SceneComponentType::slot)

        MAKE_GETTER_SETTER(SlotType, SlotType, m_slotType)
        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)
        MAKE_GETTER_SETTER(int, Index, m_index)

      private:
        SlotType m_slotType = SlotType::none;
        UUID m_simEngineId = UUID::null;
        int m_index = -1;
    };

    class SimulationSceneComponent : public SceneComponent,
                                     public DragBehaviour<SimulationSceneComponent> {
      public:
        SimulationSceneComponent() = default;
        SimulationSceneComponent(const SimulationSceneComponent &other) = default;
        SimulationSceneComponent(UUID uuid);
        SimulationSceneComponent(UUID uuid, const Transform &transform);
        ~SimulationSceneComponent() override = default;

        // Creates the slots and also add there ids inside the components
        // input slots array and output slots array
        std::vector<std::shared_ptr<SlotSceneComponent>> createIOSlots(size_t inputCount,
                                                                       size_t outputCount);

        void onMouseHovered(const Events::MouseHoveredEvent &e) override;

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        REG_SCENE_COMP(SceneComponentType::simulation)

        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)

      private:
        // Generates the positions relative to the component position
        std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>>
        calculateSlotPositions(size_t inputCount, size_t outputCount) const;

        void resetSlotPositions(SceneState &state);

        glm::vec2 calculateScale(std::shared_ptr<Renderer::MaterialRenderer> materialRenderer) override;

        void onFirstDraw(SceneState &sceneState,
                         std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                         std::shared_ptr<PathRenderer> /*unused*/) override;

        bool isCompHeaderLess() const;

      private:
        // Associated simulation engine ID
        UUID m_simEngineId = UUID::null;
        std::vector<UUID> m_inputSlots;
        std::vector<UUID> m_outputSlots;
    };
} // namespace Bess::Canvas
