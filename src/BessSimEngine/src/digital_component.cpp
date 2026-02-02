#include "digital_component.h"
#include "event_dispatcher.h"
#include "events/sim_engine_events.h"
#include "logger.h"
#include "types.h"

namespace Bess::SimEngine {
    DigitalComponent::DigitalComponent(const std::shared_ptr<ComponentDefinition> &def) {
        definition = def->clone();
        definition->computeExpressionsIfNeeded();
        definition->computeHash();
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

        auto &inputsInfo = definition->getInputSlotsInfo();
        inputsInfo.count += 1;
        if (!inputsInfo.names.empty() && inputsInfo.names.back().size() == 1) {
            char ch = inputsInfo.names.back()[0];
            inputsInfo.names.emplace_back(1, ch + 1);
        }
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

        auto &outputsInfo = definition->getOutputSlotsInfo();
        outputsInfo.count += 1;
        if (!outputsInfo.names.empty() && outputsInfo.names.back().size() == 1) {
            char ch = outputsInfo.names.back()[0];
            outputsInfo.names.emplace_back(1, ch + 1);
        }
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

namespace Bess::JsonConvert {
    void toJsonValue(Json::Value &j, const Bess::SimEngine::DigitalComponent &comp) {
        toJsonValue(comp.id, j["id"]);
        toJsonValue(comp.netUuid, j["net_uuid"]);
        toJsonValue(*comp.definition, j["definition"]);
        toJsonValue(comp.state, j["state"]);
        toJsonValue(comp.inputConnections, j["input_connections"]);
        toJsonValue(comp.outputConnections, j["output_connections"]);
    }

    void fromJsonValue(const Json::Value &j, Bess::SimEngine::DigitalComponent &comp) {
        fromJsonValue(j["id"], comp.id);
        fromJsonValue(j["net_uuid"], comp.netUuid);
        fromJsonValue(j["definition"], comp.definition);
        fromJsonValue(j["state"], comp.state);
        comp.state.auxData = &comp.definition->getAuxData();
        fromJsonValue(j["input_connections"], comp.inputConnections);
        fromJsonValue(j["output_connections"], comp.outputConnections);
    }
} // namespace Bess::JsonConvert
