#include "digital_component.h"
#include "event_dispatcher.h"
#include "events/sim_engine_events.h"

namespace Bess::SimEngine {
    DigitalComponent::DigitalComponent(const std::shared_ptr<ComponentDefinition> &def) {
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

    size_t DigitalComponent::incrementInputCount() {
        if (!definition->onSlotsResizeReq(SlotsGroupType::input,
                                          definition->getInputSlotsInfo().count + 1)) {
            return definition->getInputSlotsInfo().count;
        }

        definition->getInputSlotsInfo().count += 1;
        state.inputStates.emplace_back();
        state.inputConnected.emplace_back(false);
        inputConnections.emplace_back();
        const bool exprComputed = definition->computeExpressionsIfNeeded();

        const auto growthPolicy = definition->getIOGrowthPolicy();

        if (exprComputed &&
            definition->getOutputSlotsInfo().count !=
                definition->getOutputExpressions().size()) {
            // if expressions were recomputed,
            // but output count does not match expressions size
            // we need to resize output related states and connections
            size_t newOutputCount = definition->getOutputExpressions().size();
            size_t oldOutputCount = definition->getOutputSlotsInfo().count;
            definition->getOutputSlotsInfo().count = newOutputCount;
            state.outputStates.resize(newOutputCount,
                                      SlotState{LogicState::low, SimTime(0)});
            state.outputConnected.resize(newOutputCount, false);
            outputConnections.resize(newOutputCount);

            EventSystem::EventDispatcher::instance().dispatch(
                Events::CompDefOutputsResizedEvent{this->id});
        } else if (growthPolicy == CompDefIOGrowthPolicy::eq &&
                   definition->getOutputSlotsInfo().count !=
                       definition->getInputSlotsInfo().count) {
            incrementOutputCount();
        }

        EventSystem::EventDispatcher::instance().dispatch(
            Events::CompDefInputsResizedEvent{this->id});

        definition->computeHash();
        return definition->getInputSlotsInfo().count;
    }

    size_t DigitalComponent::incrementOutputCount() {
        if (!definition->onSlotsResizeReq(SlotsGroupType::output,
                                          definition->getOutputSlotsInfo().count + 1)) {
            return definition->getOutputSlotsInfo().count;
        }

        definition->getOutputSlotsInfo().count += 1;
        state.outputStates.emplace_back();
        state.outputConnected.emplace_back(false);
        outputConnections.emplace_back();
        definition->computeHash();

        const auto growthPolicy = definition->getIOGrowthPolicy();
        if (growthPolicy == CompDefIOGrowthPolicy::eq &&
            definition->getOutputSlotsInfo().count !=
                definition->getInputSlotsInfo().count) {

            incrementInputCount();
        }

        EventSystem::EventDispatcher::instance().dispatch(
            Events::CompDefOutputsResizedEvent{this->id});
        return definition->getOutputSlotsInfo().count;
    }

    size_t DigitalComponent::decrementInputCount() {
        if (!definition->onSlotsResizeReq(SlotsGroupType::input,
                                          definition->getInputSlotsInfo().count - 1)) {
            return definition->getInputSlotsInfo().count;
        }

        definition->getInputSlotsInfo().count -= 1;
        state.inputStates.pop_back();
        state.inputConnected.pop_back();
        inputConnections.pop_back();

        const auto growthPolicy = definition->getIOGrowthPolicy();
        const bool exprComputed = definition->computeExpressionsIfNeeded();

        if (exprComputed &&
            definition->getOutputSlotsInfo().count !=
                definition->getOutputExpressions().size()) {
            // if expressions were recomputed,
            // but output count does not match expressions size
            // we need to resize output related states and connections
            size_t newOutputCount = definition->getOutputExpressions().size();
            size_t oldOutputCount = definition->getOutputSlotsInfo().count;
            definition->getOutputSlotsInfo().count = newOutputCount;
            state.outputStates.resize(newOutputCount,
                                      SlotState{LogicState::low, SimTime(0)});
            state.outputConnected.resize(newOutputCount, false);
            outputConnections.resize(newOutputCount);

            EventSystem::EventDispatcher::instance().dispatch(
                Events::CompDefOutputsResizedEvent{this->id});
        } else if (growthPolicy == CompDefIOGrowthPolicy::eq &&
                   definition->getOutputSlotsInfo().count !=
                       definition->getInputSlotsInfo().count) {
            decrementOutputCount();
        }
        definition->computeHash();
        EventSystem::EventDispatcher::instance().dispatch(
            Events::CompDefInputsResizedEvent{this->id});
        return definition->getInputSlotsInfo().count;
    }

    size_t DigitalComponent::decrementOutputCount() {
        if (!definition->onSlotsResizeReq(SlotsGroupType::output,
                                          definition->getOutputSlotsInfo().count - 1)) {
            return definition->getOutputSlotsInfo().count;
        }

        definition->getOutputSlotsInfo().count -= 1;
        state.outputStates.pop_back();
        state.outputConnected.pop_back();
        outputConnections.pop_back();
        definition->computeHash();

        const auto growthPolicy = definition->getIOGrowthPolicy();
        if (growthPolicy == CompDefIOGrowthPolicy::eq &&
            definition->getOutputSlotsInfo().count !=
                definition->getInputSlotsInfo().count) {
            decrementInputCount();
        }

        EventSystem::EventDispatcher::instance().dispatch(
            Events::CompDefOutputsResizedEvent{this->id});

        return definition->getOutputSlotsInfo().count;
    }
} // namespace Bess::SimEngine
