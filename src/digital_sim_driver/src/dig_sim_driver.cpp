#include "dig_sim_driver.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "common/types.h"
#include "component_catalog.h"
#include "dig_module_def.h"
#include "driver_registry.h"
#include "expression_evalutator/expr_evaluator.h"
#include "sim_driver/event_based_sim_driver.h"
#include "sim_driver/sim_driver.h"
#include "simulation_engine.h"
#include "json/value.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace Bess::SimEngine::Drivers::Digital {

    static const struct DigSimDriverLoader {
        DigSimDriverLoader() {
            DriverRegistry::registerDriver(DigitalSimDriver::NAME, []() {
                return std::make_shared<DigitalSimDriver>();
            });
        }

        ~DigSimDriverLoader() {
            DriverRegistry::unregisterDriver(DigitalSimDriver::NAME);
        }
    } g_driverLoader;

    typedef std::shared_ptr<Drivers::Digital::DigCompSimData> TSimFnDataPtr;

    TSimFnDataPtr exprEvalSimFunc(const TSimFnDataPtr &simData) {
        bool changed = false;
        const auto *expressions = simData->expressions;

        BESS_ASSERT(expressions,
                    "Expressions cannot be null in exprEvalSimFunc");

        const auto &inputs = simData->inputStates;
        const auto &prevState = simData->prevState;

        auto &newOuts = simData->outputStates;

        BESS_ASSERT(
            newOuts.size() == expressions->size(),
            "[ExprEval] Output states size must match expressions size");

        for (int i = 0; i < (int)expressions->size(); i++) {
            std::vector<bool> states;
            states.reserve(inputs.size());
            for (auto &state : inputs)
                states.emplace_back(state.isHigh());
            bool newStateBool =
                ExprEval::evaluateExpression(expressions->at(i), states);
            changed =
                changed || prevState.outputStates[i].isHigh() != newStateBool;
            newOuts[i] = PortState::digital(newStateBool ? LogicState::high
                                                         : LogicState::low,
                                            simData->simTime);
        }

        simData->simDependants = changed;

        return simData;
    }

    namespace {
        std::shared_ptr<DigCompDef> loadDef(const Json::Value &defJson) {
            if (!defJson.isObject()) {
                throw std::runtime_error(
                    "digital component definition must be an object");
            }

            const auto defName = defJson.get("name", "").asString();
            const auto defTypeName = defJson.get("typeName", "").asString();

            std::shared_ptr<DigCompDef> def;

            if (defTypeName == ModuleDefinition::TypeName) {
                def = std::make_shared<ModuleDefinition>();
                auto moduleDef =
                    std::dynamic_pointer_cast<ModuleDefinition>(def);
                moduleDef->setSimFn(
                    [moduleDef](
                        const ModuleDefinition::TDigSimFnDataPtr &data) {
                        return moduleDef->simFunction(data);
                    });
            } else if (!defName.empty()) {
                const auto baseDef = SimEngine::ComponentCatalog::instance()
                                         .getComponentDefinition(defName);
                if (baseDef) {
                    def =
                        std::dynamic_pointer_cast<DigCompDef>(baseDef->clone());
                }
            }

            if (!def) {
                throw std::runtime_error(
                    "unknown digital component definition '" + defName +
                    "' (type '" + defTypeName + "')");
            }

            def->loadJson(defJson);

            if (!def->getSimFn() && (defJson.isMember("opInfo") ||
                                     defJson.isMember("expressions"))) {
                def->setSimFn(exprEvalSimFunc);
            }

            if (!def->getSimFn()) {
                throw std::runtime_error("digital component definition '" +
                                         def->getName() +
                                         "' has no simulation function");
            }

            return def;
        }

        Connections &connectionsFor(DigSimComp &comp, PortDirection direction) {
            return direction == PortDirection::input
                       ? comp.getInputConnections()
                       : comp.getOutputConnections();
        }

        const Connections &connectionsFor(const DigSimComp &comp,
                                          PortDirection direction) {
            return direction == PortDirection::input
                       ? comp.getInputConnections()
                       : comp.getOutputConnections();
        }

        std::vector<PortState> &statesFor(DigSimComp &comp,
                                          PortDirection direction) {
            return direction == PortDirection::input ? comp.getInputStates()
                                                     : comp.getOutputStates();
        }

        const std::vector<PortState> &statesFor(const DigSimComp &comp,
                                                PortDirection direction) {
            return direction == PortDirection::input ? comp.getInputStates()
                                                     : comp.getOutputStates();
        }

        std::vector<bool> &connectedFor(DigSimComp &comp,
                                        PortDirection direction) {
            return direction == PortDirection::input
                       ? comp.getIsInputConnected()
                       : comp.getIsOutputConnected();
        }

        PortDirection oppositeDirection(PortDirection direction) {
            if (direction == PortDirection::input) {
                return PortDirection::output;
            }
            if (direction == PortDirection::output) {
                return PortDirection::input;
            }
            return PortDirection::none;
        }

        SlotsGroupType slotsGroupTypeFor(PortDirection direction) {
            return direction == PortDirection::input ? SlotsGroupType::input
                                                     : SlotsGroupType::output;
        }

        PortCountChangeRes changeResFor(PortDirection direction) {
            return direction == PortDirection::input
                       ? PortCountChangeRes::inputsChanged()
                       : PortCountChangeRes::outputsChanged();
        }

        bool isSupportedDigitalPort(const PortRef &port) {
            return port.isValid() && port.signalKind == SignalKind::digital &&
                   (port.direction == PortDirection::input ||
                    port.direction == PortDirection::output);
        }

        void markPortConnection(DigSimComp &comp,
                                PortDirection direction,
                                int index,
                                bool connected) {
            auto &connectedList = connectedFor(comp, direction);
            auto &stateList = statesFor(comp, direction);
            if (index < 0 ||
                static_cast<size_t>(index) >= connectedList.size()) {
                return;
            }

            connectedList[index] = connected;
            if (!connected && static_cast<size_t>(index) < stateList.size()) {
                stateList[index].connState = ConnectionState::high_z;
            } else if (connected &&
                       static_cast<size_t>(index) < stateList.size()) {
                stateList[index].connState = ConnectionState::driven;
            }
        }

        PortDescriptor portDescriptorFor(const SlotsGroupInfo &info,
                                         PortDirection direction) {
            return {.direction = direction,
                    .signalKind = SignalKind::digital,
                    .quantityKind = QuantityKind::logic,
                    .unit = "",
                    .count = info.count,
                    .names = info.names,
                    .isResizeable = info.isResizeable};
        }
    } // namespace

    std::string DigitalSimDriver::getName() const {
        return DigitalSimDriver::NAME;
    }

    bool DigitalSimDriver::simulate(const SimEvt &evt,
                                    const std::vector<PortState> &inputs) {
        const auto &id = evt.compId;

        const auto &comp = this->template getComponent<DigSimComp>(id);

        if (!comp) {
            BESS_WARN(
                "(DigitalSimDriver.simulate) Component with UUID {} not found",
                (uint64_t)id);
            return false;
        }

        auto simData = std::make_shared<DigCompSimData>();
        simData->simTime = m_currentSimTime;
        simData->prevState.inputStates = comp->getInputStates();
        simData->prevState.outputStates = comp->getOutputStates();
        simData->inputStates = inputs;
        if (simData->outputStates.size() != comp->getOutputStates().size()) {
            simData->outputStates = comp->getOutputStates();
        }
        simData->expressions =
            &comp->getDefinition<DigCompDef>()->getOutputExpressions();

        BESS_ASSERT(simData->expressions, "Failed to set expressions ptr");

        auto newData =
            std::dynamic_pointer_cast<DigCompSimData>(comp->simulate(simData));

        if (!newData) {
            BESS_WARN("(DigitalSimDriver.simulate) Simulation function for "
                      "component with UUID {} did not return DigCompSimData",
                      (uint64_t)id);
            return false;
        }

        comp->setInputStates(newData->inputStates);
        comp->setOutputStates(newData->outputStates);

        comp->onPostSimulate();

        if (newData->simDependants) {
            for (const auto &[_, fn] : comp->getOnStateChangeCbs()) {
                fn(newData->inputStates, newData->outputStates);
            }
        }

        return newData->simDependants;
    }

    UUID
    DigitalSimDriver::addComponent(const std::shared_ptr<SimComponent> &comp,
                                   bool scheduleSim) {
        EvtBasedSimDriver::addComponent(comp, scheduleSim);
        return comp->getUuid();
    }

    void DigitalSimDriver::onBeforeRun() {
        EvtBasedSimDriver::onBeforeRun();
        BESS_DEBUG("Starting DigitalSimDriver run loop");
    }

    bool DigitalSimDriver::isSimStable() const {
        if (m_events.empty()) {
            return true;
        }

        if (m_events.size() == 1) {
            const auto &evt = *m_events.begin();
            auto comp = getComponent<DigSimComp>(evt.compId);
            return comp &&
                   comp->getDefinition<DigCompDef>()->getAutoReschedule();
        }

        return false;
    }

    std::shared_ptr<SimComponent>
    DigitalSimDriver::createComp(const std::shared_ptr<CompDef> &def,
                                 bool cloneDef) {
        if (!supportsDef(def)) {
            BESS_WARN("(DigitalSimDriver.addComponent) Unsupported component "
                      "definition type: {}",
                      def->getName());
            return nullptr;
        }

        bool isModule =
            std::dynamic_pointer_cast<ModuleDefinition>(def) != nullptr;
        if (isModule) {
            std::dynamic_pointer_cast<ModuleDefinition>(def)->setEngine(
                getEngine());
        }

        const auto comp =
            isModule ? DigSimComp::fromDef<DigModuleSimComp>(def, cloneDef)
                     : DigSimComp::fromDef(def, cloneDef);

        if (!comp) {
            BESS_WARN("(DigitalSimDriver.addComponent) Failed to create "
                      "component from definition: {}",
                      def->getName());
            return nullptr;
        }

        BESS_DEBUG("(DigitalSimDriver.addComponent) Created component '{}' "
                   "with UUID {} from definition '{}'",
                   comp->getName(),
                   (uint64_t)comp->getUuid(),
                   def->getName());

        return comp;
    }

    void DigitalSimDriver::onComponentAdded(
        const std::shared_ptr<SimComponent> &comp) {
        const auto digiComp = std::dynamic_pointer_cast<DigSimComp>(comp);
        if (!digiComp) {
            return;
        }

        BESS_ASSERT(digiComp->getDefinition(),
                    "Component definition cannot be null for digital sim "
                    "component with UUID {}",
                    (uint64_t)comp->getUuid());

        const auto digDef =
            std::dynamic_pointer_cast<DigCompDef>(comp->getDefinition());

        BESS_ASSERT(digDef,
                    "Component definition for component with UUID {} is "
                    "not of type DigCompDef",
                    (uint64_t)comp->getUuid());

        digDef->computeExpressionsIfNeeded();

        Net net;
        net.addComponent(digiComp->getUuid());
        digiComp->setNetUuid(net.getUUID());
        m_nets[net.getUUID()] = net;
        m_isNetUpdated = true;
    }

    void DigitalSimDriver::deleteComponent(const UUID &uuid) {
        const auto comp = getComponent<DigSimComp>(uuid);
        if (!comp) {
            EvtBasedSimDriver::deleteComponent(uuid);
            return;
        }

        auto removeBackReferences = [&](Connections &pins,
                                        bool removeFromInputs) {
            for (const auto &pin : pins) {
                for (const auto &[otherId, otherIdx] : pin) {
                    const auto other = getComponent<DigSimComp>(otherId);
                    if (!other) {
                        continue;
                    }

                    auto &otherPins = removeFromInputs
                                          ? other->getInputConnections()
                                          : other->getOutputConnections();
                    if (otherIdx < 0 ||
                        static_cast<size_t>(otherIdx) >= otherPins.size()) {
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

        EvtBasedSimDriver::deleteComponent(uuid);
    }

    void DigitalSimDriver::clearComponents() {
        EvtBasedSimDriver::clearComponents();
        m_nets.clear();
        m_isNetUpdated = true;
    }

    std::pair<bool, std::string>
    DigitalSimDriver::canConnectPorts(const PortRef &src,
                                      const PortRef &dst) const {
        if (!isSupportedDigitalPort(src) || !isSupportedDigitalPort(dst)) {
            return {false, "DigitalSimDriver only supports digital ports"};
        }

        if (src.direction == dst.direction) {
            return {false,
                    "Cannot connect pins of the same type i.e. input -> "
                    "input or output -> output"};
        }

        const auto &srcComp = getComponent<DigSimComp>(src.componentId);
        const auto &dstComp = getComponent<DigSimComp>(dst.componentId);

        if (!srcComp || !dstComp) {
            return {false,
                    "Source or destination component does not exist in "
                    "DigitalSimDriver"};
        }

        auto &outPins = connectionsFor(*srcComp, src.direction);
        auto &inPins = connectionsFor(*dstComp, dst.direction);

        if (src.index < 0 || src.index >= static_cast<int>(outPins.size())) {
            return {false,
                    "Invalid source pin index. Valid range: 0 to " +
                        std::to_string(outPins.size() - 1)};
        }
        if (dst.index < 0 || dst.index >= static_cast<int>(inPins.size())) {
            return {false,
                    "Invalid destination pin index. Valid range: 0 to " +
                        std::to_string(inPins.size() - 1)};
        }

        // Check for duplicate connection.
        auto &conns = outPins[src.index];
        bool exists = std::ranges::any_of(conns, [&](const auto &conn) {
            return conn.first == dst.componentId && conn.second == dst.index;
        });

        return {!exists, exists ? "Connection already exists" : ""};
    }

    bool DigitalSimDriver::connectPorts(const PortRef &src,
                                        const PortRef &dst,
                                        bool overrideConn) {
        auto [canConnect, errorMsg] = canConnectPorts(src, dst);
        if (!canConnect &&
            !(overrideConn && errorMsg == "Connection already exists")) {
            BESS_WARN("Cannot connect components: {}", errorMsg);
            return false;
        }

        if (!canConnect && overrideConn) {
            deleteConnection(src, dst);
        }

        const auto srcComp = getComponent<DigSimComp>(src.componentId);
        const auto dstComp = getComponent<DigSimComp>(dst.componentId);
        if (!srcComp || !dstComp) {
            return false;
        }

        auto &outPins = connectionsFor(*srcComp, src.direction);
        auto &inPins = connectionsFor(*dstComp, dst.direction);

        if (src.index < 0 || dst.index < 0 ||
            static_cast<size_t>(src.index) >= outPins.size() ||
            static_cast<size_t>(dst.index) >= inPins.size()) {
            return false;
        }

        outPins[src.index].emplace_back(dst.componentId, dst.index);
        inPins[dst.index].emplace_back(src.componentId, src.index);

        markPortConnection(*srcComp, src.direction, src.index, true);
        markPortConnection(*dstComp, dst.direction, dst.index, true);

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
                        if (const auto comp =
                                getComponent<DigSimComp>(compUuid)) {
                            comp->setNetUuid(finalNetId);
                        }
                    }
                    finalNet.addComponent(compUuid);
                }
                m_nets.erase(movedNetId);
                m_isNetUpdated = true;
            }
        }

        const auto &inputPort = src.isInput() ? src : dst;
        const auto &outputPort = src.isOutput() ? src : dst;
        scheduleEvt(inputPort.componentId,
                    m_currentSimTime,
                    outputPort.componentId,
                    true);

        BESS_INFO("Connected components in DigitalSimDriver");
        return true;
    }

    void DigitalSimDriver::deleteConnection(const PortRef &portA,
                                            const PortRef &portB) {
        const auto compARef = getComponent<DigSimComp>(portA.componentId);
        const auto compBRef = getComponent<DigSimComp>(portB.componentId);
        if (!compARef || !compBRef) {
            return;
        }

        auto &pinsA = connectionsFor(*compARef, portA.direction);
        auto &pinsB = connectionsFor(*compBRef, portB.direction);

        if (portA.index < 0 || portB.index < 0 ||
            static_cast<size_t>(portA.index) >= pinsA.size() ||
            static_cast<size_t>(portB.index) >= pinsB.size()) {
            return;
        }

        std::erase_if(pinsA[portA.index], [&](const auto &c) {
            return c.first == portB.componentId && c.second == portB.index;
        });

        std::erase_if(pinsB[portB.index], [&](const auto &c) {
            return c.first == portA.componentId && c.second == portA.index;
        });

        const bool stillAConnected = !pinsA[portA.index].empty();
        const bool stillBConnected = !pinsB[portB.index].empty();

        markPortConnection(
            *compARef, portA.direction, portA.index, stillAConnected);
        markPortConnection(
            *compBRef, portB.direction, portB.index, stillBConnected);

        const auto &inputPort = portA.isInput() ? portA : portB;
        scheduleEvt(inputPort.componentId, m_currentSimTime, UUID::null, true);

        m_isNetUpdated = true;
        BESS_INFO("Deleted connection in DigitalSimDriver");
    }

    PortCountChangeRes DigitalSimDriver::addPort(const PortRef &port,
                                                 bool force) {
        if (!isSupportedDigitalPort(port)) {
            return PortCountChangeRes::noChange();
        }

        const auto compId = port.componentId;
        const auto index = port.index;
        const auto direction = port.direction;
        const bool isInput = port.isInput();
        const auto digComp = getComponent<DigSimComp>(compId);
        if (!digComp) {
            BESS_WARN(
                "(DigitalSimDriver.addPort) Component with UUID {} not found",
                (uint64_t)compId);
            return PortCountChangeRes::noChange();
        }

        const auto digDef = digComp->getDefinition<DigCompDef>();
        if (!digDef) {
            BESS_WARN("(DigitalSimDriver.addPort) Component definition for "
                      "component with UUID {} is not a DigCompDef",
                      (uint64_t)compId);
            return PortCountChangeRes::noChange();
        }

        auto info = isInput ? digDef->getInputSlotsInfo()
                            : digDef->getOutputSlotsInfo();

        if (!force && !info.isResizeable) {
            BESS_WARN("(DigitalSimDriver.addPort) Ports of type {} for "
                      "component with UUID {} are not resizeable",
                      isInput ? "input" : "output",
                      (uint64_t)compId);
            return PortCountChangeRes::noChange();
        }

        if (!force && !digDef->onSlotsResizeReq(slotsGroupTypeFor(direction),
                                                info.count + 1)) {
            BESS_WARN("(DigitalSimDriver.addPort) Component definition for "
                      "component with UUID {} rejected slot resize request",
                      (uint64_t)compId);
            return PortCountChangeRes::noChange();
        }

        if (isInput) {
            auto &states = digComp->getInputStates();
            auto &connections = digComp->getInputConnections();
            auto &connected = digComp->getIsInputConnected();
            if (static_cast<size_t>(index) > states.size()) {
                return PortCountChangeRes::noChange();
            }
            states.insert(states.begin() + static_cast<long>(index),
                          PortState{});
            connections.insert(connections.begin() + static_cast<long>(index),
                               std::vector<ComponentPin>{});
            connected.insert(connected.begin() + static_cast<long>(index),
                             false);
            info.count += 1;
            digDef->setInputSlotsInfo(info);

            if (digDef->getKeepIOCountEq()) {
                auto &outStates = digComp->getOutputStates();
                auto &outConnections = digComp->getOutputConnections();
                auto &outConnected = digComp->getIsOutputConnected();
                outStates.insert(outStates.begin() + static_cast<long>(index),
                                 PortState{});
                outConnections.insert(outConnections.begin() +
                                          static_cast<long>(index),
                                      std::vector<ComponentPin>{});
                outConnected.insert(
                    outConnected.begin() + static_cast<long>(index), false);
                auto outInfo = digDef->getOutputSlotsInfo();
                outInfo.count += 1;
                digDef->setOutputSlotsInfo(outInfo);
            }

        } else {
            auto &states = digComp->getOutputStates();
            auto &connections = digComp->getOutputConnections();
            auto &connected = digComp->getIsOutputConnected();
            if (static_cast<size_t>(index) > states.size()) {
                return PortCountChangeRes::noChange();
            }
            states.insert(states.begin() + static_cast<long>(index),
                          PortState{});
            connections.insert(connections.begin() + static_cast<long>(index),
                               std::vector<ComponentPin>{});
            connected.insert(connected.begin() + static_cast<long>(index),
                             false);
            info.count += 1;
            digDef->setOutputSlotsInfo(info);

            if (digDef->getKeepIOCountEq()) {
                auto &inStates = digComp->getInputStates();
                auto &inConnections = digComp->getInputConnections();
                auto &inConnected = digComp->getIsInputConnected();
                inStates.insert(inStates.begin() + static_cast<long>(index),
                                PortState{});
                inConnections.insert(inConnections.begin() +
                                         static_cast<long>(index),
                                     std::vector<ComponentPin>{});
                inConnected.insert(
                    inConnected.begin() + static_cast<long>(index), false);
                auto inInfo = digDef->getInputSlotsInfo();
                inInfo.count += 1;
                digDef->setInputSlotsInfo(inInfo);
            }
        }

        digDef->computeExpressionsIfNeeded();

        triggerPortCountChangeCbs(
            compId, direction, port.signalKind, (int)info.count);

        if (digDef->getKeepIOCountEq()) {
            triggerPortCountChangeCbs(compId,
                                      oppositeDirection(direction),
                                      port.signalKind,
                                      (int)info.count);

            return PortCountChangeRes::bothChanged();
        }

        return changeResFor(direction);
    }

    PortCountChangeRes DigitalSimDriver::removePort(const PortRef &port,
                                                    bool force) {
        if (!isSupportedDigitalPort(port)) {
            return PortCountChangeRes::noChange();
        }

        const auto compId = port.componentId;
        const auto index = port.index;
        const auto direction = port.direction;
        const bool isInput = port.isInput();
        const auto digComp = getComponent<DigSimComp>(compId);
        if (!digComp)
            return PortCountChangeRes::noChange();

        const auto digDef = digComp->getDefinition<DigCompDef>();
        if (!digDef)
            return PortCountChangeRes::noChange();

        auto info = isInput ? digDef->getInputSlotsInfo()
                            : digDef->getOutputSlotsInfo();

        if ((!force && !info.isResizeable) || info.count <= 0)
            return PortCountChangeRes::noChange();

        if (!force && !digDef->onSlotsResizeReq(slotsGroupTypeFor(direction),
                                                info.count - 1)) {
            return PortCountChangeRes::noChange();
        }

        if (isInput) {
            auto &states = digComp->getInputStates();
            auto &connections = digComp->getInputConnections();
            auto &connected = digComp->getIsInputConnected();
            if (index < 0 || static_cast<size_t>(index) >= states.size()) {
                return PortCountChangeRes::noChange();
            }
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

            if (digDef->getKeepIOCountEq()) {
                auto &outStates = digComp->getOutputStates();
                auto &outConnections = digComp->getOutputConnections();
                auto &outConnected = digComp->getIsOutputConnected();
                if (static_cast<size_t>(index) < outStates.size())
                    outStates.erase(outStates.begin() + index);
                if (static_cast<size_t>(index) < outConnections.size())
                    outConnections.erase(outConnections.begin() + index);
                if (static_cast<size_t>(index) < outConnected.size())
                    outConnected.erase(outConnected.begin() + index);
                auto outInfo = digDef->getOutputSlotsInfo();
                outInfo.count -= 1;
                if (static_cast<size_t>(index) < outInfo.names.size())
                    outInfo.names.erase(outInfo.names.begin() + index);
                digDef->setOutputSlotsInfo(outInfo);
            }

        } else {
            auto &states = digComp->getOutputStates();
            auto &connections = digComp->getOutputConnections();
            auto &connected = digComp->getIsOutputConnected();
            if (index < 0 || static_cast<size_t>(index) >= states.size()) {
                return PortCountChangeRes::noChange();
            }
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

            if (digDef->getKeepIOCountEq()) {
                auto &inStates = digComp->getInputStates();
                auto &inConnections = digComp->getInputConnections();
                auto &inConnected = digComp->getIsInputConnected();
                if (static_cast<size_t>(index) < inStates.size())
                    inStates.erase(inStates.begin() + index);
                if (static_cast<size_t>(index) < inConnections.size())
                    inConnections.erase(inConnections.begin() + index);
                if (static_cast<size_t>(index) < inConnected.size())
                    inConnected.erase(inConnected.begin() + index);
                auto inInfo = digDef->getInputSlotsInfo();
                inInfo.count -= 1;
                if (static_cast<size_t>(index) < inInfo.names.size())
                    inInfo.names.erase(inInfo.names.begin() + index);
                digDef->setInputSlotsInfo(inInfo);
            }
        }

        digDef->computeExpressionsIfNeeded();

        triggerPortCountChangeCbs(
            compId, direction, port.signalKind, (int)info.count);

        if (digDef->getKeepIOCountEq()) {
            triggerPortCountChangeCbs(compId,
                                      oppositeDirection(direction),
                                      port.signalKind,
                                      (int)info.count);

            return PortCountChangeRes::bothChanged();
        }

        return changeResFor(direction);
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

                if (std::ranges::find(dependants, targetId) ==
                    dependants.end()) {
                    dependants.emplace_back(targetId);
                }
            }
        }

        return dependants;
    }

    std::vector<PortState> DigitalSimDriver::collapseInputs(const UUID &id) {
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
                collapsed[pinIdx] = LogicState::low;
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

                if (static_cast<size_t>(srcSlotIdx) >=
                    srcComp->getOutputStates().size()) {
                    continue;
                }

                const auto &srcState = srcComp->getOutputStates()[srcSlotIdx];
                latestTs = std::max(latestTs, srcState.lastChangeTime);
                if (srcState.getLogicState() != LogicState::unknown) {
                    anyKnown = true;
                }
                if (srcState.getLogicState() == LogicState::high) {
                    mergedState = LogicState::high;
                }
            }

            if (mergedState != LogicState::high) {
                mergedState = anyKnown ? LogicState::low : LogicState::unknown;
            }

            collapsed[pinIdx] = PortState{mergedState, latestTs};
        }

        return collapsed;
    }

    std::vector<PortState>
    DigitalSimDriver::getInputPortStates(const UUID &compId) {
        return collapseInputs(compId);
    }

    PortState DigitalSimDriver::getPortState(const PortRef &port) const {
        const auto comp = getComponent<DigSimComp>(port.componentId);
        if (!comp) {
            BESS_WARN("[getDigitalPinState] Component with UUID {} is invalid",
                      (uint64_t)port.componentId);
            return {LogicState::unknown, SimTime(0)};
        }

        if (!isSupportedDigitalPort(port)) {
            BESS_WARN("[getDigitalPinState] Unsupported port reference for "
                      "component {}",
                      (uint64_t)port.componentId);
            return {LogicState::unknown, SimTime(0)};
        }

        if (port.index < 0) {
            BESS_WARN(
                "[getDigitalPinState] Negative slot index {} for component {}",
                port.index,
                (uint64_t)port.componentId);
            return {LogicState::unknown, SimTime(0)};
        }

        const auto &states = statesFor(*comp, port.direction);
        if (port.isOutput()) {
            if (static_cast<size_t>(port.index) >= states.size()) {
                BESS_WARN("[getDigitalPinState] Output slot index {} out of "
                          "range for component {}",
                          port.index,
                          (uint64_t)port.componentId);
                return {LogicState::unknown, SimTime(0)};
            }
            return states[port.index];
        }

        if (static_cast<size_t>(port.index) >= states.size()) {
            BESS_WARN("[getPortState] Input port index {} out of range "
                      "for component {}",
                      port.index,
                      (uint64_t)port.componentId);
            return {LogicState::unknown, SimTime(0)};
        }
        return states[port.index];
    }

    bool DigitalSimDriver::setInputPortState(const UUID &uuid,
                                             int pinIdx,
                                             const PortState &state) {
        const auto comp = getComponent<DigSimComp>(uuid);
        if (!comp) {
            BESS_WARN("[setInputPortState] Component with UUID {} is invalid",
                      (uint64_t)uuid);
            return false;
        }

        if (!state.isDigital()) {
            BESS_WARN("[setInputPortState] DigitalSimDriver only accepts "
                      "digital port states");
            return false;
        }

        auto &inputs = comp->getInputStates();

        if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= inputs.size()) {
            BESS_WARN("[setInputPortState] Input port index {} out of range "
                      "for component {}",
                      pinIdx,
                      (uint64_t)uuid);
            return false;
        }

        inputs[pinIdx] = state;
        inputs[pinIdx].lastChangeTime = m_currentSimTime;
        return true;
    }

    bool DigitalSimDriver::setOutputPortState(const UUID &uuid,
                                              int pinIdx,
                                              const PortState &state) {
        const auto comp = getComponent<DigSimComp>(uuid);
        if (!comp) {
            BESS_WARN("[setOutputPortState] Component with UUID {} is invalid",
                      (uint64_t)uuid);
            return false;
        }

        if (!state.isDigital()) {
            BESS_WARN("[setOutputPortState] DigitalSimDriver only accepts "
                      "digital port states");
            return false;
        }

        auto &outputs = comp->getOutputStates();

        if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= outputs.size()) {
            BESS_WARN("[setOutputPortState] Output port index {} out of range "
                      "for component {}",
                      pinIdx,
                      (uint64_t)uuid);
            return false;
        }

        outputs[pinIdx] = state;
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

        if (m_opInfo.op != '0') {
            JsonConvert::toJsonValue(m_opInfo, json["opInfo"]);
        }

        if (!m_outputExpressions.empty()) {
            JsonConvert::toJsonValue(m_outputExpressions, json["expressions"]);
        }

        return json;
    }

    bool DigCompDef::onSlotsResizeReq(SlotsGroupType groupType,
                                      size_t newSize) {
        if (groupType == SlotsGroupType::input)
            return m_inputSlotsInfo.isResizeable;
        return m_outputSlotsInfo.isResizeable;
    }

    void DigCompDef::onStateChange(const ComponentState &oldState,
                                   const ComponentState &newState) {
    }

    void DigCompDef::onExpressionsChange() {
    }

    std::shared_ptr<CompDef> DigCompDef::clone() const {
        return std::make_shared<DigCompDef>(*this);
    }

    PortDescriptor DigCompDef::getInputPortDescriptor() const {
        return portDescriptorFor(m_inputSlotsInfo, PortDirection::input);
    }

    PortDescriptor DigCompDef::getOutputPortDescriptor() const {
        return portDescriptorFor(m_outputSlotsInfo, PortDirection::output);
    }

    bool DigCompDef::computeExpressionsIfNeeded() {
        // operator '0' means no operation
        // if no operation is defined, no expressions to compute
        if (m_opInfo.op == '0') {
            return false;
        }

        if (m_inputSlotsInfo.count <= 0) {
            BESS_WARN("[SimulationEngine][ComponentDefinition] Input count not "
                      "provided for expression(s) generation");
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
                m_outputExpressions.emplace_back(
                    std::format("{}{}", m_opInfo.op, i));
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
                BESS_WARN("(DigCompDef.setSimFn) Invalid data type passed to "
                          "sim function");
                return nullptr;
            }
            return simFn(digData);
        };
    }

    std::string DigCompDef::getTypeName() const {
        return TypeName;
    }

    std::shared_ptr<DigSimComp>
    DigSimComp::fromDef(const std::shared_ptr<CompDef> &compDef,
                        bool cloneDef) {
        return fromDef<DigSimComp>(compDef, cloneDef);
    }

    void DigModuleSimComp::onPostSimulate() {
        auto def = getDefinition<ModuleDefinition>();

        BESS_ASSERT(def, "Failed to get ModuleDefinition for DigModuleSimComp");

        const auto &inpId = def->getInputId();

        auto *simEngine = getDefinition<ModuleDefinition>()->getEngine();
        BESS_ASSERT(simEngine, "Simulation engine is not set");
        if (!simEngine) {
            return;
        }
        auto driver = simEngine->getDriverWithName(DigitalSimDriver::NAME);
        BESS_ASSERT(driver, "DigitalSimDriver not found in simulation engine");
        auto digitalDriver =
            std::dynamic_pointer_cast<DigitalSimDriver>(driver);
        BESS_ASSERT(digitalDriver, "Failed to cast to DigitalSimDriver");

        const auto &inpSimComp = driver->getComponent<DigSimComp>(inpId);
        if (!inpSimComp) {
            BESS_WARN("(DigModuleSimComp.onPostSimulate) Input component with "
                      "UUID {} not found for module with UUID {}",
                      (uint64_t)inpId,
                      (uint64_t)getUuid());
            BESS_ASSERT(false, "Input component not found for module");
            return;
        }

        auto &outStates = inpSimComp->getOutputStates();
        const auto &currStates = getInputStates();

        bool inpChanged = false;

        BESS_ASSERT(outStates.size() == currStates.size(),
                    "For assc Inp, output states size does not match current "
                    "input states of module");

        for (size_t i = 0; i < outStates.size(); ++i) {
            if (i < currStates.size()) {
                inpChanged |= (outStates[i].getLogicState() !=
                               currStates[i].getLogicState());
                outStates[i] = currStates[i];
            }
        }

        if (inpChanged) {
            digitalDriver->scheduleEvt(
                inpId, digitalDriver->getCurrentSimTime(), getUuid(), true);
        }
    }

    Json::Value DigSimComp::toJson() const {
        Json::Value json = EvtBasedSimComp::toJson();
        JsonConvert::toJsonValue(m_inputStates, json["inputStates"]);
        JsonConvert::toJsonValue(m_outputStates, json["outputStates"]);
        JsonConvert::toJsonValue(m_inputConnections, json["inputConnections"]);
        JsonConvert::toJsonValue(m_outputConnections,
                                 json["outputConnections"]);
        JsonConvert::toJsonValue(m_isInputConnected, json["isInputConnected"]);
        JsonConvert::toJsonValue(m_isOutputConnected,
                                 json["isOutputConnected"]);
        JsonConvert::toJsonValue(m_netUuid, json["netUuid"]);
        return json;
    }

    void DigSimComp::loadJson(const Json::Value &json) {
        EvtBasedSimComp::loadJson(json);

        if (json.isMember("inputStates")) {
            JsonConvert::fromJsonValue(json["inputStates"], m_inputStates);
        }

        if (json.isMember("outputStates")) {
            JsonConvert::fromJsonValue(json["outputStates"], m_outputStates);
        }

        if (json.isMember("inputConnections")) {
            JsonConvert::fromJsonValue(json["inputConnections"],
                                       m_inputConnections);
        }

        if (json.isMember("outputConnections")) {
            JsonConvert::fromJsonValue(json["outputConnections"],
                                       m_outputConnections);
        }

        if (json.isMember("isInputConnected")) {
            JsonConvert::fromJsonValue(json["isInputConnected"],
                                       m_isInputConnected);
        }

        if (json.isMember("isOutputConnected")) {
            JsonConvert::fromJsonValue(json["isOutputConnected"],
                                       m_isOutputConnected);
        }

        if (json.isMember("netUuid")) {
            JsonConvert::fromJsonValue(json["netUuid"], m_netUuid);
        }
    }

    Json::Value DigitalSimDriver::toJson() const {
        Json::Value json = Json::Value(Json::objectValue);
        for (const auto &[id, comp] : m_components) {
            json["components"].append(comp->toJson());
        }
        return json;
    }

    void DigitalSimDriver::loadJson(const Json::Value &json) {
        clearPendingEvents();
        clearComponents();

        if (!json.isObject() || !json.isMember("components") ||
            !json["components"].isArray()) {
            m_isNetUpdated = false;
            return;
        }

        {
            std::lock_guard lk(m_compMapMutex);
            for (const auto &compJson : json["components"]) {
                if (!compJson.isObject() || !compJson.isMember("def")) {
                    continue;
                }

                const auto def = loadDef(compJson["def"]);

                const auto comp = std::dynamic_pointer_cast<DigSimComp>(
                    createComp(def, false));
                if (!comp) {
                    continue;
                }

                comp->loadJson(compJson);
                m_components[comp->getUuid()] = comp;
            }
        }

        std::vector<UUID> reSchedComps;

        m_nets.clear();
        for (const auto &[compId, compBase] : m_components) {
            const auto comp = std::dynamic_pointer_cast<DigSimComp>(compBase);
            if (!comp) {
                continue;
            }

            if (comp->getDefinition<DigCompDef>()->getAutoReschedule()) {
                reSchedComps.emplace_back(compId);
            }

            const auto &netId = comp->getNetUuid();
            if (netId == UUID::null) {
                continue;
            }

            auto &net = m_nets[netId];
            net.setUUID(netId);
            net.addComponent(compId);
        }

        for (int i = 0; i < reSchedComps.size(); i++) {
            const auto &compId = reSchedComps.at(i);

            // notify after scheduling last comp
            scheduleEvt(compId,
                        m_currentSimTime,
                        UUID::null,
                        i == reSchedComps.size() - 1);
        }

        m_isNetUpdated = false;
    }

    void DigitalSimDriver::onInit() {
        auto &catalog = ComponentCatalog::instance();

        typedef std::shared_ptr<Drivers::Digital::DigCompSimData> TSimFnData;

        const auto inpDef = std::make_shared<Drivers::Digital::DigCompDef>();
        inpDef->setName("Digital Input");
        inpDef->setGroupName("IO");
        inpDef->setBehaviorType(ComponentBehaviorType::input);
        inpDef->setOutputSlotsInfo({SlotsGroupType::output, true, 1, {}, {}});
        inpDef->setSimFn([](const TSimFnData &state) -> TSimFnData {
            state->simDependants = true;
            return state;
        });
        inpDef->setPropDelay(TimeNs(0));
        catalog.registerComponent(inpDef);

        const auto outDef = std::make_shared<Drivers::Digital::DigCompDef>();
        outDef->setName("Digital Output");
        outDef->setGroupName("IO");
        outDef->setBehaviorType(ComponentBehaviorType::output);
        outDef->setInputSlotsInfo(
            {SlotsGroupType::input, true, 1, {"LSB"}, {}});
        outDef->setSimFn([](const TSimFnData &state) -> TSimFnData {
            state->simDependants = true;
            return state;
        });
        outDef->setPropDelay(TimeNs(0));
        catalog.registerComponent(outDef);
    }

    void DigCompDef::loadJson(const Json::Value &json) {
        EvtBasedCompDef::loadJson(json);

        if (json.isMember("inpSlotsInfo")) {
            JsonConvert::fromJsonValue(json["inpSlotsInfo"], m_inputSlotsInfo);
        }

        if (json.isMember("outSlotsInfo")) {
            JsonConvert::fromJsonValue(json["outSlotsInfo"], m_outputSlotsInfo);
        }

        if (json.isMember("opInfo")) {
            JsonConvert::fromJsonValue(json["opInfo"], m_opInfo);
        }

        if (json.isMember("behaviorType")) {
            JsonConvert::fromJsonValue(json["behaviorType"], m_behaviorType);
        }

        if (json.isMember("expressions")) {
            JsonConvert::fromJsonValue(json["expressions"],
                                       m_outputExpressions);
        }
    }
} // namespace Bess::SimEngine::Drivers::Digital

namespace Bess::JsonConvert {
    void
    toJsonValue(Json::Value &json,
                const Bess::SimEngine::Drivers::Digital::DigSimComp &data) {
        json = data.toJson();
    }
} // namespace Bess::JsonConvert
