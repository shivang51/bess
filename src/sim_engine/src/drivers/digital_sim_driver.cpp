#include "drivers/digital_sim_driver.h"
#include "drivers/event_based_sim_driver.h"
#include "expression_evalutator/expr_evaluator.h"
#include "json/value.h"

#include <algorithm>
#include <memory>

namespace Bess::SimEngine::Drivers::Digital {
    std::string DigitalSimDriver::getName() const {
        return "Digital Simulation Driver";
    }

    bool DigitalSimDriver::simulate(const SimEvt &evt) {
        const auto &id = evt.compId;

        const auto &comp = this->template getComponent<DigSimComp>(id);

        if (!comp) {
            BESS_WARN("(DigitalSimDriver.simulate) Component with UUID {} not found",
                      (uint64_t)id);
            return false;
        }

        auto simData = std::make_shared<DigCompSimData>();
        simData->simTime = m_currentSimTime;
        simData->prevState.inputStates = comp->getInputStates();
        simData->prevState.outputStates = comp->getOutputStates();
        simData->inputStates = collapseInputs(id);
        if (simData->outputStates.size() != comp->getOutputStates().size()) {
            simData->outputStates = comp->getOutputStates();
        }
        simData->expressions = &comp->getDefinition<DigCompDef>()->getOutputExpressions();

        BESS_ASSERT(simData->expressions, "Failed to set expressions ptr");

        auto newData = std::dynamic_pointer_cast<DigCompSimData>(
            comp->simulate(simData));

        if (!newData) {
            BESS_WARN("(DigitalSimDriver.simulate) Simulation function for component with UUID {} did not return DigCompSimData",
                      (uint64_t)id);
            return false;
        }

        comp->setInputStates(newData->inputStates);
        comp->setOutputStates(newData->outputStates);

        return newData->simDependants;
    }

    void DigitalSimDriver::addComponent(const std::shared_ptr<DigSimComp> &comp,
                                        bool scheduleSim) {
        EvtBasedSimDriver::addComponent(comp, scheduleSim);
        std::dynamic_pointer_cast<DigCompDef>(
            comp->getDefinition())
            ->computeExpressionsIfNeeded();
    }

    void DigitalSimDriver::onBeforeRun() {
        EvtBasedSimDriver::onBeforeRun();
        BESS_DEBUG("Starting DigitalSimDriver run loop");
    }

    std::shared_ptr<SimComponent> DigitalSimDriver::createComp(const std::shared_ptr<CompDef> &def) {
        if (!suuportsDef(def)) {
            BESS_WARN("(DigitalSimDriver.addComponent) Unsupported component definition type: {}",
                      def->getName());
            return nullptr;
        }

        const auto comp = DigSimComp::fromDef(def->clone());
        BESS_DEBUG("(DigitalSimDriver.addComponent) Created component '{}' with UUID {} from definition '{}'",
                   comp->getName(), (uint64_t)comp->getUuid(), def->getName());

        if (!comp) {
            BESS_WARN("(DigitalSimDriver.addComponent) Failed to create component from definition: {}",
                      def->getName());
            return nullptr;
        }

        addComponent(comp, false);

        return comp;
    }

    void DigitalSimDriver::onComponentAdded(const std::shared_ptr<SimComponent> &comp) {
        const auto digiComp = std::dynamic_pointer_cast<DigSimComp>(comp);
        if (!digiComp) {
            return;
        }

        Net net;
        net.addComponent(digiComp->getUuid());
        digiComp->setNetUuid(net.getUUID());
        m_nets[net.getUUID()] = net;
        m_isNetUpdated = true;
    }

    void DigitalSimDriver::deleteComponent(const UUID &uuid) {
        const auto comp = getComponent<DigSimComp>(uuid);
        if (!comp) {
            SimDriver::deleteComponent(uuid);
            return;
        }

        auto removeBackReferences = [&](Connections &pins, bool removeFromInputs) {
            for (const auto &pin : pins) {
                for (const auto &[otherId, otherIdx] : pin) {
                    const auto other = getComponent<DigSimComp>(otherId);
                    if (!other) {
                        continue;
                    }

                    auto &otherPins = removeFromInputs ? other->getInputConnections() : other->getOutputConnections();
                    if (otherIdx < 0 || static_cast<size_t>(otherIdx) >= otherPins.size()) {
                        continue;
                    }

                    auto &targetPin = otherPins[otherIdx];
                    std::erase_if(targetPin, [&](const auto &conn) {
                        return conn.first == uuid;
                    });
                }
            }
        };

        removeBackReferences(comp->getOutputConnections(), true);
        removeBackReferences(comp->getInputConnections(), false);

        const auto netId = comp->getNetUuid();
        if (m_nets.contains(netId)) {
            m_nets[netId].removeComponent(uuid);
            if (m_nets[netId].size() == 0) {
                m_nets.erase(netId);
            }
            m_isNetUpdated = true;
        }

        SimDriver::deleteComponent(uuid);
    }

    void DigitalSimDriver::clearComponents() {
        SimDriver::clearComponents();
        m_nets.clear();
        m_isNetUpdated = true;
    }

    std::pair<bool, std::string> DigitalSimDriver::canConnectComponents(
        const UUID &src, int srcSlotIdx, SlotType srcType,
        const UUID &dst, int dstSlotIdx, SlotType dstType) const {
        if (srcType == dstType) {
            return {false, "Cannot connect pins of the same type i.e. input -> input or output -> output"};
        }

        const auto &srcComp = getComponent<DigSimComp>(src);
        const auto &dstComp = getComponent<DigSimComp>(dst);

        if (!srcComp || !dstComp) {
            return {false, "Source or destination component does not exist in DigitalSimDriver"};
        }

        auto &outPins = srcType == SlotType::digitalOutput
                            ? srcComp->getOutputConnections()
                            : srcComp->getInputConnections();
        auto &inPins = dstType == SlotType::digitalInput
                           ? dstComp->getInputConnections()
                           : dstComp->getOutputConnections();

        if (srcSlotIdx < 0 || srcSlotIdx >= static_cast<int>(outPins.size())) {
            return {false, "Invalid source pin index. Valid range: 0 to " + std::to_string(outPins.size() - 1)};
        }
        if (dstSlotIdx < 0 || dstSlotIdx >= static_cast<int>(inPins.size())) {
            return {false, "Invalid destination pin index. Valid range: 0 to " + std::to_string(inPins.size() - 1)};
        }

        // Check for duplicate connection.
        auto &conns = outPins[srcSlotIdx];
        bool exists = std::ranges::any_of(conns, [&](const auto &conn) {
            return conn.first == dst && conn.second == dstSlotIdx;
        });

        return {!exists, exists ? "Connection already exists" : ""};
    }

    bool DigitalSimDriver::connectComponent(
        const UUID &src, int srcSlotIdx, SlotType srcType,
        const UUID &dst, int dstSlotIdx, SlotType dstType, bool overrideConn) {
        auto [canConnect, errorMsg] = canConnectComponents(src, srcSlotIdx, srcType, dst, dstSlotIdx, dstType);
        if (!canConnect && !(overrideConn && errorMsg == "Connection already exists")) {
            BESS_WARN("Cannot connect components: {}", errorMsg);
            return false;
        }

        if (!canConnect && overrideConn) {
            deleteConnection(src, srcType, srcSlotIdx, dst, dstType, dstSlotIdx);
        }

        const auto srcComp = getComponent<DigSimComp>(src);
        const auto dstComp = getComponent<DigSimComp>(dst);
        if (!srcComp || !dstComp) {
            return false;
        }

        auto &outPins = srcType == SlotType::digitalOutput
                            ? srcComp->getOutputConnections()
                            : srcComp->getInputConnections();
        auto &inPins = dstType == SlotType::digitalInput
                           ? dstComp->getInputConnections()
                           : dstComp->getOutputConnections();

        if (srcSlotIdx < 0 || dstSlotIdx < 0 ||
            static_cast<size_t>(srcSlotIdx) >= outPins.size() ||
            static_cast<size_t>(dstSlotIdx) >= inPins.size()) {
            return false;
        }

        outPins[srcSlotIdx].emplace_back(dst, dstSlotIdx);
        inPins[dstSlotIdx].emplace_back(src, srcSlotIdx);

        if (srcType == SlotType::digitalOutput && static_cast<size_t>(srcSlotIdx) < srcComp->getIsOutputConnected().size()) {
            srcComp->getIsOutputConnected()[srcSlotIdx] = true;
        }
        if (srcType == SlotType::digitalInput && static_cast<size_t>(srcSlotIdx) < srcComp->getIsInputConnected().size()) {
            srcComp->getIsInputConnected()[srcSlotIdx] = true;
        }
        if (dstType == SlotType::digitalInput && static_cast<size_t>(dstSlotIdx) < dstComp->getIsInputConnected().size()) {
            dstComp->getIsInputConnected()[dstSlotIdx] = true;
        }
        if (dstType == SlotType::digitalOutput && static_cast<size_t>(dstSlotIdx) < dstComp->getIsOutputConnected().size()) {
            dstComp->getIsOutputConnected()[dstSlotIdx] = true;
        }

        if (srcComp->getNetUuid() != dstComp->getNetUuid()) {
            UUID finalNetId = srcComp->getNetUuid();
            UUID movedNetId = dstComp->getNetUuid();

            if (m_nets.contains(finalNetId) && m_nets.contains(movedNetId) &&
                m_nets.at(finalNetId).size() < m_nets.at(movedNetId).size()) {
                std::swap(finalNetId, movedNetId);
            }

            if (m_nets.contains(finalNetId) && m_nets.contains(movedNetId)) {
                auto &finalNet = m_nets[finalNetId];
                auto &movedNet = m_nets[movedNetId];
                for (const auto &compUuid : movedNet.getComponents()) {
                    if (compUuid != UUID::null) {
                        if (const auto comp = getComponent<DigSimComp>(compUuid)) {
                            comp->setNetUuid(finalNetId);
                        }
                    }
                    finalNet.addComponent(compUuid);
                }
                m_nets.erase(movedNetId);
                m_isNetUpdated = true;
            }
        }

        propagateFromComponent(src);

        BESS_INFO("Connected components in DigitalSimDriver");
        return true;
    }

    void DigitalSimDriver::deleteConnection(
        const UUID &compA, SlotType pinAType, int idxA,
        const UUID &compB, SlotType pinBType, int idxB) {
        const auto compARef = getComponent<DigSimComp>(compA);
        const auto compBRef = getComponent<DigSimComp>(compB);
        if (!compARef || !compBRef) {
            return;
        }

        auto &pinsA = pinAType == SlotType::digitalInput
                          ? compARef->getInputConnections()
                          : compARef->getOutputConnections();
        auto &pinsB = pinBType == SlotType::digitalInput
                          ? compBRef->getInputConnections()
                          : compBRef->getOutputConnections();

        if (idxA < 0 || idxB < 0 ||
            static_cast<size_t>(idxA) >= pinsA.size() ||
            static_cast<size_t>(idxB) >= pinsB.size()) {
            return;
        }

        std::erase_if(pinsA[idxA], [&](const auto &c) {
            return c.first == compB && c.second == idxB;
        });

        std::erase_if(pinsB[idxB], [&](const auto &c) {
            return c.first == compA && c.second == idxA;
        });

        if (pinAType == SlotType::digitalOutput && static_cast<size_t>(idxA) < compARef->getIsOutputConnected().size()) {
            compARef->getIsOutputConnected()[idxA] = !pinsA[idxA].empty();
        }
        if (pinAType == SlotType::digitalInput && static_cast<size_t>(idxA) < compARef->getIsInputConnected().size()) {
            compARef->getIsInputConnected()[idxA] = !pinsA[idxA].empty();
        }
        if (pinBType == SlotType::digitalOutput && static_cast<size_t>(idxB) < compBRef->getIsOutputConnected().size()) {
            compBRef->getIsOutputConnected()[idxB] = !pinsB[idxB].empty();
        }
        if (pinBType == SlotType::digitalInput && static_cast<size_t>(idxB) < compBRef->getIsInputConnected().size()) {
            compBRef->getIsInputConnected()[idxB] = !pinsB[idxB].empty();
        }

        m_isNetUpdated = true;
        BESS_INFO("Deleted connection in DigitalSimDriver");
    }

    bool DigitalSimDriver::addSlot(const UUID &compId, SlotType type, int index) {
        const auto digComp = getComponent<DigSimComp>(compId);
        if (!digComp)
            return false;

        const auto digDef = digComp->getDefinition<DigCompDef>();
        if (!digDef)
            return false;

        const bool isInput = (type == SlotType::digitalInput);
        auto info = isInput ? digDef->getInputSlotsInfo() : digDef->getOutputSlotsInfo();

        if (!info.isResizeable)
            return false;

        if (!digDef->onSlotsResizeReq(isInput ? SlotsGroupType::input : SlotsGroupType::output, info.count + 1)) {
            return false;
        }

        if (isInput) {
            auto &states = digComp->getInputStates();
            auto &connections = digComp->getInputConnections();
            auto &connected = digComp->getIsInputConnected();
            states.insert(states.begin() + static_cast<long>(index), SlotState{});
            connections.insert(connections.begin() + static_cast<long>(index), std::vector<ComponentPin>{});
            connected.insert(connected.begin() + static_cast<long>(index), false);
            info.count += 1;
            digDef->setInputSlotsInfo(info);
        } else {
            auto &states = digComp->getOutputStates();
            auto &connections = digComp->getOutputConnections();
            auto &connected = digComp->getIsOutputConnected();
            states.insert(states.begin() + static_cast<long>(index), SlotState{});
            connections.insert(connections.begin() + static_cast<long>(index), std::vector<ComponentPin>{});
            connected.insert(connected.begin() + static_cast<long>(index), false);
            info.count += 1;
            digDef->setOutputSlotsInfo(info);
        }

        return true;
    }

    bool DigitalSimDriver::removeSlot(const UUID &compId, SlotType type, int index) {
        const auto digComp = getComponent<DigSimComp>(compId);
        if (!digComp)
            return false;

        const auto digDef = digComp->getDefinition<DigCompDef>();
        if (!digDef)
            return false;

        const bool isInput = (type == SlotType::digitalInput);
        auto info = isInput ? digDef->getInputSlotsInfo() : digDef->getOutputSlotsInfo();

        if (!info.isResizeable || info.count <= 0)
            return false;

        if (!digDef->onSlotsResizeReq(isInput ? SlotsGroupType::input : SlotsGroupType::output, info.count - 1)) {
            return false;
        }

        if (isInput) {
            auto &states = digComp->getInputStates();
            auto &connections = digComp->getInputConnections();
            auto &connected = digComp->getIsInputConnected();
            if (static_cast<size_t>(index) < states.size())
                states.erase(states.begin() + index);
            if (static_cast<size_t>(index) < connections.size())
                connections.erase(connections.begin() + index);
            if (static_cast<size_t>(index) < connected.size())
                connected.erase(connected.begin() + index);
            info.count -= 1;
            if (static_cast<size_t>(index) < info.names.size())
                info.names.erase(info.names.begin() + index);
            digDef->setInputSlotsInfo(info);
        } else {
            auto &states = digComp->getOutputStates();
            auto &connections = digComp->getOutputConnections();
            auto &connected = digComp->getIsOutputConnected();
            if (static_cast<size_t>(index) < states.size())
                states.erase(states.begin() + index);
            if (static_cast<size_t>(index) < connections.size())
                connections.erase(connections.begin() + index);
            if (static_cast<size_t>(index) < connected.size())
                connected.erase(connected.begin() + index);
            info.count -= 1;
            if (static_cast<size_t>(index) < info.names.size())
                info.names.erase(info.names.begin() + index);
            digDef->setOutputSlotsInfo(info);
        }

        return true;
    }

    ConnectionBundle DigitalSimDriver::getConnections(const UUID &uuid) const {
        ConnectionBundle bundle;
        const auto comp = getComponent<DigSimComp>(uuid);
        if (!comp) {
            return bundle;
        }

        bundle.inputs = comp->getInputConnections();
        bundle.outputs = comp->getOutputConnections();
        return bundle;
    }

    std::vector<UUID> DigitalSimDriver::getDependants(const UUID &id) {
        const auto comp = getComponent<DigSimComp>(id);
        if (!comp) {
            return {};
        }

        std::vector<UUID> dependants;
        for (const auto &pinConnections : comp->getOutputConnections()) {
            for (const auto &[targetId, targetInputIdx] : pinConnections) {
                if (targetId == UUID::null) {
                    continue;
                }

                if (std::ranges::find(dependants, targetId) == dependants.end()) {
                    dependants.emplace_back(targetId);
                }
            }
        }

        return dependants;
    }

    std::vector<SlotState> DigitalSimDriver::collapseInputs(const UUID &id) {
        const auto comp = getComponent<DigSimComp>(id);
        if (!comp) {
            return {};
        }

        auto collapsed = comp->getInputStates();
        const auto &inputConns = comp->getInputConnections();
        if (collapsed.size() < inputConns.size()) {
            collapsed.resize(inputConns.size());
        }

        for (size_t pinIdx = 0; pinIdx < inputConns.size(); ++pinIdx) {
            const auto &pinConns = inputConns[pinIdx];
            if (pinConns.empty()) {
                continue;
            }

            LogicState mergedState = LogicState::unknown;
            SimTime latestTs(0);
            bool anyKnown = false;

            for (const auto &[srcId, srcSlotIdx] : pinConns) {
                const auto srcComp = getComponent<DigSimComp>(srcId);
                if (!srcComp || srcSlotIdx < 0) {
                    continue;
                }

                if (static_cast<size_t>(srcSlotIdx) >= srcComp->getOutputStates().size()) {
                    continue;
                }

                const auto &srcState = srcComp->getOutputStates()[srcSlotIdx];
                latestTs = std::max(latestTs, srcState.lastChangeTime);
                if (srcState.state != LogicState::unknown) {
                    anyKnown = true;
                }
                if (srcState.state == LogicState::high) {
                    mergedState = LogicState::high;
                }
            }

            if (mergedState != LogicState::high) {
                mergedState = anyKnown ? LogicState::low : LogicState::unknown;
            }

            collapsed[pinIdx] = SlotState{mergedState, latestTs};
        }

        return collapsed;
    }

    std::vector<SlotState> DigitalSimDriver::getInputSlotsState(const UUID &compId) {
        return collapseInputs(compId);
    }

    SlotState DigitalSimDriver::getSlotState(const UUID &uuid, SlotType type, int idx) const {
        const auto comp = getComponent<DigSimComp>(uuid);
        if (!comp) {
            BESS_WARN("[getDigitalPinState] Component with UUID {} is invalid", (uint64_t)uuid);
            return {LogicState::unknown, SimTime(0)};
        }

        if (idx < 0) {
            BESS_WARN("[getDigitalPinState] Negative slot index {} for component {}", idx, (uint64_t)uuid);
            return {LogicState::unknown, SimTime(0)};
        }

        if (type == SlotType::digitalOutput) {
            if (static_cast<size_t>(idx) >= comp->getOutputStates().size()) {
                BESS_WARN("[getDigitalPinState] Output slot index {} out of range for component {}", idx, (uint64_t)uuid);
                return {LogicState::unknown, SimTime(0)};
            }
            return comp->getOutputStates()[idx];
        }

        if (static_cast<size_t>(idx) >= comp->getInputStates().size()) {
            BESS_WARN("[getDigitalPinState] Input slot index {} out of range for component {}", idx, (uint64_t)uuid);
            return {LogicState::unknown, SimTime(0)};
        }
        return comp->getInputStates()[idx];
    }

    bool DigitalSimDriver::setInputSlotState(
        const UUID &uuid,
        int pinIdx,
        LogicState state) {
        const auto comp = getComponent<DigSimComp>(uuid);
        if (!comp) {
            BESS_WARN("[setInputSlotState] Component with UUID {} is invalid", (uint64_t)uuid);
            return false;
        }

        auto &inputs = comp->getInputStates();

        if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= inputs.size()) {
            BESS_WARN("[setInputSlotState] Input slot index {} out of range for component {}",
                      pinIdx,
                      (uint64_t)uuid);
            return false;
        }

        inputs[pinIdx].state = state;
        inputs[pinIdx].lastChangeTime = m_currentSimTime;
        return true;
    }

    bool DigitalSimDriver::setOutputSlotState(
        const UUID &uuid,
        int pinIdx,
        LogicState state) {
        const auto comp = getComponent<DigSimComp>(uuid);
        if (!comp) {
            BESS_WARN("[setOutputSlotState] Component with UUID {} is invalid", (uint64_t)uuid);
            return false;
        }

        auto &outputs = comp->getOutputStates();

        if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= outputs.size()) {
            BESS_WARN("[setOutputSlotState] Output slot index {} out of range for component {}",
                      pinIdx,
                      (uint64_t)uuid);
            return false;
        }

        outputs[pinIdx].state = state;
        outputs[pinIdx].lastChangeTime = m_currentSimTime;

        propagateFromComponent(uuid);

        return true;
    }

    ComponentState DigitalSimDriver::getComponentState(const UUID &uuid) const {
        ComponentState state;
        const auto comp = getComponent<DigSimComp>(uuid);
        if (!comp) {
            return state;
        }

        state.inputStates = comp->getInputStates();
        state.outputStates = comp->getOutputStates();
        state.inputConnected = comp->getIsInputConnected();
        state.outputConnected = comp->getIsOutputConnected();
        return state;
    }

    const std::unordered_map<UUID, Net> &DigitalSimDriver::getNetsMap() const {
        return m_nets;
    }

    bool DigitalSimDriver::isNetUpdated() const {
        return m_isNetUpdated;
    }

    void DigitalSimDriver::clearNetUpdated() {
        m_isNetUpdated = false;
    }

    Json::Value DigCompDef::toJson() const {
        Json::Value json = EvtBasedCompDef::toJson();

        JsonConvert::toJsonValue(m_inputSlotsInfo, json["inpSlotsInfo"]);
        JsonConvert::toJsonValue(m_outputSlotsInfo, json["outSlotsInfo"]);
        JsonConvert::toJsonValue(m_opInfo, json["opInfo"]);
        JsonConvert::toJsonValue(m_behaviorType, json["behaviorType"]);
        JsonConvert::toJsonValue(m_outputExpressions, json["expressions"]);

        return json;
    }

    bool DigCompDef::onSlotsResizeReq(SlotsGroupType groupType, size_t newSize) {
        if (groupType == SlotsGroupType::input)
            return m_inputSlotsInfo.isResizeable;
        return m_outputSlotsInfo.isResizeable;
    }

    void DigCompDef::onStateChange(const ComponentState &oldState,
                                   const ComponentState &newState) {}

    void DigCompDef::onExpressionsChange() {}

    std::shared_ptr<CompDef> DigCompDef::clone() const {
        return std::make_shared<DigCompDef>(*this);
    }

    bool DigCompDef::computeExpressionsIfNeeded() {
        // operator '0' means no operation
        // if no operation is defined, no expressions to compute
        if (m_opInfo.op == '0') {
            return false;
        }

        if (m_inputSlotsInfo.count <= 0) {
            BESS_WARN("[SimulationEngine][ComponentDefinition] Input count not provided for expression(s) generation");
            return false;
        }

        m_outputExpressions.clear();

        // handeling unary and binary operators
        // For binary operators, only single output is supported
        // and for uniary operator, each input generates one output
        if (!ExprEval::isUninaryOperator(m_opInfo.op) &&
            m_outputSlotsInfo.count == 1) {
            std::string expr = m_opInfo.shouldNegateOutput ? "!(0" : "0";
            for (size_t i = 1; i < m_inputSlotsInfo.count; i++) {
                expr += m_opInfo.op + std::to_string(i);
            }
            if (m_opInfo.shouldNegateOutput)
                expr += ")";
            m_outputExpressions = {expr};
        } else if (ExprEval::isUninaryOperator(m_opInfo.op)) {
            m_outputExpressions.reserve(m_inputSlotsInfo.count);
            for (size_t i = 0; i < m_inputSlotsInfo.count; i++) {
                m_outputExpressions.emplace_back(std::format("{}{}", m_opInfo.op, i));
            }
        } else {
            BESS_ERROR("Invalid IO config for expression generation");
            assert(false);
        }

        onExpressionsChange();

        return true;
    }

    void DigCompDef::setSimFn(const TDigSimFn &simFn) {
        m_simFn = [simFn](const SimFnDataPtr &data) -> SimFnDataPtr {
            auto digData = std::dynamic_pointer_cast<DigCompSimData>(data);
            if (!digData) {
                BESS_WARN("(DigCompDef.setSimFn) Invalid data type passed to sim function");
                return nullptr;
            }
            return simFn(digData);
        };
    }

    std::string DigCompDef::getTypeName() const {
        return TypeName;
    }
} // namespace Bess::SimEngine::Drivers::Digital

namespace Bess::JsonConvert {
    void toJsonValue(Json::Value &json,
                     const Bess::SimEngine::Drivers::Digital::DigSimComp &data) {
        json = data.toJson();
    }
} // namespace Bess::JsonConvert
