#include "module_def.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "component_catalog.h"
#include "component_definition.h"
#include "simulation_engine.h"
#include <cstdint>

namespace Bess::SimEngine {

    ComponentState ModuleDefinition::simulationFunction(const std::vector<SlotState> &inputs,
                                                        SimTime simTime,
                                                        const ComponentState &prevState) {
        ComponentState newState = prevState;
        bool isInputChanged = false;
        for (size_t i = 0; i < inputs.size(); ++i) {
            newState.inputStates[i] = inputs[i];
            isInputChanged = isInputChanged || (inputs[i].state != prevState.inputStates[i].state);
        }

        auto &simEngine = SimulationEngine::instance();

        if (isInputChanged) {
            for (int i = 0; i < inputs.size(); ++i) {
                simEngine.setOutputSlotState(m_input, i, inputs[i].state);
            }
        }

        const auto &outputState = simEngine.getComponentState(m_output);
        bool isChanged = false;
        for (size_t i = 0; i < outputState.inputStates.size(); ++i) {
            newState.outputStates[i] = outputState.inputStates[i];
            if (newState.outputStates[i].state != prevState.outputStates[i].state) {
                isChanged = true;
            }
        }
        newState.isChanged = isChanged;
        return newState;
    }

    std::shared_ptr<ModuleDefinition> ModuleDefinition::createNew() {
        auto &simEngine = SimulationEngine::instance();

        auto moduleDef = std::make_shared<ModuleDefinition>();

        moduleDef->setName("New Module");
        moduleDef->setGroupName("Modules");

        moduleDef->m_inputSlotsInfo = SlotsGroupInfo{
            .type = SlotsGroupType::input,
            .isResizeable = true,
            .count = 1,
        };
        moduleDef->m_outputSlotsInfo = SlotsGroupInfo{
            .type = SlotsGroupType::output,
            .isResizeable = true,
            .count = 1,
        };

        moduleDef->m_simulationFunction = [moduleDef](const std::vector<SlotState> &inputs,
                                                      SimTime simTime,
                                                      const ComponentState &prevState) {
            return moduleDef->simulationFunction(inputs, simTime, prevState);
        };

        const auto &catalog = ComponentCatalog::instance();

        // create a input and output component for the module
        const auto &inpDef = catalog.getComponentDefinition(5271179154965332885);
        BESS_ASSERT(inpDef, "Input component definition not found in catalog");
        moduleDef->m_input = simEngine.addComponent(inpDef);

        const auto &outDef = catalog.getComponentDefinition(15124334025293992558ULL);
        BESS_ASSERT(outDef, "Output component definition not found in catalog");
        moduleDef->m_output = simEngine.addComponent(outDef);

        return moduleDef;
    }
} // namespace Bess::SimEngine
