#pragma once

#include "bess_uuid.h"
#include "scene/renderer/material_renderer.h"
#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/sim_scene_comp_draw_hook.h"
#include "scene/scene_state/components/slot_scene_component.h"

namespace Bess::Canvas {
    class SimulationSceneComponent : public SceneComponent,
                                     public DragBehaviour<SimulationSceneComponent> {
      public:
        SimulationSceneComponent();
        SimulationSceneComponent(const SimulationSceneComponent &other) = default;
        ~SimulationSceneComponent() override = default;

        static std::shared_ptr<SimulationSceneComponent> createNewAndRegister(SceneState &sceneState, UUID simEngineId);

        // Creates the slots and also add there ids inside the components
        // input slots array and output slots array
        std::vector<std::shared_ptr<SlotSceneComponent>> createIOSlots(size_t inputCount,
                                                                       size_t outputCount);

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void drawSchematic(SceneState &state,
                           std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                           std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        REG_SCENE_COMP(SceneComponentType::simulation)

        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)
        MAKE_GETTER_SETTER(UUID, NetId, m_netId)
        MAKE_GETTER_SETTER(std::vector<UUID>, InputSlots, m_inputSlots)
        MAKE_GETTER_SETTER(std::vector<UUID>, OutputSlots, m_outputSlots)
        MAKE_GETTER_SETTER(std::shared_ptr<SimSceneCompDrawHook>, DrawHook, m_drawHook)

        size_t getInputSlotsCount() const;
        size_t getOutputSlotsCount() const;

        void addInputSlot(UUID slotId, bool isLastResizeable = true);
        void addOutputSlot(UUID slotId, bool isLastResizeable = true);

        void setScaleDirty();

        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

      protected:
        /**
         * Resets the slot positions based on the current scale and number of slots
         * in the component. Will ignore slots that are resize slots for the schematic view.
         */
        void resetSlotPositions(SceneState &state);

        // Generates the positions relative to the component position
        std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>>
        calculateSlotPositions(size_t inputCount, size_t outputCount) const;

        glm::vec2 calculateScale(std::shared_ptr<Renderer::MaterialRenderer> materialRenderer) override;

        virtual void calculateSchematicScale(SceneState &state);

        void onFirstDraw(SceneState &sceneState,
                         std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                         std::shared_ptr<PathRenderer> /*unused*/) override;

        void onFirstSchematicDraw(SceneState &sceneState,
                                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                  std::shared_ptr<PathRenderer> /*unused*/) override;

        void removeChildComponent(const UUID &uuid) override;

      protected:
        // Associated simulation engine ID
        UUID m_simEngineId = UUID::null;
        UUID m_netId = UUID::null;
        std::vector<UUID> m_inputSlots;
        std::vector<UUID> m_outputSlots;
        bool m_isScaleDirty = true;
        glm::vec2 m_schematicScale = glm::vec2(0.f);
        std::shared_ptr<SimSceneCompDrawHook> m_drawHook = nullptr;
    };
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SimulationSceneComponent &component, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::SimulationSceneComponent &component);
} // namespace Bess::JsonConvert
