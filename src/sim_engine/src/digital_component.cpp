#include "digital_component.h"
#include "common/helpers.h"
#include "event_dispatcher.h"
#include "events/sim_engine_events.h"
#include "module_def.h"
#include "types.h"
#include <memory>

namespace Bess::SimEngine {
    DigitalComponent::DigitalComponent(const std::shared_ptr<ComponentDefinition> &def,
                                       bool cloneDef) {

        if (cloneDef) {
            definition = def->clone();
        } else {
            definition = def;
        }

        m_name = Common::Helpers::toUpperCase(definition->getName().substr(0, 3));
        if (m_name[1] == ' ')
            m_name[1] = '_';

        if (m_name.size() < 3) {
            m_name.resize(3, '_');
        }

        const auto count = getNameCountMap()[m_name];
        getNameCountMap()[m_name] += 1;

        m_name += "_" + std::to_string(count);

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

    size_t DigitalComponent::incrementInputCount(bool force) {
        if (!force && !definition->onSlotsResizeReq(SlotsGroupType::input,
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

            EventSystem::EventDispatcher::instance().queue(
                Events::CompDefOutputsResizedEvent{this->id});
        } else if (growthPolicy == CompDefIOGrowthPolicy::eq &&
                   definition->getOutputSlotsInfo().count !=
                       definition->getInputSlotsInfo().count) {
            incrementOutputCount();
        }

        dispatchInputSlotCountChange(definition->getInputSlotsInfo().count);

        EventSystem::EventDispatcher::instance().queue(
            Events::CompDefInputsResizedEvent{this->id});

        definition->computeHash();
        return definition->getInputSlotsInfo().count;
    }

    size_t DigitalComponent::incrementOutputCount(bool force) {
        if (!force && !definition->onSlotsResizeReq(SlotsGroupType::output,
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

        dispatchOutputSlotCountChange(definition->getOutputSlotsInfo().count);

        EventSystem::EventDispatcher::instance().queue(
            Events::CompDefOutputsResizedEvent{this->id});
        return definition->getOutputSlotsInfo().count;
    }

    size_t DigitalComponent::decrementInputCount(bool force) {
        if (!force && !definition->onSlotsResizeReq(SlotsGroupType::input,
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

            dispatchOutputSlotCountChange(newOutputCount);
            EventSystem::EventDispatcher::instance().queue(
                Events::CompDefOutputsResizedEvent{this->id});
        } else if (growthPolicy == CompDefIOGrowthPolicy::eq &&
                   definition->getOutputSlotsInfo().count !=
                       definition->getInputSlotsInfo().count) {
            decrementOutputCount();
        }
        definition->computeHash();

        dispatchInputSlotCountChange(definition->getInputSlotsInfo().count);
        EventSystem::EventDispatcher::instance().queue(
            Events::CompDefInputsResizedEvent{this->id});
        return definition->getInputSlotsInfo().count;
    }

    size_t DigitalComponent::decrementOutputCount(bool force) {
        if (!force && !definition->onSlotsResizeReq(SlotsGroupType::output,
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

        definition->computeHash();

        dispatchOutputSlotCountChange(definition->getOutputSlotsInfo().count);

        EventSystem::EventDispatcher::instance().queue(
            Events::CompDefOutputsResizedEvent{this->id});

        return definition->getOutputSlotsInfo().count;
    }

    void DigitalComponent::dispatchStateChange(ComponentState &oldState, ComponentState &newState) {
        for (const auto &cb : m_onStateChangeCbs) {
            cb.second(oldState, newState);
        }
    }

    void DigitalComponent::dispatchInputSlotCountChange(size_t newCount) {
        for (const auto &cb : m_onInputSlotCountChangeCbs) {
            cb.second(newCount);
        }
    }

    void DigitalComponent::dispatchOutputSlotCountChange(size_t newCount) {
        for (const auto &cb : m_onOutputSlotCountChangeCbs) {
            cb.second(newCount);
        }
    }

    void DigitalComponent::clearCallbacks() {
        m_onInputSlotCountChangeCbs.clear();
        m_onOutputSlotCountChangeCbs.clear();
        m_onStateChangeCbs.clear();
    }

    void DigitalComponent::addOnStateChangeCB(const UUID &id, const TOnStateChangeCB &cb) {
        m_onStateChangeCbs.emplace_back(id, cb);
    }

    void DigitalComponent::addOnInputSlotCountChangeCB(const UUID &id,
                                                       const TOnSlotCountChangeCB &cb) {
        m_onInputSlotCountChangeCbs.emplace_back(id, cb);
    }

    void DigitalComponent::addOnOutputSlotCountChangeCB(const UUID &id,
                                                        const TOnSlotCountChangeCB &cb) {
        m_onOutputSlotCountChangeCbs.emplace_back(id, cb);
    }

    void DigitalComponent::removeOnStateChangeCB(const UUID &id) {
        std::erase_if(m_onStateChangeCbs, [id](const auto &cb) {
            return cb.first == id;
        });
    }

    void DigitalComponent::removeOnInputSlotCountChangeCB(const UUID &id) {
        std::erase_if(m_onInputSlotCountChangeCbs, [id](const auto &cb) {
            return cb.first == id;
        });
    }

    void DigitalComponent::removeOnOutputSlotCountChangeCB(const UUID &id) {
        std::erase_if(m_onOutputSlotCountChangeCbs, [id](const auto &cb) {
            return cb.first == id;
        });
    }

    std::unordered_map<std::string, int> &DigitalComponent::getNameCountMap() {
        static std::unordered_map<std::string, int> nameCountMap;
        return nameCountMap;
    }
} // namespace Bess::SimEngine

namespace Bess::JsonConvert {
    void toJsonValue(Json::Value &j, const Bess::SimEngine::DigitalComponent &comp) {
        toJsonValue(comp.id, j["id"]);
        toJsonValue(comp.netUuid, j["net_uuid"]);
        auto moduleDef = std::dynamic_pointer_cast<SimEngine::ModuleDefinition>(comp.definition);
        if (moduleDef) {
            toJsonValue(*moduleDef, j["definition"]);
            j["definition"]["is_module"] = true;
        } else {
            toJsonValue(*comp.definition, j["definition"]);
        }
        toJsonValue(comp.state, j["state"]);
        toJsonValue(comp.inputConnections, j["input_connections"]);
        toJsonValue(comp.outputConnections, j["output_connections"]);
        j["name"] = comp.getName();
    }

    void fromJsonValue(const Json::Value &j, Bess::SimEngine::DigitalComponent &comp) {
        fromJsonValue(j["id"], comp.id);
        fromJsonValue(j["net_uuid"], comp.netUuid);

        if (j.isMember("name")) {
            comp.setName(j["name"].asString());
        }

        if (j["definition"].isMember("is_module")) {
            auto modDef = std::make_shared<SimEngine::ModuleDefinition>();
            fromJsonValue(j["definition"], modDef);
            comp.definition = std::move(modDef);
        } else {
            comp.definition = std::make_shared<SimEngine::ComponentDefinition>();
            fromJsonValue(j["definition"], comp.definition);
        }
        fromJsonValue(j["state"], comp.state);
        comp.state.auxData = &comp.definition->getAuxData();
        fromJsonValue(j["input_connections"], comp.inputConnections);
        fromJsonValue(j["output_connections"], comp.outputConnections);
    }
} // namespace Bess::JsonConvert
