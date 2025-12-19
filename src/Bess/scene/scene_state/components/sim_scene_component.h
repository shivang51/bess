#pragma once

#include "bess_uuid.h"
#include "scene/renderer/material_renderer.h"
#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/slot_scene_component.h"

namespace Bess::Canvas {
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

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        REG_SCENE_COMP(SceneComponentType::simulation)

        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)
        MAKE_GETTER_SETTER(UUID, NetId, m_netId)

      protected:
        // Generates the positions relative to the component position
        std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>>
        calculateSlotPositions(size_t inputCount, size_t outputCount) const;

        void resetSlotPositions(SceneState &state);

        glm::vec2 calculateScale(std::shared_ptr<Renderer::MaterialRenderer> materialRenderer) override;

        void onFirstDraw(SceneState &sceneState,
                         std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                         std::shared_ptr<PathRenderer> /*unused*/) override;

      protected:
        // Associated simulation engine ID
        UUID m_simEngineId = UUID::null;
        UUID m_netId = UUID::null;
        std::vector<UUID> m_inputSlots;
        std::vector<UUID> m_outputSlots;
    };
} // namespace Bess::Canvas
