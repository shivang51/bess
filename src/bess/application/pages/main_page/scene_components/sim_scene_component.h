#pragma once

#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_state/components/behaviours/drag_behaviour.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_state/components/scene_component_types.h"
#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_ui/controls/label_comp.h"
#include "bess_core/scene/scene_ui/layout.h"
#include "bess_core/settings/viewport_theme.h"
#include "common/bess_uuid.h"
#include "scene_comp_types.h"
#include "sim_driver/sim_driver.h"
#include "slot_scene_component.h"

#define SIM_SC_SER_PROPS                                                       \
    ("simEngineId", getSimEngineId, setSimEngineId),                           \
        ("netId", getNetId, setNetId),                                         \
        ("inputSlots", getInputSlots, setInputSlots),                          \
        ("outputSlots", getOutputSlots, setOutputSlots),                       \
        ("schematicTransform", getSchematicTransform, setSchematicTransform)

namespace Bess::Canvas {
    class SimulationSceneComponent
        : public SceneComponent,
          public DragBehaviour<SimulationSceneComponent> {
      public:
        SimulationSceneComponent();
        SimulationSceneComponent(const SimulationSceneComponent &other) =
            default;
        ~SimulationSceneComponent() override = default;

        // Create a new SimSceneComp
        // [0] -> Component itself
        // [1...] -> Created slots
        static std::vector<std::shared_ptr<SceneComponent>>
        createNew(const std::shared_ptr<SimEngine::Drivers::CompDef> &compDef);

        template <typename T = SimulationSceneComponent>
        static std::vector<std::shared_ptr<SceneComponent>>
        createNew(const std::shared_ptr<SimEngine::Drivers::CompDef> &compDef) {
            std::vector<std::shared_ptr<SceneComponent>> createdComps;

            const UUID uuid;
            std::shared_ptr<T> sceneComp = std::make_shared<T>();
            sceneComp->setCompDef(compDef);

            createdComps.push_back(sceneComp);

            // setting the name before adding to scene state, so that event
            // listeners can access it
            sceneComp->setName(compDef->getName());

            // style
            auto &style = sceneComp->getStyle();

            style.headerColor =
                ViewportTheme::getCompHeaderColor(compDef->getGroupName());

            const auto inpDetails = compDef->getInputPortDescriptor();
            const auto outDetails = compDef->getOutputPortDescriptor();
            const auto inputSignalKind =
                inpDetails.signalKind == SimEngine::SignalKind::none
                    ? SimEngine::SignalKind::digital
                    : inpDetails.signalKind;
            const auto outputSignalKind =
                outDetails.signalKind == SimEngine::SignalKind::none
                    ? SimEngine::SignalKind::digital
                    : outDetails.signalKind;

            int inSlotIdx = 0, outSlotIdx = 0;
            char inpCh = 'A', outCh = 'a';

            const auto slots = sceneComp->createIOSlots(inpDetails, outDetails);

            for (const auto &slot : slots) {
                if (slot->isInputSlot()) {
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
                slot->setPortDirection(SimEngine::PortDirection::input);
                slot->setSignalKind(inputSignalKind);
                slot->setResizeTrigger(true);
                slot->setIndex(-1); // assign -1 for resize slots
                sceneComp->addInputSlot(slot->getUuid(), false);
                createdComps.push_back(slot);
            }

            if (outDetails.isResizeable) {
                auto slot = std::make_shared<SlotSceneComponent>();
                slot->setPortDirection(SimEngine::PortDirection::output);
                slot->setSignalKind(outputSignalKind);
                slot->setResizeTrigger(true);
                slot->setIndex(-1); // assign -1 for resize slots
                sceneComp->addOutputSlot(slot->getUuid(), false);
                createdComps.push_back(slot);
            }

            return createdComps;
        }

        // Creates the slots and also add there ids inside the components
        // input slots array and output slots array
        std::vector<std::shared_ptr<SlotSceneComponent>>
        createIOSlots(const SimEngine::PortDescriptor &inputDescriptor,
                      const SimEngine::PortDescriptor &outputDescriptor);

        std::vector<std::shared_ptr<SlotSceneComponent>>
        createIOSlots(size_t inputCount, size_t outputCount);

        void update(Bess::TimeMs timeStep, SceneState &state) override;

        void draw(SceneDrawContext &context) override;

        void drawSchematic(SceneDrawContext &context) override;

        void updateScales(const SceneState &state);

        std::vector<std::shared_ptr<SceneComponent>>
        clone(const SceneState &sceneState) const override;

        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)
        MAKE_GETTER_SETTER(UUID, NetId, m_netId)
        MAKE_GETTER_SETTER(Transform, SchematicTransform, m_schematicTransform)
        MAKE_GETTER_SETTER(std::shared_ptr<SimEngine::Drivers::CompDef>,
                           CompDef,
                           m_compDef)

        const std::vector<UUID> &getInputSlots() const;
        void setInputSlots(const std::vector<UUID> &slotIds);

        const std::vector<UUID> &getOutputSlots() const;
        void setOutputSlots(const std::vector<UUID> &slotIds);

        void setSchSlotsPosDirty(bool val = true);
        size_t getInputSlotsCount() const;
        size_t getOutputSlotsCount() const;

        void addInputSlot(UUID slotId, bool isLastResizeable = true);
        void addOutputSlot(UUID slotId, bool isLastResizeable = true);
        void insertInputSlot(UUID slotId, size_t index);
        void insertOutputSlot(UUID slotId, size_t index);
        bool removeInputSlot(UUID slotId);
        bool removeOutputSlot(UUID slotId);

        void setScaleDirty(bool val = true);

        void setSchematicScaleDirty(bool val = true);

        std::vector<UUID> cleanup(SceneState &state,
                                  UUID caller = UUID::null) override;

        void drawPropertiesUI(SceneState &state) override;
        void onAttach(SceneState &state) override;

        void onNameChanged() override;

        void onMouseDragged(const Events::MouseDraggedEvent &e) override;

        glm::vec3 getAbsolutePosition(const SceneState &state,
                                      bool isSchematicMode) const override;

        REG_SCENE_COMP_TYPE("SimulationSceneComponent",
                            SceneComponentType::simulation)
        SCENE_COMP_SER(Bess::Canvas::SimulationSceneComponent,
                       Bess::Canvas::SceneComponent,
                       SIM_SC_SER_PROPS)

        std::vector<UUID> getDependants(const SceneState &state) const override;

        void drawBackground(SceneDrawContext &context);

        void drawSlots(SceneDrawContext &context);

        std::vector<SimEngine::LogicState>
        getInputStates(const SceneState &state) const;
        std::vector<SimEngine::LogicState>
        getOutputStates(const SceneState &state) const;

        void onTransformChanged() override;

        std::vector<std::shared_ptr<SceneComponent>>
        cloneSimulationComponent(const SceneState &sceneState,
                                 const std::shared_ptr<SimulationSceneComponent>
                                     &clonedComponent) const;

        glm::vec2 calculateScale(const SceneState &state) override;

        float getSlotStartY() const;

        void prepareUI(SceneUIPrepareCtx &ctx) override;

        std::shared_ptr<UI::ContainerComp> getInputSlotsContainer() const {
            return m_inpSlotsContainer;
        }

        std::shared_ptr<UI::ContainerComp> getOutputSlotsContainer() const {
            return m_outSlotsContainer;
        }

      protected:
        /**
         * Resets the schematic pin positions based on the current schematic
         * scale and number of slots in the component. Will ignore slots that
         * are resize slots for the schematic view.
         */
        void resetSchematicPinsPositions(const SceneState &state);

        // Generates the positions relative to the component position
        virtual void calculateSchematicScale(const SceneState &state);

        void onChildrenChanged() override;

        void resetCloneRuntimeState() override;

        void markSlotsUIDirty();

      protected:
        // Associated simulation engine ID
        UUID m_simEngineId = UUID::null;
        UUID m_netId = UUID::null;
        std::vector<UUID> m_inputSlots;
        std::vector<UUID> m_outputSlots;
        bool m_isSchematicScaleDirty = true;
        bool m_isSchSlotsPosDirty = true;
        Transform m_schematicTransform;
        std::shared_ptr<SimEngine::Drivers::CompDef> m_compDef = nullptr;

        std::shared_ptr<Bess::Canvas::UI::ContainerComp> m_nodeContainer =
                                                             nullptr,
                                                         m_slotsContainer =
                                                             nullptr,
                                                         m_inpSlotsContainer =
                                                             nullptr,
                                                         m_outSlotsContainer =
                                                             nullptr;

        std::shared_ptr<Bess::Canvas::UI::LabelComp> m_labelComp = nullptr;

        static uint32_t s_nodeShader;
        static size_t s_instanceCount;
    };
} // namespace Bess::Canvas

REG_SCENE_COMP(Bess::Canvas::SimulationSceneComponent,
               Bess::Canvas::SceneComponent,
               SIM_SC_SER_PROPS)
