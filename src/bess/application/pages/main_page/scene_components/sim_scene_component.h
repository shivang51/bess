#pragma once

#include "common/bess_uuid.h"
#include "analog_simulation.h"
#include "component_definition.h"
#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene_comp_types.h"
#include "scene_draw_context.h"
#include "settings/viewport_theme.h"
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
        static std::vector<std::shared_ptr<SceneComponent>> createNew(
            const std::shared_ptr<SimEngine::ComponentDefinition> &compDef);

        template <typename T = SimulationSceneComponent>
        static std::vector<std::shared_ptr<SceneComponent>> createNew(
            const std::shared_ptr<SimEngine::ComponentDefinition> &compDef) {
            std::vector<std::shared_ptr<SceneComponent>> createdComps;

            const UUID uuid;
            std::shared_ptr<T> sceneComp = std::make_shared<T>();
            sceneComp->setCompDef(compDef);

            createdComps.push_back(sceneComp);

            // setting the name before adding to scene state, so that event listeners can access it
            sceneComp->setName(compDef->getName());

            // style
            auto &style = sceneComp->getStyle();

            style.color = ViewportTheme::colors.componentBG;
            style.borderRadius = glm::vec4(6.f);
            style.headerColor = ViewportTheme::getCompHeaderColor(compDef->getGroupName());
            style.borderColor = ViewportTheme::colors.componentBorder;
            style.borderSize = glm::vec4(1.f);
            style.color = ViewportTheme::colors.componentBG;

            const auto &inpDetails = compDef->getInputSlotsInfo();
            const auto &outDetails = compDef->getOutputSlotsInfo();

            int inSlotIdx = 0, outSlotIdx = 0;
            char inpCh = 'A', outCh = 'a';

            const auto analogTrait = compDef->getTrait<SimEngine::AnalogComponentTrait>();
            const auto slots = analogTrait
                                   ? sceneComp->createAnalogTerminalSlots(analogTrait->terminalCount)
                                   : sceneComp->createIOSlots(compDef->getInputSlotsInfo().count,
                                                              compDef->getOutputSlotsInfo().count);

            for (const auto &slot : slots) {
                if (slot->getSlotType() == SlotType::analogTerminal) {
                    const auto terminalIdx = static_cast<size_t>(slot->getIndex());
                    if (analogTrait && analogTrait->terminalNames.size() > terminalIdx) {
                        slot->setName(analogTrait->terminalNames[terminalIdx]);
                    } else {
                        slot->setName("T" + std::to_string(terminalIdx));
                    }
                } else if (slot->getSlotType() == SlotType::digitalInput) {
                    if (inpDetails.names.size() > inSlotIdx)
                        slot->setName(inpDetails.names[inSlotIdx++]);
                    else
                        slot->setName(std::string(1, inpCh++));
                } else {
                    if (outDetails.names.size() > outSlotIdx)
                        slot->setName(outDetails.names[outSlotIdx++]);
                    else
                        slot->setName(std::string(1, outCh++));
                }
                createdComps.push_back(slot);
            }

            if (inpDetails.isResizeable) {
                auto slot = std::make_shared<SlotSceneComponent>();
                slot->setSlotType(SlotType::inputsResize);
                slot->setIndex(-1); // assign -1 for resize slots
                sceneComp->addInputSlot(slot->getUuid(), false);
                createdComps.push_back(slot);
            }

            if (outDetails.isResizeable) {
                auto slot = std::make_shared<SlotSceneComponent>();
                slot->setSlotType(SlotType::outputsResize);
                slot->setIndex(-1); // assign -1 for resize slots
                sceneComp->addOutputSlot(slot->getUuid(), false);
                createdComps.push_back(slot);
            }

            return createdComps;
        }

        // Creates the slots and also add there ids inside the components
        // input slots array and output slots array
        std::vector<std::shared_ptr<SlotSceneComponent>> createIOSlots(size_t inputCount,
                                                                       size_t outputCount);
        std::vector<std::shared_ptr<SlotSceneComponent>> createAnalogTerminalSlots(size_t terminalCount);

        void update(Bess::TimeMs timeStep, SceneState &state) override;

        void draw(SceneDrawContext &context) override;

        void drawSchematic(SceneDrawContext &context) override;

        void updateScales(const SceneState &state);

        std::vector<std::shared_ptr<SceneComponent>> clone(const SceneState &sceneState) const override;

        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)
        MAKE_GETTER_SETTER(UUID, NetId, m_netId)
        MAKE_GETTER_SETTER(std::vector<UUID>, InputSlots, m_inputSlots)
        MAKE_GETTER_SETTER(std::vector<UUID>, OutputSlots, m_outputSlots)
        MAKE_GETTER_SETTER(Transform, SchematicTransform, m_schematicTransform)
        MAKE_GETTER_SETTER(std::shared_ptr<SimEngine::ComponentDefinition>, CompDef, m_compDef)

        void setSchSlotsPosDirty(bool val = true);
        size_t getInputSlotsCount() const;
        size_t getOutputSlotsCount() const;

        void addInputSlot(UUID slotId, bool isLastResizeable = true);
        void addOutputSlot(UUID slotId, bool isLastResizeable = true);

        void setScaleDirty(bool val = true);

        void setSchematicScaleDirty(bool val = true);

        std::vector<UUID>
        cleanup(SceneState &state, UUID caller = UUID::null) override;

        void drawPropertiesUI(SceneState &state) override;
        void onAttach(SceneState &state) override;

        void onNameChanged() override;

        void onMouseDragged(const Events::MouseDraggedEvent &e) override;

        glm::vec3 getAbsolutePosition(const SceneState &state) const override;

        REG_SCENE_COMP_TYPE("SimulationSceneComponent", SceneComponentType::simulation)
        SCENE_COMP_SER(Bess::Canvas::SimulationSceneComponent,
                       Bess::Canvas::SceneComponent,
                       SIM_SC_SER_PROPS)

        std::vector<UUID> getDependants(const SceneState &state) const override;

        void drawBackground(SceneDrawContext &context);

        void drawSlots(SceneDrawContext &context);

        std::vector<SimEngine::LogicState> getInputStates(const SceneState &state) const;
        std::vector<SimEngine::LogicState> getOutputStates(const SceneState &state) const;
        bool isAnalogComponent() const;

        void onTransformChanged() override;

        std::vector<std::shared_ptr<SceneComponent>> cloneSimulationComponent(
            const SceneState &sceneState,
            const std::shared_ptr<SimulationSceneComponent> &clonedComponent) const;

      protected:
        /**
         * Resets the slot positions based on the current scale and number of slots
         * in the component.
         */
        void resetSlotPositions(const SceneState &state);

        /**
         * Resets the schematic pin positions based on the current schematic scale and number of slots
         * in the component. Will ignore slots that are resize slots for the schematic view.
         */
        void resetSchematicPinsPositions(const SceneState &state);

        // Generates the positions relative to the component position
        std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>>
        calculateSlotPositions(size_t inputCount, size_t outputCount) const;

        glm::vec2 calculateScale(const SceneState &state) override;

        virtual void calculateSchematicScale(const SceneState &state);

        void onChildrenChanged() override;

      protected:
        // Associated simulation engine ID
        UUID m_simEngineId = UUID::null;
        UUID m_netId = UUID::null;
        std::vector<UUID> m_inputSlots;
        std::vector<UUID> m_outputSlots;
        bool m_isScaleDirty = true, m_isSchematicScaleDirty = true;
        bool m_isSchSlotsPosDirty = true;
        Transform m_schematicTransform;
        std::shared_ptr<SimEngine::ComponentDefinition> m_compDef = nullptr;
    };
} // namespace Bess::Canvas

REG_SCENE_COMP(Bess::Canvas::SimulationSceneComponent,
               Bess::Canvas::SceneComponent, SIM_SC_SER_PROPS)
