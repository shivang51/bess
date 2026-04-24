#include "drivers/digital_sim_driver.h"
#include "simulation_engine.h"

namespace Bess::SimEngine::Drivers::Digital {
    std::string DigitalSimDriver::getName() const {
        return "Digital Simulation Driver";
    }

    bool DigitalSimDriver::simulate(const SimEvt &evt) {
        const auto &id = evt.compId;

        const auto &comp = this->template getComponent<DigitalSimComponent>(id);

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
        simData->outputStates = comp->getOutputStates();

        auto newData = std::dynamic_pointer_cast<DigCompSimData>(
            comp->simulate(simData));

        if (!newData) {
            BESS_WARN("(DigitalSimDriver.simulate) Simulation function for component with UUID {} did not return DigCompSimData",
                      (uint64_t)id);
            return false;
        }

        comp->setOutputStates(newData->outputStates);

        return newData->simDependants;
    }

    void DigitalSimDriver::addComponent(const std::shared_ptr<DigitalSimComponent> &comp,
                                        bool scheduleSim) {
        EvtBasedSimDriver::addComponent(comp, scheduleSim);
    }

    void DigitalSimDriver::onBeforeRun() {
        EvtBasedSimDriver::onBeforeRun();
        BESS_DEBUG("Starting DigitalSimDriver run loop");
    }

    std::shared_ptr<SimComponent> DigitalSimDriver::createComp(const std::shared_ptr<ComponentDef> &def) {
        if (!suuportsDef(def)) {
            BESS_WARN("(DigitalSimDriver.addComponent) Unsupported component definition type: {}",
                      def->getName());
            return nullptr;
        }

        const auto comp = DigitalSimComponent::fromDef(def->clone());
        BESS_DEBUG("(DigitalSimDriver.addComponent) Created component '{}' with UUID {} from definition '{}'",
                   comp->getName(), (uint64_t)comp->getUuid(), def->getName());

        if (!comp) {
            BESS_WARN("(DigitalSimDriver.addComponent) Failed to create component from definition: {}",
                      def->getName());
            return nullptr;
        }

        // Engine-managed propagation is synchronous; avoid queueing a bootstrap event
        // for every newly created component from the simulation engine.
        addComponent(comp, false);

        return comp;
    }

    std::pair<bool, std::string> DigitalSimDriver::canConnectComponents(
        Bess::SimEngine::SimulationEngine &engine, const UUID &src, int srcSlotIdx, SlotType srcType,
        const UUID &dst, int dstSlotIdx, SlotType dstType) const {
        if (srcType == dstType) {
            return {false, "Cannot connect pins of the same type i.e. input -> input or output -> output"};
        }

        const auto &srcComp = getComponent<DigitalSimComponent>(src);
        const auto &dstComp = getComponent<DigitalSimComponent>(dst);

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
        Bess::SimEngine::SimulationEngine &engine, const UUID &src, int srcSlotIdx, SlotType srcType,
        const UUID &dst, int dstSlotIdx, SlotType dstType, bool overrideConn) {
        auto [canConnect, errorMsg] = canConnectComponents(engine, src, srcSlotIdx, srcType, dst, dstSlotIdx, dstType);
        if (!canConnect && !(overrideConn && errorMsg == "Connection already exists")) {
            BESS_WARN("Cannot connect components: {}", errorMsg);
            return false;
        }

        if (!canConnect && overrideConn) {
            deleteConnection(engine, src, srcType, srcSlotIdx, dst, dstType, dstSlotIdx);
        }

        const auto srcComp = getComponent<DigitalSimComponent>(src);
        const auto dstComp = getComponent<DigitalSimComponent>(dst);
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

        auto &m_nets = engine.getNetsMapMutable();

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
                        if (const auto comp = getComponent<DigitalSimComponent>(compUuid)) {
                            comp->setNetUuid(finalNetId);
                        }
                    }
                    finalNet.addComponent(compUuid);
                }
                m_nets.erase(movedNetId);
                engine.setNetUpdated(true);
            }
        }

        const auto shouldPropagate = engine.getSimulationState() == SimulationState::running;
        if (shouldPropagate) {
            engine.triggerPropagation(src);
        } else {
            engine.markPendingSignalSource(src);
        }

        BESS_INFO("Connected components in DigitalSimDriver");
        return true;
    }

    void DigitalSimDriver::deleteConnection(
        Bess::SimEngine::SimulationEngine &engine, const UUID &compA, SlotType pinAType, int idxA,
        const UUID &compB, SlotType pinBType, int idxB) {
        const auto compARef = getComponent<DigitalSimComponent>(compA);
        const auto compBRef = getComponent<DigitalSimComponent>(compB);
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

        engine.setNetUpdated(true);
        BESS_INFO("Deleted connection in DigitalSimDriver");
    }

    bool DigitalSimDriver::addSlot(Bess::SimEngine::SimulationEngine &engine, const UUID &compId, SlotType type, int index) {
        const auto digComp = getComponent<DigitalSimComponent>(compId);
        if (!digComp) return false;
        
        const auto digDef = digComp->getDefinition<DigCompDef>();
        if (!digDef) return false;
        
        const bool isInput = (type == SlotType::digitalInput);
        auto info = isInput ? digDef->getInputSlotsInfo() : digDef->getOutputSlotsInfo();
        
        if (!info.isResizeable) return false;
        
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

    bool DigitalSimDriver::removeSlot(Bess::SimEngine::SimulationEngine &engine, const UUID &compId, SlotType type, int index) {
        const auto digComp = getComponent<DigitalSimComponent>(compId);
        if (!digComp) return false;
        
        const auto digDef = digComp->getDefinition<DigCompDef>();
        if (!digDef) return false;
        
        const bool isInput = (type == SlotType::digitalInput);
        auto info = isInput ? digDef->getInputSlotsInfo() : digDef->getOutputSlotsInfo();
        
        if (!info.isResizeable || info.count <= 0) return false;
        
        if (!digDef->onSlotsResizeReq(isInput ? SlotsGroupType::input : SlotsGroupType::output, info.count - 1)) {
            return false;
        }
        
        if (isInput) {
            auto &states = digComp->getInputStates();
            auto &connections = digComp->getInputConnections();
            auto &connected = digComp->getIsInputConnected();
            if (static_cast<size_t>(index) < states.size()) states.erase(states.begin() + index);
            if (static_cast<size_t>(index) < connections.size()) connections.erase(connections.begin() + index);
            if (static_cast<size_t>(index) < connected.size()) connected.erase(connected.begin() + index);
            info.count -= 1;
            if (static_cast<size_t>(index) < info.names.size()) info.names.erase(info.names.begin() + index);
            digDef->setInputSlotsInfo(info);
        } else {
            auto &states = digComp->getOutputStates();
            auto &connections = digComp->getOutputConnections();
            auto &connected = digComp->getIsOutputConnected();
            if (static_cast<size_t>(index) < states.size()) states.erase(states.begin() + index);
            if (static_cast<size_t>(index) < connections.size()) connections.erase(connections.begin() + index);
            if (static_cast<size_t>(index) < connected.size()) connected.erase(connected.begin() + index);
            info.count -= 1;
            if (static_cast<size_t>(index) < info.names.size()) info.names.erase(info.names.begin() + index);
            digDef->setOutputSlotsInfo(info);
        }
        
        return true;
    }
} // namespace Bess::SimEngine::Drivers::Digital

namespace Bess::JsonConvert {
    void toJsonValue(Json::Value &json,
                     const Bess::SimEngine::Drivers::Digital::DigitalSimComponent &data) {
        json = data.toJson();
    }
} // namespace Bess::JsonConvert
