#pragma once

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
        SlotSceneComponent(UUID uuid, UUID parentId);

        SlotSceneComponent(UUID uuid, const Transform &transform);
        ~SlotSceneComponent() override = default;

        REG_SCENE_COMP(SceneComponentType::slot)

        MAKE_GETTER_SETTER(SlotType, SlotType, m_slotType)
        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)
        MAKE_GETTER_SETTER(UUID, ParentComponentId, m_parentComponentId)

      private:
        SlotType m_slotType = SlotType::none;
        UUID m_simEngineId = UUID::null;
        UUID m_parentComponentId = UUID::null;
    };

    class SimulationSceneComponent : public SceneComponent,
                                     public DragBehaviour<SimulationSceneComponent> {
      public:
        SimulationSceneComponent() = default;
        SimulationSceneComponent(const SimulationSceneComponent &other) = default;
        SimulationSceneComponent(UUID uuid);
        SimulationSceneComponent(UUID uuid, const Transform &transform);
        ~SimulationSceneComponent() override = default;

        void createIOSlots(size_t inputCount, size_t outputCount);

        void onMouseHovered(const glm::vec2 &mousePos) override;

        void onTransformChanged() override;

        void draw(std::shared_ptr<Renderer::MaterialRenderer> renderer) override;

        REG_SCENE_COMP(SceneComponentType::simulation)

        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)

      private:
        // Associated simulation engine ID
        UUID m_simEngineId = UUID::null;
        std::unordered_map<UUID, SlotSceneComponent> m_inputSlots;
        std::unordered_map<UUID, SlotSceneComponent> m_outputSlots;
    };
} // namespace Bess::Canvas
