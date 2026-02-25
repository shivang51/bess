#pragma once

#include "common/bess_uuid.h"
#include "component_definition.h"
#include "scene/renderer/material_renderer.h"
#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/components/sim_scene_comp_draw_hook.h"
#include "scene_comp_types.h"
#include "slot_scene_component.h"

#define SIM_SC_SER_PROPS ("simEngineId", getSimEngineId, setSimEngineId), \
                         ("netId", getNetId, setNetId),                   \
                         ("inputSlots", getInputSlots, setInputSlots),    \
                         ("outputSlots", getOutputSlots, setOutputSlots), \
                         ("schematicTransform", getSchematicTransform, setSchematicTransform)

namespace Bess::Canvas {
    class SimulationSceneComponent : public SceneComponent,
                                     public DragBehaviour<SimulationSceneComponent> {
      public:
        SimulationSceneComponent();
        SimulationSceneComponent(const SimulationSceneComponent &other) = default;
        ~SimulationSceneComponent() override = default;

        // Create a new SimSceneComp
        // [0] -> Component itself
        // [1...] -> Created slots
        static std::vector<std::shared_ptr<SceneComponent>> createNewAndRegister(
            const std::shared_ptr<SimEngine::ComponentDefinition> &compDef);

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

        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)
        MAKE_GETTER_SETTER(UUID, NetId, m_netId)
        MAKE_GETTER_SETTER(std::vector<UUID>, InputSlots, m_inputSlots)
        MAKE_GETTER_SETTER(std::vector<UUID>, OutputSlots, m_outputSlots)
        MAKE_GETTER_SETTER(std::shared_ptr<SimSceneCompDrawHook>, DrawHook, m_drawHook)
        MAKE_GETTER_SETTER(Transform, SchematicTransform, m_schematicTransform)
        MAKE_GETTER_SETTER(std::shared_ptr<SimEngine::ComponentDefinition>, CompDef, m_compDef)

        size_t getInputSlotsCount() const;
        size_t getOutputSlotsCount() const;

        void addInputSlot(UUID slotId, bool isLastResizeable = true);
        void addOutputSlot(UUID slotId, bool isLastResizeable = true);

        void setScaleDirty();

        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

        void onAttach(SceneState &state) override;

        void onMouseDragged(const Events::MouseDraggedEvent &e) override;

        glm::vec3 getAbsolutePosition(const SceneState &state) const override;

        REG_SCENE_COMP_TYPE("SimulationSceneComponent", SceneComponentType::simulation)
        SCENE_COMP_SER(Bess::Canvas::SimulationSceneComponent,
                       Bess::Canvas::SceneComponent,
                       SIM_SC_SER_PROPS)

        std::vector<UUID> getDependants(SceneState &state) const override;

      protected:
        void onTransformChanged() override;

        /**
         * Resets the slot positions based on the current scale and number of slots
         * in the component.
         */
        void resetSlotPositions(SceneState &state);

        /**
         * Resets the schematic pin positions based on the current schematic scale and number of slots
         * in the component. Will ignore slots that are resize slots for the schematic view.
         */
        void resetSchematicPinsPositions(SceneState &state);

        // Generates the positions relative to the component position
        std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>>
        calculateSlotPositions(size_t inputCount, size_t outputCount) const;

        glm::vec2 calculateScale(SceneState &state,
                                 std::shared_ptr<Renderer::MaterialRenderer> materialRenderer) override;

        virtual void calculateSchematicScale(SceneState &state,
                                             const std::shared_ptr<Renderer::MaterialRenderer> &materialRenderer);

        void onFirstDraw(SceneState &sceneState,
                         std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                         std::shared_ptr<PathRenderer> /*unused*/) override;

        void onFirstSchematicDraw(SceneState &sceneState,
                                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                  std::shared_ptr<PathRenderer> /*unused*/) override;

      protected:
        // Associated simulation engine ID
        UUID m_simEngineId = UUID::null;
        UUID m_netId = UUID::null;
        std::vector<UUID> m_inputSlots;
        std::vector<UUID> m_outputSlots;
        bool m_isScaleDirty = true;
        Transform m_schematicTransform;
        std::shared_ptr<SimSceneCompDrawHook> m_drawHook = nullptr;
        std::shared_ptr<SimEngine::ComponentDefinition> m_compDef = nullptr;
    };
} // namespace Bess::Canvas

REG_SCENE_COMP(Bess::Canvas::SimulationSceneComponent,
               Bess::Canvas::SceneComponent, SIM_SC_SER_PROPS)
