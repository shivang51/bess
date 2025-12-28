#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include "component_definition.h"
#include "types.h"
#include <entt/entt.hpp>
#include <memory>
#include <vector>

namespace Bess::SimEngine {

    struct BESS_API DigitalComponent {
        DigitalComponent() = default;
        DigitalComponent(const DigitalComponent &) = default;
        DigitalComponent(const std::shared_ptr<ComponentDefinition> &def) {
            definition = def->clone();
            definition->computeExpressionsIfNeeded();
            state.inputStates.resize(definition->getInputSlotsInfo().count,
                                     {LogicState::low, SimTime(0)});
            state.outputStates.resize(definition->getOutputSlotsInfo().count,
                                      {LogicState::low, SimTime(0)});
            state.inputConnected.resize(state.inputStates.size(), false);
            state.outputConnected.resize(state.outputStates.size(), false);
            state.auxData = &definition->getAuxData();
            inputConnections.resize(state.inputStates.size());
            outputConnections.resize(state.outputStates.size());
        }

        size_t incrementInputCount() {
            if (!definition->onSlotsResizeReq(SlotsGroupType::input,
                                              definition->getInputSlotsInfo().count + 1)) {
                return definition->getInputSlotsInfo().count;
            }

            definition->getInputSlotsInfo().count += 1;
            state.inputStates.emplace_back();
            state.inputConnected.emplace_back(false);
            inputConnections.emplace_back();
            definition->computeExpressionsIfNeeded();
            definition->computeHash();
            return definition->getInputSlotsInfo().count;
        }

        size_t incrementOutputCount() {
            if (!definition->onSlotsResizeReq(SlotsGroupType::output,
                                              definition->getOutputSlotsInfo().count + 1)) {
                return definition->getOutputSlotsInfo().count;
            }

            definition->getOutputSlotsInfo().count += 1;
            state.outputStates.emplace_back();
            state.outputConnected.emplace_back(false);
            outputConnections.emplace_back();
            definition->computeExpressionsIfNeeded();
            definition->computeHash();
            return definition->getOutputSlotsInfo().count;
        }

        UUID id;
        UUID netUuid = UUID::null;
        ComponentState state;
        std::shared_ptr<ComponentDefinition> definition;
        Connections inputConnections;
        Connections outputConnections;
    };

} // namespace Bess::SimEngine
