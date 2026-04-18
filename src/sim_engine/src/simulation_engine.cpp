#include "simulation_engine.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "component_catalog.h"
#include "component_definition.h"
#include "digital_component.h"
#include "event_dispatcher.h"
#include "events/sim_engine_events.h"
#include "init_components.h"
#include "types.h"

#include "plugin_manager.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <numeric>
#include <ranges>
#include <thread>
#include <tuple>
#include <utility>

// #define BESS_ENABLE_LOG_EVENTS

#ifdef BESS_ENABLE_LOG_EVENTS
    #define BESS_LOG_EVENT(...) BESS_TRACE(__VA_ARGS__);
#else
    #define BESS_LOG_EVENT(...)
#endif // !BESS_LOG_EVENT

namespace Bess::SimEngine {
    SimulationEngine &SimulationEngine::instance() {
        static SimulationEngine inst;
        return inst;
    }

    SimulationEngine::SimulationEngine() {
        initComponentCatalog();
        const auto &pluginMangaer = Plugins::PluginManager::getInstance();

        auto &catalog = ComponentCatalog::instance();
        for (const auto &plugin : pluginMangaer.getLoadedPlugins()) {
            const auto comps = plugin.second->onComponentsRegLoad();
            for (const auto &comp : comps) {
                catalog.registerComponent(comp);
            }
            BESS_INFO("Registered {} components from plugin {}",
                      comps.size(),
                      plugin.first);
        }
        Plugins::savePyThreadState();
        m_simThread = std::thread(&SimulationEngine::run, this);
    }

    void SimulationEngine::clear() {
        std::unique_lock lkRegistry(m_registryMutex);
        std::lock_guard lkEventQueue(m_queueMutex);
        m_eventSet.clear();

        m_simEngineState.reset();
        m_analogCircuit.clear();
        m_lastAnalogSolution = {};
        m_analogConnections.clear();
        m_analogComponentDefinitions.clear();
        m_nextEventId = 0;
        m_currentSimTime = {};
    }

    SimulationEngine::~SimulationEngine() {
        destroy();
    }

    void SimulationEngine::destroy() {
        if (m_destroyed)
            return;
        m_stopFlag.store(true);
        m_queueCV.notify_all();
        m_stateCV.notify_all();
        if (m_simThread.joinable())
            m_simThread.join();

        Plugins::restorePyThreadState();
        ComponentCatalog::instance().destroy();

        m_destroyed = true;
        m_simEngineState.reset();
    }

    void SimulationEngine::scheduleEvent(UUID id, UUID schedulerId, SimDelayNanoSeconds simTime) {
        std::lock_guard lk(m_queueMutex);
        static uint64_t eventId = 0;
        SimulationEvent ev{simTime, id, schedulerId, eventId++};
        m_eventSet.insert(ev);
        m_queueCV.notify_all();
    }

    void SimulationEngine::clearEventsForEntity(const UUID &id) {
        std::lock_guard lk(m_queueMutex);
        std::erase_if(m_eventSet, [id](auto it) {
            return it.compId == id;
        });
    }

    const UUID &SimulationEngine::addComponent(const std::shared_ptr<ComponentDefinition> &definition,
                                               bool cloneDef) {
        auto digiComp = std::make_shared<DigitalComponent>(definition, cloneDef);

        {
            std::lock_guard lk(m_registryMutex);
            m_simEngineState.addDigitalComponent(digiComp);

            // create a new net for new component
            Net newNet{};
            digiComp->netUuid = newNet.getUUID();
            newNet.addComponent(digiComp->id);
            m_nets[digiComp->netUuid] = newNet;
            m_isNetUpdated = true;
        }

        scheduleEvent(digiComp->id, UUID::null, m_currentSimTime + definition->getSimDelay());

        EventSystem::EventDispatcher::instance().queue<Events::ComponentAddedEvent>({digiComp->id});
        BESS_INFO("Added component {} with id {} | base hash {}",
                  definition->getName(),
                  (uint64_t)digiComp->id,
                  definition->getBaseHash());

        return digiComp->id;
    }

    std::pair<bool, std::string> SimulationEngine::canConnectComponents(const UUID &src, int srcSlot, SlotType srcType,
                                                                        const UUID &dst, int dstSlot, SlotType dstType) const {
        std::lock_guard lk(m_registryMutex);
        return canConnectComponentsLocked(src, srcSlot, srcType, dst, dstSlot, dstType);
    }

    std::pair<bool, std::string> SimulationEngine::canConnectComponentsLocked(const UUID &src, int srcSlot, SlotType srcType,
                                                                              const UUID &dst, int dstSlot, SlotType dstType) const {
        if (srcType == SlotType::analogTerminal || dstType == SlotType::analogTerminal) {
            return {false, "Analog terminals must be connected via canConnectSlots"};
        }

        if (src == UUID::null || dst == UUID::null) {
            return {false, "Cannot connect to/from null component"};
        }

        if (!m_simEngineState.isComponentValid(src) || !m_simEngineState.isComponentValid(dst)) {
            return {false, "Source or destination component does not exist"};
        }

        if (srcType == dstType) {
            return {false, "Cannot connect pins of the same type i.e. input -> input or output -> output"};
        }

        const auto srcComp = m_simEngineState.getDigitalComponent(src);
        const auto dstComp = m_simEngineState.getDigitalComponent(dst);

        auto &outPins = srcType == SlotType::digitalOutput
                            ? srcComp->outputConnections
                            : srcComp->inputConnections;
        auto &inPins = dstType == SlotType::digitalInput
                           ? dstComp->inputConnections
                           : dstComp->outputConnections;

        if (srcSlot < 0 || srcSlot >= static_cast<int>(outPins.size())) {
            return {false, "Invalid source pin index. Valid range: 0 to " + std::to_string(outPins.size() - 1)};
        }
        if (dstSlot < 0 || dstSlot >= static_cast<int>(inPins.size())) {
            return {false, "Invalid destination pin index. Valid range: 0 to " + std::to_string(inPins.size() - 1)};
        }

        // Check for duplicate connection.
        auto &conns = outPins[srcSlot];
        bool exists = std::ranges::any_of(conns, [&](const auto &conn) {
            return conn.first == dst && conn.second == dstSlot;
        });

        return {!exists, exists ? "Connection already exists" : ""};
    }

    std::pair<bool, std::string> SimulationEngine::canConnectSlots(const SlotEndpoint &a,
                                                                    const SlotEndpoint &b) const {
        std::lock_guard lk(m_registryMutex);
        return canConnectSlotsLocked(a, b);
    }

    std::pair<bool, std::string> SimulationEngine::canConnectSlotsLocked(const SlotEndpoint &a,
                                                                          const SlotEndpoint &b) const {
        if (a.componentId == UUID::null || b.componentId == UUID::null) {
            return {false, "Cannot connect to/from null component"};
        }

        if (a.slotIdx < 0 || b.slotIdx < 0) {
            return {false, "Slot index cannot be negative"};
        }

        const bool aAnalog = a.slotType == SlotType::analogTerminal;
        const bool bAnalog = b.slotType == SlotType::analogTerminal;

        if (aAnalog != bAnalog) {
            return {false, "Cannot connect analog terminals to digital pins"};
        }

        if (aAnalog) {
            std::string error;
            if (!validateAnalogTerminalLocked(a.componentId, a.slotIdx, error)) {
                return {false, error};
            }
            if (!validateAnalogTerminalLocked(b.componentId, b.slotIdx, error)) {
                return {false, error};
            }

            if (a.componentId == b.componentId && a.slotIdx == b.slotIdx) {
                return {false, "Cannot connect a terminal to itself"};
            }

            const auto connection = normalizeAnalogConnection({a.componentId, static_cast<size_t>(a.slotIdx)},
                                                              {b.componentId, static_cast<size_t>(b.slotIdx)});
            if (hasAnalogConnectionLocked(connection)) {
                return {false, "Connection already exists"};
            }

            return {true, ""};
        }

        SlotEndpoint src = a;
        SlotEndpoint dst = b;

        if (src.slotType == SlotType::digitalInput && dst.slotType == SlotType::digitalOutput) {
            std::swap(src, dst);
        }

        return canConnectComponentsLocked(src.componentId,
                                          src.slotIdx,
                                          src.slotType,
                                          dst.componentId,
                                          dst.slotIdx,
                                          dst.slotType);
    }

    bool SimulationEngine::connectSlots(const SlotEndpoint &a, const SlotEndpoint &b) {
        const bool aAnalog = a.slotType == SlotType::analogTerminal;
        const bool bAnalog = b.slotType == SlotType::analogTerminal;

        if (aAnalog || bAnalog) {
            std::lock_guard lk(m_registryMutex);
            const auto [canConnect, errorMsg] = canConnectSlotsLocked(a, b);
            if (!canConnect) {
                BESS_WARN("Cannot connect slots: {}", errorMsg);
                return false;
            }

            const auto connection = normalizeAnalogConnection({a.componentId, static_cast<size_t>(a.slotIdx)},
                                                              {b.componentId, static_cast<size_t>(b.slotIdx)});
            m_analogConnections.push_back(connection);
            rebuildAnalogConnectionsLocked();
            return true;
        }

        SlotEndpoint src = a;
        SlotEndpoint dst = b;
        if (src.slotType == SlotType::digitalInput && dst.slotType == SlotType::digitalOutput) {
            std::swap(src, dst);
        }

        return connectComponent(src.componentId,
                                src.slotIdx,
                                src.slotType,
                                dst.componentId,
                                dst.slotIdx,
                                dst.slotType);
    }

    bool SimulationEngine::disconnectSlots(const SlotEndpoint &a, const SlotEndpoint &b) {
        const bool aAnalog = a.slotType == SlotType::analogTerminal;
        const bool bAnalog = b.slotType == SlotType::analogTerminal;

        if (aAnalog != bAnalog) {
            return false;
        }

        if (aAnalog) {
            std::lock_guard lk(m_registryMutex);
            if (a.slotIdx < 0 || b.slotIdx < 0) {
                return false;
            }

            const auto connection = normalizeAnalogConnection({a.componentId, static_cast<size_t>(a.slotIdx)},
                                                              {b.componentId, static_cast<size_t>(b.slotIdx)});
            const bool removed = removeAnalogConnectionLocked(connection);
            rebuildAnalogConnectionsLocked();
            return removed;
        }

        if (a.slotType == b.slotType) {
            return false;
        }

        SlotEndpoint src = a;
        SlotEndpoint dst = b;
        if (src.slotType == SlotType::digitalInput && dst.slotType == SlotType::digitalOutput) {
            std::swap(src, dst);
        }

        deleteConnection(src.componentId,
                         src.slotType,
                         src.slotIdx,
                         dst.componentId,
                         dst.slotType,
                         dst.slotIdx);
        return true;
    }

    bool SimulationEngine::connectComponent(const UUID &src, int srcSlot, SlotType srcType,
                                            const UUID &dst, int dstSlot, SlotType dstType, bool overrideConn) {

        std::lock_guard lk(m_registryMutex);
        auto [canConnect, errorMsg] = canConnectComponentsLocked(src, srcSlot, srcType, dst, dstSlot, dstType);
        if (!canConnect) {
            BESS_WARN("Cannot connect components: {}", errorMsg);
            return false;
        }

        const auto srcComp = m_simEngineState.getDigitalComponent(src);
        const auto dstComp = m_simEngineState.getDigitalComponent(dst);

        auto &outPins = srcType == SlotType::digitalOutput
                            ? srcComp->outputConnections
                            : srcComp->inputConnections;
        auto &inPins = dstType == SlotType::digitalInput
                           ? dstComp->inputConnections
                           : dstComp->outputConnections;

        // FIXME: State monitor attachment logic
        // {
        //     StateMonitorComponent *stateMonitorComp = nullptr;
        //     PinType type = srcType;
        //     ComponentPin pin;
        //     if (m_registry.try_get<StateMonitorComponent>(srcEnt)) {
        //         type = srcType;
        //         pin = {dst, dstPin};
        //     } else if (m_registry.try_get<StateMonitorComponent>(dstEnt)) {
        //         type = dstType;
        //         pin = {src, srcPin};
        //     }
        //
        //     if (stateMonitorComp) {
        //         stateMonitorComp->attacthTo(pin, type);
        //         BESS_INFO("Attached state monitor");
        //     }
        // }

        outPins[srcSlot].emplace_back(dst, dstSlot);
        inPins[dstSlot].emplace_back(src, srcSlot);

        if (srcType == SlotType::digitalOutput) {
            srcComp->state.outputConnected[srcSlot] = true;
            dstComp->state.inputConnected[dstSlot] = true;
        } else {
            srcComp->state.inputConnected[srcSlot] = true;
            dstComp->state.outputConnected[dstSlot] = true;
        }

        // Mix two nets
        // Final net will have id of net with most components initially
        if (srcComp->netUuid != dstComp->netUuid) {
            UUID finalNetId = UUID::null;
            UUID changedNetId = UUID::null;
            auto &srcNet = m_nets[srcComp->netUuid];
            auto &dstNet = m_nets[dstComp->netUuid];

            if (srcNet.size() > dstNet.size()) {
                finalNetId = srcComp->netUuid;
                changedNetId = dstComp->netUuid;
            } else {
                finalNetId = dstComp->netUuid;
                changedNetId = srcComp->netUuid;
            }

            auto &finalNet = m_nets[finalNetId];
            auto &changedNet = m_nets[changedNetId];

            for (const auto &compUuid : changedNet.getComponents()) {
                if (compUuid != UUID::null) {
                    const auto &comp = m_simEngineState.getDigitalComponent(compUuid);
                    comp->netUuid = finalNetId;
                }
                finalNet.addComponent(compUuid);
            }
            m_nets.erase(changedNetId);
            m_isNetUpdated = true;
        }

        scheduleEvent(dst, src, m_currentSimTime + dstComp->definition->getSimDelay());
        BESS_INFO("Connected components");
        return true;
    }

    void SimulationEngine::deleteComponent(const UUID &uuid) {
        std::set<UUID> affected;
        std::lock_guard lk(m_registryMutex);
        if (uuid == UUID::null || !m_simEngineState.isComponentValid(uuid))
            return;

        clearEventsForEntity(uuid);

        // Remove all connections to and from this component
        {
            const auto &digiComp = m_simEngineState.getDigitalComponent(uuid);

            for (const auto &pin : digiComp->outputConnections) {
                for (const auto &otherPin : pin) {
                    const auto otherPinIdx = otherPin.second;

                    auto comp = m_simEngineState.getDigitalComponent(otherPin.first);
                    auto &connections = comp->inputConnections[otherPinIdx];

                    auto removeIt = std::ranges::remove_if(connections, [&](auto &c) { return c.first == uuid; });
                    connections.erase(removeIt.begin(), removeIt.end());

                    comp->state.inputConnected[otherPinIdx] = !connections.empty();
                    affected.insert(otherPin.first);
                }
            }

            for (const auto &pin : digiComp->inputConnections) {
                for (const auto &otherPin : pin) {
                    const auto otherPinIdx = otherPin.second;

                    auto comp = m_simEngineState.getDigitalComponent(otherPin.first);
                    auto &connections = comp->outputConnections[otherPinIdx];

                    auto removeIt = std::ranges::remove_if(connections, [&](auto &c) { return c.first == uuid; });
                    connections.erase(removeIt.begin(), removeIt.end());

                    comp->state.outputConnected[otherPinIdx] = !connections.empty();
                    affected.insert(otherPin.first);
                }
            }
        }

        // update the nets
        {
            auto comp = m_simEngineState.getDigitalComponent(uuid);
            auto &ogNet = m_nets[comp->netUuid];
            ogNet.removeComponent(uuid);
            if (ogNet.size() == 0) {
                m_nets.erase(comp->netUuid);
            }

            updateNets(std::vector<UUID>(affected.begin(), affected.end()));
        }

        m_simEngineState.removeDigitalComponent(uuid);

        for (const auto e : affected) {
            if (!m_simEngineState.isComponentValid(e))
                continue;

            const auto &dc = m_simEngineState.getDigitalComponent(e);
            scheduleEvent(e, UUID::null, m_currentSimTime + dc->definition->getSimDelay());
        }

        BESS_INFO("Deleted component {}", (uint64_t)uuid);
    }

    bool SimulationEngine::updateNets(const std::vector<UUID> &startCompIds) {
        std::unordered_map<UUID, std::vector<UUID>> connGraphs;
        std::unordered_map<UUID, bool> visited;

        for (const UUID &e : startCompIds) {
            if (visited[e])
                continue;
            connGraphs[e] = getConnGraph(e);

            for (const auto &entInGraph : connGraphs[e]) {
                visited[entInGraph] = true;
            }
        }

        if (connGraphs.size() <= 1) {
            return false;
        }

        for (const auto &graphPair : connGraphs) {
            Net newNet{};
            for (const auto &entInGraph : graphPair.second) {
                const auto &compInGraph = m_simEngineState.getDigitalComponent(entInGraph);

                if (m_nets.contains(compInGraph->netUuid)) {
                    auto &prevNet = m_nets[compInGraph->netUuid];
                    prevNet.removeComponent(entInGraph);
                    if (prevNet.size() == 0) {
                        m_nets.erase(prevNet.getUUID());
                    }
                }

                compInGraph->netUuid = newNet.getUUID();
                newNet.addComponent(entInGraph);
            }
            m_nets[newNet.getUUID()] = newNet;
        }

        m_isNetUpdated = connGraphs.size() > 1;

        return true;
    }

    SlotState SimulationEngine::getDigitalSlotState(const UUID &uuid, SlotType type, int idx) const {
        if (type == SlotType::analogTerminal) {
            BESS_WARN("[getDigitalPinState] Analog slot type passed to digital state lookup");
            return {LogicState::unknown, SimTime(0)};
        }

        if (!m_simEngineState.isComponentValid(uuid)) {
            BESS_WARN("[getDigitalPinState] Component with UUID {} is invalid", (uint64_t)uuid);
            return {LogicState::unknown, SimTime(0)};
        }
        const auto &comp = m_simEngineState.getDigitalComponent(uuid);

        if (idx < 0) {
            BESS_WARN("[getDigitalPinState] Negative slot index {} for component {}", idx, (uint64_t)uuid);
            return {LogicState::unknown, SimTime(0)};
        }

        if (type == SlotType::digitalOutput) {
            if (static_cast<size_t>(idx) >= comp->state.outputStates.size()) {
                BESS_WARN("[getDigitalPinState] Output slot index {} out of range for component {}", idx, (uint64_t)uuid);
                return {LogicState::unknown, SimTime(0)};
            }
            return comp->state.outputStates[idx];
        }

        if (static_cast<size_t>(idx) >= comp->state.inputStates.size()) {
            BESS_WARN("[getDigitalPinState] Input slot index {} out of range for component {}", idx, (uint64_t)uuid);
            return {LogicState::unknown, SimTime(0)};
        }
        return comp->state.inputStates[idx];
    }

    SlotState SimulationEngine::getSlotState(const UUID &uuid, SlotType type, int idx) const {
        if (type == SlotType::analogTerminal) {
            constexpr double kAnalogCurrentFlowThresholdAmps = 1e-12;

            std::lock_guard lk(m_registryMutex);
            std::string error;
            if (!validateAnalogTerminalLocked(uuid, idx, error)) {
                return {LogicState::unknown, SimTime(0)};
            }

            const auto analogState = m_analogCircuit.getComponentState(uuid);
            if (analogState.simError || static_cast<size_t>(idx) >= analogState.terminals.size()) {
                return {LogicState::unknown, SimTime(0)};
            }

            const auto &terminalState = analogState.terminals[idx];
            if (!terminalState.connected) {
                return {LogicState::low, SimTime(0)};
            }

            if (!std::isfinite(terminalState.current)) {
                return {LogicState::unknown, SimTime(0)};
            }

            return {std::abs(terminalState.current) > kAnalogCurrentFlowThresholdAmps
                        ? LogicState::high
                        : LogicState::low,
                    SimTime(0)};
        }

        return getDigitalSlotState(uuid, type, idx);
    }

    bool SimulationEngine::isSlotConnected(const UUID &uuid, SlotType type, int idx) const {
        if (idx < 0) {
            return false;
        }

        if (type == SlotType::analogTerminal) {
            std::lock_guard lk(m_registryMutex);
            std::string error;
            if (!validateAnalogTerminalLocked(uuid, idx, error)) {
                return false;
            }

            const auto analogState = m_analogCircuit.getComponentState(uuid);
            return static_cast<size_t>(idx) < analogState.terminals.size() &&
                   analogState.terminals[idx].connected;
        }

        const auto digitalComp = m_simEngineState.getDigitalComponent(uuid);
        if (!digitalComp) {
            return false;
        }

        if (type == SlotType::digitalInput) {
            return static_cast<size_t>(idx) < digitalComp->state.inputConnected.size() &&
                   digitalComp->state.inputConnected[idx];
        }

        return static_cast<size_t>(idx) < digitalComp->state.outputConnected.size() &&
               digitalComp->state.outputConnected[idx];
    }

    bool SimulationEngine::isComponentValid(const UUID &uuid) const {
        std::lock_guard lk(m_registryMutex);
        return m_simEngineState.isComponentValid(uuid) ||
               (m_analogCircuit.getComponent(uuid) != nullptr);
    }

    ConnectionBundle SimulationEngine::getConnections(const UUID &uuid) {
        ConnectionBundle bundle;
        if (!m_simEngineState.isComponentValid(uuid)) {
            BESS_WARN("[getConnections] Component with UUID {} is invalid",
                      (uint64_t)uuid);
            return bundle;
        }
        const auto &comp = m_simEngineState.getDigitalComponent(uuid);

        bundle.inputs = comp->inputConnections;
        bundle.outputs = comp->outputConnections;

        return bundle;
    }

    void SimulationEngine::setInputSlotState(const UUID &uuid, int pinIdx, LogicState state) {
        const auto comp = m_simEngineState.getDigitalComponent(uuid);
        if (!comp) {
            BESS_WARN("[setInputSlotState] Component with UUID {} is invalid", (uint64_t)uuid);
            return;
        }

        if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= comp->state.inputStates.size()) {
            BESS_WARN("[setInputSlotState] Input slot index {} out of range for component {}",
                      pinIdx,
                      (uint64_t)uuid);
            return;
        }

        comp->state.inputStates[pinIdx].state = state;
        comp->state.inputStates[pinIdx].lastChangeTime = m_currentSimTime;
        scheduleEvent(uuid, UUID::null, m_currentSimTime + comp->definition->getSimDelay());
    }

    void SimulationEngine::setOutputSlotState(const UUID &uuid, int pinIdx, LogicState state) {
        const auto comp = m_simEngineState.getDigitalComponent(uuid);
        if (!comp) {
            BESS_WARN("[setOutputSlotState] Component with UUID {} is invalid", (uint64_t)uuid);
            return;
        }

        if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= comp->state.outputStates.size()) {
            BESS_WARN("[setOutputSlotState] Output slot index {} out of range for component {}",
                      pinIdx,
                      (uint64_t)uuid);
            return;
        }

        auto oldState = comp->state;
        comp->state.outputStates[pinIdx].state = state;
        comp->state.outputStates[pinIdx].lastChangeTime = m_currentSimTime;
        comp->dispatchStateChange(oldState, comp->state);
        scheduleDependantsOf(uuid);
    }

    void SimulationEngine::invertInputSlotState(const UUID &uuid, int pinIdx) {
        const auto comp = m_simEngineState.getDigitalComponent(uuid);
        if (!comp) {
            BESS_WARN("[invertInputSlotState] Component with UUID {} is invalid", (uint64_t)uuid);
            return;
        }

        if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= comp->state.inputStates.size()) {
            BESS_WARN("[invertInputSlotState] Input slot index {} out of range for component {}",
                      pinIdx,
                      (uint64_t)uuid);
            return;
        }

        const auto state = comp->state.inputStates[pinIdx].state == LogicState::high
                               ? LogicState::low
                               : LogicState::high;

        comp->state.inputStates[pinIdx].state = state;
        comp->state.inputStates[pinIdx].lastChangeTime = m_currentSimTime;
        scheduleEvent(uuid, UUID::null, m_currentSimTime + comp->definition->getSimDelay());
    }

    const ComponentState &SimulationEngine::getComponentState(const UUID &uuid) {
        const auto &comp = m_simEngineState.getDigitalComponent(uuid);
        return comp->state;
    }

    const std::shared_ptr<ComponentDefinition> &SimulationEngine::getComponentDefinition(const UUID &uuid) const {
        static const std::shared_ptr<ComponentDefinition> nullDefinition;

        const auto &comp = m_simEngineState.getDigitalComponent(uuid);
        if (comp) {
            return comp->definition;
        }

        if (auto it = m_analogComponentDefinitions.find(uuid); it != m_analogComponentDefinitions.end()) {
            return it->second;
        }

        return nullDefinition;
    }

    void SimulationEngine::deleteConnection(const UUID &compA, SlotType pinAType, int idxA,
                                            const UUID &compB, SlotType pinBType, int idxB) {

        if (pinAType == SlotType::analogTerminal || pinBType == SlotType::analogTerminal) {
            BESS_WARN("deleteConnection called with analog slot types; use disconnectSlots instead");
            return;
        }

        assert(m_simEngineState.isComponentValid(compA) ||
               m_simEngineState.isComponentValid(compB));
        std::lock_guard lk(m_registryMutex);
        const auto compARef = m_simEngineState.getDigitalComponent(compA);
        const auto compBRef = m_simEngineState.getDigitalComponent(compB);

        auto &pinsA = pinAType == SlotType::digitalInput
                          ? compARef->inputConnections
                          : compARef->outputConnections;
        auto &pinsB = pinBType == SlotType::digitalInput
                          ? compBRef->inputConnections
                          : compBRef->outputConnections;

        // Erase connection from A -> B
        pinsA[idxA].erase(
            std::remove_if(pinsA[idxA].begin(), pinsA[idxA].end(),
                           [&](auto &c) { return c.first == compB && c.second == idxB; }),
            pinsA[idxA].end());
        // Erase connection from B -> A
        pinsB[idxB].erase(
            std::remove_if(pinsB[idxB].begin(), pinsB[idxB].end(),
                           [&](auto &c) { return c.first == compA && c.second == idxA; }),
            pinsB[idxB].end());

        if (pinsA[idxA].empty()) {
            if (pinAType == SlotType::digitalOutput) {
                compARef->state.outputConnected[idxA] = false;
            } else {
                compARef->state.inputConnected[idxA] = false;
            }
        }

        if (pinsB[idxB].empty()) {
            if (pinBType == SlotType::digitalOutput) {
                compBRef->state.outputConnected[idxB] = false;
            } else {
                compBRef->state.inputConnected[idxB] = false;
            }
        }

        // Schedule next simulation on the appropriate side
        const UUID toSchedule = pinAType == SlotType::digitalOutput ? compB : compA;

        updateNets({compA, compB});

        const auto &dc = m_simEngineState.getDigitalComponent(toSchedule);
        scheduleEvent(toSchedule, UUID::null, m_currentSimTime + dc->definition->getSimDelay());

        BESS_INFO("Deleted connection");
    }

    std::vector<SlotState> SimulationEngine::getInputSlotsState(UUID compId) const {

        if (!m_simEngineState.isComponentValid(compId)) {
            BESS_WARN("Component with UUID {} is invalid", (uint64_t)compId);
            return {};
        }

        std::vector<SlotState> states;

        const auto &comp = m_simEngineState.getDigitalComponent(compId);
        for (const auto &pinConnections : comp->inputConnections) {
            SlotState aggregatedPinState = {LogicState::low, SimTime(0)};

            if (pinConnections.empty()) {
                states.push_back(aggregatedPinState);
                continue;
            }

            // Iterate over all the output pins connected to this single input pin
            for (const auto &conn : pinConnections) {
                if (conn.first == UUID::null)
                    continue;

                const auto &sourceComponent = m_simEngineState.getDigitalComponent(conn.first);
                const auto &sourcePin = sourceComponent->state.outputStates[conn.second];

                if (sourcePin.state == LogicState::high_z) {
                    continue;
                }

                if (sourcePin.state == LogicState::high) {
                    if (aggregatedPinState.state != LogicState::high) {
                        aggregatedPinState = sourcePin;
                    } else if (sourcePin.lastChangeTime > aggregatedPinState.lastChangeTime) {
                        aggregatedPinState.lastChangeTime = sourcePin.lastChangeTime;
                    }
                } else if (sourcePin.state == LogicState::unknown && aggregatedPinState.state == LogicState::low) {
                    aggregatedPinState = sourcePin;
                }
            }

            states.push_back(aggregatedPinState);
        }
        return states;
    }

    bool SimulationEngine::simulateComponent(const UUID &compId, const std::vector<SlotState> &inputs) {
        const auto &comp = m_simEngineState.getDigitalComponent(compId);
        BESS_ASSERT(comp,
                    std::format("Component {} is invalid", (uint64_t)compId));
        const auto &def = comp->definition;
        BESS_ASSERT(def,
                    std::format("Component definition of {} is invalid", (uint64_t)comp->id));
#ifdef BESS_ENABLE_LOG_EVENTS
        BESS_LOG_EVENT("Simulating {}, with delay {}ns", def->getName(), def->getSimDelay().count());
        BESS_LOG_EVENT("\tInputs:");
        for (auto &inp : inputs) {
            BESS_LOG_EVENT("\t\t{}", (int)inp.state);
        }
        BESS_LOG_EVENT("");
#endif // BESS_ENABLE_LOG_EVENTS

        auto &simFunction = def->getSimulationFunction();

        if (!simFunction) {
            BESS_ERROR("Component {} does not have a simulation function defined. Skipping simulation.", def->getName());
            assert(false && "Simulation function not defined for component");
            return false;
        }

        comp->state.simError = false;
        auto oldState = comp->state;
        ComponentState newState;
        try {
            newState = def->getSimulationFunction()(inputs, m_currentSimTime, comp->state);
        } catch (std::exception &ex) {
            BESS_ERROR("Exception during simulation of component {}. Output won't be updated: {}",
                       def->getName(), ex.what());
            comp->state.simError = true;
            comp->state.errorMessage = ex.what();
            comp->state.isChanged = false;
        }

        comp->state.inputStates = inputs;

        BESS_LOG_EVENT("\tState changed: {}", newState.isChanged ? "YES" : "NO");

        if (newState.isChanged && !comp->state.simError) {
            comp->state = newState;
            comp->definition->onStateChange(oldState, comp->state);
            comp->dispatchStateChange(oldState, newState);
            BESS_LOG_EVENT("\tOutputs changed to:");
            for (auto &outp : newState.outputStates) {
                BESS_LOG_EVENT("\t\t{}", (bool)outp.state);
            }
        }

        // FIXME: State monitor logic
        // if (auto *stateMonitor = m_registry.try_get<StateMonitorComponent>(e)) {
        //     stateMonitor->appendState(newState.inputStates[0].lastChangeTime,
        //                               newState.inputStates[0].state);
        // }
        //

        return newState.isChanged;
    }

    SimulationState SimulationEngine::getSimulationState() const {
        return m_simState.load();
    }

    void SimulationEngine::stepSimulation() {
        std::unique_lock stateLock(m_stateMutex);
        if (m_simState.load() != SimulationState::paused || m_stepFlag.load())
            return;
        m_stepFlag.store(true);
        m_stateCV.notify_all();
    }

    SimulationState SimulationEngine::toggleSimState() {
        if (m_simState == SimulationState::paused) {
            setSimulationState(SimulationState::running);
        } else if (m_simState == SimulationState::running) {
            setSimulationState(SimulationState::paused);
        }

        return m_simState.load();
    }

    void SimulationEngine::setSimulationState(SimulationState state) {
        std::unique_lock stateLock(m_stateMutex);
        m_simState.store(state);
        m_stateCV.notify_all();
    }

    std::chrono::milliseconds SimulationEngine::getSimulationTimeMS() {
        return std::chrono::time_point_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now())
            .time_since_epoch();
    }

    std::chrono::seconds SimulationEngine::getSimulationTimeS() {
        return std::chrono::time_point_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now())
            .time_since_epoch();
    }

    void SimulationEngine::run() {
        auto state = Plugins::capturePyThreadState();
        Plugins::releasePyThreadState(state);
        BESS_INFO("[SimulationEngine] Simulation loop started");
        m_currentSimTime = SimTime(0);

        while (!m_stopFlag.load()) {
            std::unique_lock queueLock(m_queueMutex);

            m_queueCV.wait(queueLock, [&] { return m_stopFlag.load() || !m_eventSet.empty(); });
            if (m_stopFlag.load())
                break;

            std::unique_lock stateLock(m_stateMutex);
            if (m_simState.load() == SimulationState::paused) {
                queueLock.unlock();
                m_stepFlag.store(false);
                m_stateCV.wait(stateLock, [&] { return m_stopFlag.load() ||
                                                       m_simState.load() == SimulationState::running ||
                                                       m_stepFlag.load(); });
                queueLock.lock();
            }

            if (m_stopFlag.load())
                break;

            if (m_eventSet.empty())
                continue;

            stateLock.unlock();
            std::unique_lock regLock(m_registryMutex, std::defer_lock);
            queueLock.unlock();
            std::lock(regLock, queueLock);

            if (m_eventSet.empty()) {
                continue;
            }

            auto deltaTime = m_eventSet.begin()->simTime - m_currentSimTime;
            m_currentSimTime = m_eventSet.begin()->simTime;

            std::set<SimulationEvent> eventsToSim = {};
            while (!m_eventSet.empty() && m_eventSet.begin()->simTime == m_currentSimTime) {
                auto ev = *m_eventSet.begin();
                if (!eventsToSim.empty() && eventsToSim.rbegin()->schedulerId != ev.schedulerId)
                    break;
                eventsToSim.insert(ev);
                m_eventSet.erase(m_eventSet.begin());
            }

            BESS_LOG_EVENT("");
            BESS_LOG_EVENT("[SimulationEngine][t = {}ns][dt = {}ns] Picked {} events to simulate",
                           m_currentSimTime.count(), deltaTime.count(), eventsToSim.size());

            m_isSimulating = true;
            queueLock.unlock();

            std::unordered_map<UUID, std::vector<SlotState>> inputsMap = {};

            for (auto &ev : eventsToSim) {
                inputsMap[ev.compId] = getInputSlotsState(ev.compId);
            }
            BESS_LOG_EVENT("[SimulationEngine] Selected {} unique entites to simulate", inputsMap.size());

            std::vector<std::tuple<UUID, UUID, SimTime>> eventsToSchedule;
            for (auto &[compId, inputs] : inputsMap) {
                const bool changed = simulateComponent(compId, inputs);
                const auto &dc = m_simEngineState.getDigitalComponent(compId);

                if (changed) {
                    for (auto &pin : dc->outputConnections) {
                        const auto &keyView = pin | std::views::keys;
                        const std::set<UUID> uniqueEntities = std::set<UUID>(keyView.begin(), keyView.end());
                        for (auto &ent : uniqueEntities) {
                            const auto dependant = m_simEngineState.getDigitalComponent(ent);
                            if (!dependant || !dependant->definition) {
                                continue;
                            }
                            eventsToSchedule.emplace_back(ent,
                                                          compId,
                                                          m_currentSimTime + dependant->definition->getSimDelay());
                        }
                    }
                }

                if (dc->definition->getShouldAutoReschedule()) {
                    eventsToSchedule.emplace_back(compId,
                                                  UUID::null,
                                                  dc->definition->getRescheduleTime(m_currentSimTime));
                }
            }

            for (const auto &[id, schedulerId, simTime] : eventsToSchedule) {
                scheduleEvent(id, schedulerId, simTime);
            }

            regLock.unlock();
            queueLock.lock();
            m_isSimulating = false;
            m_queueCV.notify_all();

            BESS_LOG_EVENT("[BessSimEngine] Sim Cycle End");
            BESS_LOG_EVENT("");

            if (!m_eventSet.empty()) {
                m_queueCV.wait_until(queueLock, std::chrono::steady_clock::now() +
                                                    (m_eventSet.begin()->simTime - m_currentSimTime));
            } else {
                BESS_DEBUG("[BessSimEngine] Event queue empty, waiting for new events");
                m_queueCV.notify_all();
            }
        }
    }

    bool SimulationEngine::updateInputCount(const UUID &uuid, int n) {

        throw std::runtime_error("updateInputCount is not implemented yet");

        if (!m_simEngineState.isComponentValid(uuid))
            return false;

        const auto digiComp = m_simEngineState.getDigitalComponent(uuid);
        // digiComp->updateInputCount(n);
        return true;
    }

    std::vector<std::pair<float, bool>> SimulationEngine::getStateMonitorData(UUID uuid) {
        if (!m_simEngineState.isComponentValid(uuid)) {
            BESS_WARN("[SimulationEngine] getStateMonitorData was called on entity with no StateMonitorComponent: {}", (uint64_t)uuid);
            return {};
        }

        // FIXME: State monitor is not implemented yet
        // auto comp = m_simEngineState.getDigitalComponent(uuid);
        // return comp->timestepedBoolData;
        return {};
    }

    std::vector<UUID> SimulationEngine::getConnGraph(UUID start) {
        std::vector<UUID> graph;
        std::set<UUID> visited;
        std::vector<UUID> toVisit;
        toVisit.push_back(start);

        while (!toVisit.empty()) {
            auto current = toVisit.back();
            toVisit.pop_back();

            if (visited.contains(current))
                continue;

            visited.insert(current);
            graph.push_back(current);

            const auto &comp = m_simEngineState.getDigitalComponent(current);

            for (const auto &pinConnections : comp->inputConnections) {
                for (const auto &conn : pinConnections) {
                    if (conn.first != UUID::null && !visited.contains(conn.first)) {
                        toVisit.push_back(conn.first);
                    }
                }
            }

            for (const auto &pinConnections : comp->outputConnections) {
                for (const auto &conn : pinConnections) {
                    if (conn.first != UUID::null && !visited.contains(conn.first)) {
                        toVisit.push_back(conn.first);
                    }
                }
            }
        }

        return graph;
    }

    bool SimulationEngine::isNetUpdated() const {
        return m_isNetUpdated;
    }

    const std::unordered_map<UUID, Net> &SimulationEngine::getNetsMap(bool update) {

        // using clean code rather than short on purpose ;)
        if (update)
            m_isNetUpdated = false;

        return m_nets;
    }

    TruthTable SimulationEngine::getTruthTableOfNet(const UUID &netUuid) {
        if (!m_nets.contains(netUuid))
            return {};

        BESS_INFO("\nGenerating truth table for net {}", (uint64_t)netUuid);
        const auto &net = m_nets.at(netUuid);

        TruthTable truthTable;

        std::vector<UUID> components;

        std::vector<std::pair<UUID, int>> inputs;
        std::vector<std::pair<UUID, int>> outputs;
        std::unordered_map<UUID, size_t> inputSlotCounts;

        for (const auto &compUuid : net.getComponents()) {
            if (compUuid == UUID::null)
                continue;

            const auto &comp = m_simEngineState.getDigitalComponent(compUuid);
            bool isInput = comp->definition->getBehaviorType() == ComponentBehaviorType::input;
            bool isOutput = comp->definition->getBehaviorType() == ComponentBehaviorType::output;

            if (isInput) {
                for (int i = 0; i < comp->definition->getOutputSlotsInfo().count; i++) {
                    inputs.emplace_back(compUuid, i);
                    truthTable.inputUuids.push_back(compUuid);
                }
            } else if (isOutput) {
                for (int i = 0; i < comp->definition->getInputSlotsInfo().count; i++) {
                    outputs.emplace_back(compUuid, i);
                    truthTable.outputUuids.push_back(compUuid);
                }
            } else {
                components.emplace_back(compUuid);
            }
        }

        if (inputs.size() == 0 || outputs.size() == 0) {
            BESS_WARN("Cannot generate truth table for net {} without input or output components",
                      (uint64_t)netUuid);
            return {};
        }

        BESS_INFO("Truth table will have {} inputs and {} outputs", inputs.size(), outputs.size());

        const size_t numInputs = inputs.size();
        const size_t numCombinations = 1 << numInputs;

        std::vector<std::vector<LogicState>> tableData(numCombinations,
                                                       std::vector<LogicState>(
                                                           numInputs + outputs.size(),
                                                           LogicState::low));

        BESS_INFO("Truth table dimensions: {} rows x {} columns",
                  tableData.size(),
                  tableData[0].size());

        BESS_INFO("Total combinations to simulate: {}", numCombinations);

        for (size_t comb = 0; comb < numCombinations; comb++) {
            BESS_INFO("Simulating combination {}/{}: ", comb + 1, numCombinations);
            size_t runningIdx = numInputs - 1, i = 0;
            for (const auto &[inpUuid, slotIdx] : inputs) {
                std::lock_guard lk(m_registryMutex);
                const LogicState state = (comb & (1 << i)) ? LogicState::high : LogicState::low;
                setOutputSlotState(inpUuid, slotIdx, state);
                tableData[comb][runningIdx] = state;
                runningIdx--;
                i++;
            }

            BESS_INFO("Waiting for simulation to stabilize...");

            // Wait for simulation to stabilize
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_queueCV.wait(lock, [&] {
                    return isSimStableLocked();
                });
            }

            BESS_INFO("Simulation stabilized. Recording outputs");

            // FIXME: Temporary sleep to ensure all events are processed
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            std::lock_guard lk(m_queueMutex);
            for (size_t i = 0; i < outputs.size(); i++) {
                const auto states = getInputSlotsState(outputs[i].first);
                tableData[comb][numInputs + i] = states[outputs[i].second].state;
                BESS_DEBUG("Output component {} slot {} state: {}",
                           (uint64_t)outputs[i].first,
                           outputs[i].second,
                           (int)tableData[comb][numInputs + i]);
            }
        }

        BESS_INFO("Truth table generation completed for net {}\n", (uint64_t)netUuid);
        std::ranges::reverse(truthTable.inputUuids);
        truthTable.table = tableData;

        return truthTable;
    }

    bool SimulationEngine::isSimStable() {
        std::lock_guard lk(m_queueMutex);
        return isSimStableLocked();
    }

    bool SimulationEngine::isSimStableLocked() const {
        if (m_isSimulating)
            return false;

        if (m_eventSet.empty()) {
            return true;
        }

        return true;
    }

    std::shared_ptr<DigitalComponent> SimulationEngine::getDigitalComponent(const UUID &uuid) const {
        return m_simEngineState.getDigitalComponent(uuid);
    }

    const SimEngineState &SimulationEngine::getSimEngineState() const {
        return m_simEngineState;
    }

    SimEngineState &SimulationEngine::getSimEngineState() {
        return m_simEngineState;
    }

    SimTime SimulationEngine::getSimulationTime() const {
        return m_currentSimTime;
    }

    AnalogCircuit &SimulationEngine::getAnalogCircuit() {
        return m_analogCircuit;
    }

    const AnalogCircuit &SimulationEngine::getAnalogCircuit() const {
        return m_analogCircuit;
    }

    const UUID &SimulationEngine::addAnalogComponent(std::shared_ptr<AnalogComponent> component,
                                                     const std::shared_ptr<ComponentDefinition> &definition) {
        std::lock_guard lk(m_registryMutex);
        const UUID &id = m_analogCircuit.addComponent(std::move(component));
        if (definition) {
            m_analogComponentDefinitions[id] = definition;
        }
        return id;
    }

    const UUID &SimulationEngine::addAnalogResistor(double resistanceOhms, const std::string &name) {
        std::lock_guard lk(m_registryMutex);
        return m_analogCircuit.addResistor(resistanceOhms, name);
    }

    const UUID &SimulationEngine::addAnalogResistor(AnalogNodeId a, AnalogNodeId b, double resistanceOhms,
                                                    const std::string &name) {
        std::lock_guard lk(m_registryMutex);
        return m_analogCircuit.addResistor(a, b, resistanceOhms, name);
    }

    bool SimulationEngine::connectAnalogTerminal(const UUID &componentId, size_t terminalIdx, AnalogNodeId node) {
        std::lock_guard lk(m_registryMutex);
        return m_analogCircuit.connectTerminal(componentId, terminalIdx, node);
    }

    bool SimulationEngine::disconnectAnalogTerminal(const UUID &componentId, size_t terminalIdx) {
        std::lock_guard lk(m_registryMutex);
        return m_analogCircuit.disconnectTerminal(componentId, terminalIdx);
    }

    bool SimulationEngine::removeAnalogComponent(const UUID &componentId) {
        std::lock_guard lk(m_registryMutex);
        removeAnalogConnectionsForComponentLocked(componentId);
        m_analogComponentDefinitions.erase(componentId);

        const bool removed = m_analogCircuit.removeComponent(componentId);
        if (removed) {
            rebuildAnalogConnectionsLocked();
        }
        return removed;
    }

    AnalogComponentState SimulationEngine::getAnalogComponentState(const UUID &componentId) const {
        std::lock_guard lk(m_registryMutex);
        return m_analogCircuit.getComponentState(componentId);
    }

    std::optional<double> SimulationEngine::getAnalogResistorValue(const UUID &componentId) const {
        std::lock_guard lk(m_registryMutex);
        const auto component = m_analogCircuit.getComponent(componentId);
        if (!component) {
            return std::nullopt;
        }

        const auto resistor = std::dynamic_pointer_cast<Resistor>(component);
        if (!resistor) {
            return std::nullopt;
        }

        return resistor->resistanceOhms();
    }

    AnalogSolution SimulationEngine::solveAnalogCircuit(const AnalogSolveOptions &options) {
        std::lock_guard lk(m_registryMutex);
        m_lastAnalogSolution = m_analogCircuit.solve(options);
        return m_lastAnalogSolution;
    }

    void SimulationEngine::clearAnalogCircuit() {
        std::lock_guard lk(m_registryMutex);
        m_analogCircuit.clear();
        m_lastAnalogSolution = {};
        m_analogConnections.clear();
        m_analogComponentDefinitions.clear();
    }

    bool SimulationEngine::validateAnalogTerminalLocked(const UUID &componentId,
                                                        int slotIdx,
                                                        std::string &error) const {
        const auto component = m_analogCircuit.getComponent(componentId);
        if (!component) {
            error = "Source or destination component does not exist";
            return false;
        }

        if (slotIdx < 0) {
            error = "Slot index cannot be negative";
            return false;
        }

        const auto terminals = component->terminals();
        if (static_cast<size_t>(slotIdx) >= terminals.size()) {
            error = "Invalid terminal index. Valid range: 0 to " +
                    std::to_string(terminals.empty() ? 0 : terminals.size() - 1);
            return false;
        }

        return true;
    }

    SimulationEngine::AnalogTerminalConnection
    SimulationEngine::normalizeAnalogConnection(const AnalogTerminalRef &a,
                                                const AnalogTerminalRef &b) {
        const auto aId = static_cast<uint64_t>(a.componentId);
        const auto bId = static_cast<uint64_t>(b.componentId);
        const bool bBeforeA = (bId < aId) || (bId == aId && b.terminalIdx < a.terminalIdx);
        if (bBeforeA) {
            return {b, a};
        }
        return {a, b};
    }

    bool SimulationEngine::hasAnalogConnectionLocked(const AnalogTerminalConnection &connection) const {
        return std::ranges::any_of(m_analogConnections, [&](const auto &existing) {
            return existing.a.componentId == connection.a.componentId &&
                   existing.a.terminalIdx == connection.a.terminalIdx &&
                   existing.b.componentId == connection.b.componentId &&
                   existing.b.terminalIdx == connection.b.terminalIdx;
        });
    }

    bool SimulationEngine::removeAnalogConnectionLocked(const AnalogTerminalConnection &connection) {
        const auto before = m_analogConnections.size();
        std::erase_if(m_analogConnections, [&](const auto &existing) {
            return existing.a.componentId == connection.a.componentId &&
                   existing.a.terminalIdx == connection.a.terminalIdx &&
                   existing.b.componentId == connection.b.componentId &&
                   existing.b.terminalIdx == connection.b.terminalIdx;
        });
        return m_analogConnections.size() != before;
    }

    void SimulationEngine::removeAnalogConnectionsForComponentLocked(const UUID &componentId) {
        std::erase_if(m_analogConnections, [&](const auto &connection) {
            return connection.a.componentId == componentId ||
                   connection.b.componentId == componentId;
        });
    }

    void SimulationEngine::rebuildAnalogConnectionsLocked() {
        struct DisjointSet {
            std::vector<size_t> parent;

            explicit DisjointSet(size_t n) : parent(n) {
                std::iota(parent.begin(), parent.end(), 0);
            }

            size_t find(size_t x) {
                if (parent[x] != x) {
                    parent[x] = find(parent[x]);
                }
                return parent[x];
            }

            void unite(size_t a, size_t b) {
                const size_t rootA = find(a);
                const size_t rootB = find(b);
                if (rootA != rootB) {
                    parent[rootB] = rootA;
                }
            }
        };

        std::vector<AnalogTerminalRef> terminals;
        terminals.reserve(64);

        for (const auto &component : m_analogCircuit.components()) {
            if (!component) {
                continue;
            }

            const auto componentId = component->getUUID();
            const auto componentTerminals = component->terminals();

            for (size_t terminalIdx = 0; terminalIdx < componentTerminals.size(); ++terminalIdx) {
                m_analogCircuit.disconnectTerminal(componentId, terminalIdx);
                terminals.push_back({componentId, terminalIdx});
            }
        }

        if (terminals.empty()) {
            return;
        }

        std::unordered_map<std::string, size_t> terminalIndexLookup;
        terminalIndexLookup.reserve(terminals.size());

        auto keyFor = [](const AnalogTerminalRef &ref) {
            return std::to_string((uint64_t)ref.componentId) + ":" + std::to_string(ref.terminalIdx);
        };

        for (size_t idx = 0; idx < terminals.size(); ++idx) {
            terminalIndexLookup[keyFor(terminals[idx])] = idx;
        }

        DisjointSet dsu(terminals.size());
        std::vector<bool> touched(terminals.size(), false);

        std::erase_if(m_analogConnections, [&](const auto &connection) {
            const auto aKey = keyFor(connection.a);
            const auto bKey = keyFor(connection.b);

            if (!terminalIndexLookup.contains(aKey) || !terminalIndexLookup.contains(bKey)) {
                return true;
            }

            const auto idxA = terminalIndexLookup[aKey];
            const auto idxB = terminalIndexLookup[bKey];

            dsu.unite(idxA, idxB);
            touched[idxA] = true;
            touched[idxB] = true;
            return false;
        });

        std::unordered_map<size_t, AnalogNodeId> groupNodes;
        for (size_t idx = 0; idx < terminals.size(); ++idx) {
            if (!touched[idx]) {
                continue;
            }

            const size_t root = dsu.find(idx);
            if (!groupNodes.contains(root)) {
                groupNodes[root] = m_analogCircuit.createNode();
            }

            const auto &terminalRef = terminals[idx];
            m_analogCircuit.connectTerminal(terminalRef.componentId,
                                            terminalRef.terminalIdx,
                                            groupNodes[root]);
        }
    }

    void SimulationEngine::scheduleDependantsOf(const UUID &compId) {
        const auto &dc = m_simEngineState.getDigitalComponent(compId);
        if (!dc) {
            return;
        }
        for (auto &pin : dc->outputConnections) {
            const auto &keyView = pin | std::views::keys;
            const std::set<UUID> uniqueEntities = std::set<UUID>(keyView.begin(), keyView.end());
            for (auto &ent : uniqueEntities) {
                const auto dependant = m_simEngineState.getDigitalComponent(ent);
                if (!dependant || !dependant->definition) {
                    continue;
                }
                const auto simDelay = dependant->definition->getSimDelay();
                scheduleEvent(ent,
                              compId,
                              m_currentSimTime + simDelay);
            }
        }
    }
} // namespace Bess::SimEngine
