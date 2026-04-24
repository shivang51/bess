#include "simulation_engine.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "component_catalog.h"
#include "digital_component.h"
#include "drivers/digital_sim_driver.h"
#include "drivers/sim_driver.h"
#include "init_components.h"
#include "types.h"

#include "plugin_manager.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>

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
        loadDrivers();
        initDrivers();

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
        std::lock_guard lkEventQueue(m_queueMutex);
        m_eventSet.clear();

        std::lock_guard lkRegistry(m_registryMutex);
        m_simEngineState.reset();
        m_nextEventId = 0;
        m_currentSimTime = {};
        m_nets.clear();
        m_isNetUpdated = false;
    }

    SimulationEngine::~SimulationEngine() {
        destroy();
    }

    void SimulationEngine::destroy() {
        if (m_destroyed)
            return;

        clear();

        stopDrivers();
        destroyDrivers();
        unloadDrivers();

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

    void SimulationEngine::clearEventsForEntity(const UUID &id) {
        std::lock_guard lk(m_queueMutex);
        std::erase_if(m_eventSet, [id](auto it) {
            return it.compId == id;
        });
    }

    const UUID &SimulationEngine::addComponent(
        const std::shared_ptr<Drivers::ComponentDef> &definition,
        bool cloneDef) {
        for (const auto &driver : m_simDrivers) {
            if (driver->suuportsDef(definition)) {
                auto comp = driver->createComp(definition);
                return comp ? comp->getUuid() : UUID::null;
            }
        }

        BESS_WARN("No compatible driver found for {}.", definition->getName());
        return UUID::null;
    }

    std::pair<bool, std::string> SimulationEngine::canConnectComponents(const UUID &src, int srcSlot, SlotType srcType,
                                                                        const UUID &dst, int dstSlot, SlotType dstType) const {
        if (src == UUID::null || dst == UUID::null) {
            return {false, "Cannot connect to/from null component"};
        }

        if (!m_simEngineState.isComponentValid(src) || !m_simEngineState.isComponentValid(dst)) {
            return {false, "Source or destination component does not exist"};
        }

        if (srcType == dstType) {
            return {false, "Cannot connect pins of the same type i.e. input -> input or output -> output"};
        }

        const auto &srcComp = getComponent<Drivers::Digital::DigitalSimComponent>(src);
        const auto &dstComp = getComponent<Drivers::Digital::DigitalSimComponent>(dst);

        auto &outPins = srcType == SlotType::digitalOutput
                            ? srcComp->getOutputConnections()
                            : srcComp->getInputConnections();
        auto &inPins = dstType == SlotType::digitalInput
                           ? dstComp->getInputConnections()
                           : dstComp->getOutputConnections();

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

    bool SimulationEngine::connectComponent(const UUID &src, int srcSlot, SlotType srcType,
                                            const UUID &dst, int dstSlot, SlotType dstType, bool overrideConn) {

        return false;

        // auto [canConnect, errorMsg] = canConnectComponents(src, srcSlot, srcType, dst, dstSlot, dstType);
        // if (!canConnect) {
        //     BESS_WARN("Cannot connect components: {}", errorMsg);
        //     return false;
        // }
        //
        // const auto &srcComp = getComponent<Drivers::Digital::DigitalSimComponent>(src);
        // const auto &dstComp = getComponent<Drivers::Digital::DigitalSimComponent>(dst);
        //
        // auto &outPins = srcType == SlotType::digitalOutput
        //                     ? srcComp->getOutputConnections()
        //                     : srcComp->getInputConnections();
        // auto &inPins = dstType == SlotType::digitalInput
        //                    ? dstComp->getInputConnections()
        //                    : dstComp->getOutputConnections();
        //
        // outPins[srcSlot].emplace_back(dst, dstSlot);
        // inPins[dstSlot].emplace_back(src, srcSlot);
        //
        // if (srcType == SlotType::digitalOutput) {
        //     srcComp->getIsOutputConnected()[srcSlot] = true;
        //     dstComp->getIsInputConnected()[dstSlot] = true;
        // } else {
        //     srcComp->getIsInputConnected()[dstSlot] = true;
        //     dstComp->getIsOutputConnected()[srcSlot] = true;
        // }
        //
        // // Mix two nets
        // // Final net will have id of net with most components initially
        // if (srcComp->getNetUuid() != dstComp->getNetUuid()) {
        //     UUID finalNetId = UUID::null;
        //     UUID changedNetId = UUID::null;
        //     auto &srcNet = m_nets[srcComp->getNetUuid()];
        //     auto &dstNet = m_nets[dstComp->getNetUuid()];
        //
        //     if (srcNet.size() > dstNet.size()) {
        //         finalNetId = srcComp->getNetUuid();
        //         changedNetId = dstComp->getNetUuid();
        //     } else {
        //         finalNetId = dstComp->getNetUuid();
        //         changedNetId = srcComp->getNetUuid();
        //     }
        //
        //     auto &finalNet = m_nets[finalNetId];
        //     auto &changedNet = m_nets[changedNetId];
        //
        //     for (const auto &compUuid : changedNet.getComponents()) {
        //         if (compUuid != UUID::null) {
        //             const auto &comp = m_simEngineState.getComponent<
        //                 Drivers::Digital::DigitalSimComponent>(compUuid);
        //             comp->setNetUuid(finalNetId);
        //         }
        //         finalNet.addComponent(compUuid);
        //     }
        //     m_nets.erase(changedNetId);
        //     m_isNetUpdated = true;
        // }
        //
        // BESS_INFO("Connected components");
        // return true;
    }

    void SimulationEngine::deleteComponent(const UUID &uuid) {
        // if (uuid == UUID::null && !m_simEngineState.isComponentValid(uuid))
        //     return;
        //
        // clearEventsForEntity(uuid);
        // std::set<UUID> affected;
        // std::lock_guard lk(m_registryMutex);
        //
        // // Remove all connections to and from this component
        // {
        //     const auto &digiComp = m_simEngineState.getDigitalComponent(uuid);
        //
        //     for (const auto &pin : digiComp->outputConnections) {
        //         for (const auto &otherPin : pin) {
        //             const auto otherPinIdx = otherPin.second;
        //
        //             auto comp = m_simEngineState.getDigitalComponent(otherPin.first);
        //             auto &connections = comp->inputConnections[otherPinIdx];
        //
        //             auto removeIt = std::ranges::remove_if(connections, [&](auto &c) { return c.first == uuid; });
        //             connections.erase(removeIt.begin(), removeIt.end());
        //
        //             comp->state.inputConnected[otherPinIdx] = !connections.empty();
        //             affected.insert(otherPin.first);
        //         }
        //     }
        //
        //     for (const auto &pin : digiComp->inputConnections) {
        //         for (const auto &otherPin : pin) {
        //             const auto otherPinIdx = otherPin.second;
        //
        //             auto comp = m_simEngineState.getDigitalComponent(otherPin.first);
        //             auto &connections = comp->outputConnections[otherPinIdx];
        //
        //             auto removeIt = std::ranges::remove_if(connections, [&](auto &c) { return c.first == uuid; });
        //             connections.erase(removeIt.begin(), removeIt.end());
        //
        //             comp->state.outputConnected[otherPinIdx] = !connections.empty();
        //             affected.insert(otherPin.first);
        //         }
        //     }
        // }
        //
        // // update the nets
        // {
        //     auto comp = m_simEngineState.getDigitalComponent(uuid);
        //     auto &ogNet = m_nets[comp->netUuid];
        //     ogNet.removeComponent(uuid);
        //     if (ogNet.size() == 0) {
        //         m_nets.erase(comp->netUuid);
        //     }
        //
        //     updateNets(std::vector<UUID>(affected.begin(), affected.end()));
        // }
        //
        // m_simEngineState.removeDigitalComponent(uuid);
        //
        // for (const auto e : affected) {
        //     if (!m_simEngineState.isComponentValid(e))
        //         continue;
        //
        //     const auto &dc = m_simEngineState.getDigitalComponent(e);
        //     scheduleEvent(e, UUID::null, m_currentSimTime + dc->definition->getSimDelay());
        // }
        //
        // BESS_INFO("Deleted component {}", (uint64_t)uuid);
    }

    bool SimulationEngine::updateNets(const std::vector<UUID> &startCompIds) {
        return false;
        // std::unordered_map<UUID, std::vector<UUID>> connGraphs;
        // std::unordered_map<UUID, bool> visited;
        //
        // for (const UUID &e : startCompIds) {
        //     if (visited[e])
        //         continue;
        //     connGraphs[e] = getConnGraph(e);
        //
        //     for (const auto &entInGraph : connGraphs[e]) {
        //         visited[entInGraph] = true;
        //     }
        // }
        //
        // if (connGraphs.size() <= 1) {
        //     return false;
        // }
        //
        // for (const auto &graphPair : connGraphs) {
        //     Net newNet{};
        //     for (const auto &entInGraph : graphPair.second) {
        //         const auto &compInGraph = m_simEngineState.getDigitalComponent(entInGraph);
        //
        //         if (m_nets.contains(compInGraph->netUuid)) {
        //             auto &prevNet = m_nets[compInGraph->netUuid];
        //             prevNet.removeComponent(entInGraph);
        //             if (prevNet.size() == 0) {
        //                 m_nets.erase(prevNet.getUUID());
        //             }
        //         }
        //
        //         compInGraph->netUuid = newNet.getUUID();
        //         newNet.addComponent(entInGraph);
        //     }
        //     m_nets[newNet.getUUID()] = newNet;
        // }
        //
        // m_isNetUpdated = connGraphs.size() > 1;
        //
        // return true;
    }

    SlotState SimulationEngine::getDigitalSlotState(const UUID &uuid, SlotType type, int idx) {
        if (!m_simEngineState.isComponentValid(uuid)) {
            BESS_WARN("[getDigitalPinState] Component with UUID {} is invalid", (uint64_t)uuid);
            return {LogicState::unknown, SimTime(0)};
        }

        const auto &comp = getComponent<Drivers::Digital::DigitalSimComponent>(uuid);

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

    ConnectionBundle SimulationEngine::getConnections(const UUID &uuid) {
        ConnectionBundle bundle;
        // if (!m_simEngineState.isComponentValid(uuid)) {
        //     BESS_WARN("[getConnections] Component with UUID {} is invalid",
        //               (uint64_t)uuid);
        //     return bundle;
        // }
        // const auto &comp = m_simEngineState.getDigitalComponent(uuid);
        //
        // bundle.inputs = comp->inputConnections;
        // bundle.outputs = comp->outputConnections;

        return bundle;
    }

    void SimulationEngine::setInputSlotState(const UUID &uuid, int pinIdx, LogicState state) {
        const auto comp = getDigitalComponent(uuid);
        if (!comp) {
            BESS_WARN("[setInputSlotState] Component with UUID {} is invalid", (uint64_t)uuid);
            return;
        }

        auto &inputs = comp->getInputStates();

        if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= inputs.size()) {
            BESS_WARN("[setInputSlotState] Input slot index {} out of range for component {}",
                      pinIdx,
                      (uint64_t)uuid);
            return;
        }

        inputs[pinIdx].state = state;
        inputs[pinIdx].lastChangeTime = m_currentSimTime;

        // FIXME
        // scheduleEvent(uuid, UUID::null, m_currentSimTime + comp->definition->getSimDelay());
    }

    void SimulationEngine::setOutputSlotState(const UUID &uuid, int pinIdx, LogicState state) {
        const auto comp = getDigitalComponent(uuid);
        if (!comp) {
            BESS_WARN("[setInputSlotState] Component with UUID {} is invalid", (uint64_t)uuid);
            return;
        }

        auto &outputs = comp->getOutputStates();

        if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= outputs.size()) {
            BESS_WARN("[setInputSlotState] Output slot index {} out of range for component {}",
                      pinIdx,
                      (uint64_t)uuid);
            return;
        }

        outputs[pinIdx].state = state;
        outputs[pinIdx].lastChangeTime = m_currentSimTime;

        // FIXME
        // scheduleEvent(uuid, UUID::null, m_currentSimTime + comp->definition->getSimDelay());
    }

    void SimulationEngine::invertInputSlotState(const UUID &uuid, int pinIdx) {
        // const auto comp = m_simEngineState.getDigitalComponent(uuid);
        // if (!comp) {
        //     BESS_WARN("[invertInputSlotState] Component with UUID {} is invalid", (uint64_t)uuid);
        //     return;
        // }
        //
        // if (pinIdx < 0 || static_cast<size_t>(pinIdx) >= comp->state.inputStates.size()) {
        //     BESS_WARN("[invertInputSlotState] Input slot index {} out of range for component {}",
        //               pinIdx,
        //               (uint64_t)uuid);
        //     return;
        // }
        //
        // const auto state = comp->state.inputStates[pinIdx].state == LogicState::high
        //                        ? LogicState::low
        //                        : LogicState::high;
        //
        // comp->state.inputStates[pinIdx].state = state;
        // comp->state.inputStates[pinIdx].lastChangeTime = m_currentSimTime;
        // scheduleEvent(uuid, UUID::null, m_currentSimTime + comp->definition->getSimDelay());
    }

    const ComponentState &SimulationEngine::getComponentState(const UUID &uuid) {
        return {};
        // const auto &comp = m_simEngineState.getDigitalComponent(uuid);
        // return comp->state;
    }

    const std::shared_ptr<Drivers::ComponentDef> &SimulationEngine::getComponentDefinition(
        const UUID &uuid) const {
        const auto &comp = getComponent<Drivers::SimComponent>(uuid);
        return comp->getDefinition();
    }

    void SimulationEngine::deleteConnection(const UUID &compA, SlotType pinAType, int idxA,
                                            const UUID &compB, SlotType pinBType, int idxB) {

        // assert(m_simEngineState.isComponentValid(compA) ||
        //        m_simEngineState.isComponentValid(compB));
        // std::lock_guard lk(m_registryMutex);
        // const auto compARef = m_simEngineState.getDigitalComponent(compA);
        // const auto compBRef = m_simEngineState.getDigitalComponent(compB);
        //
        // auto &pinsA = pinAType == SlotType::digitalInput
        //                   ? compARef->inputConnections
        //                   : compARef->outputConnections;
        // auto &pinsB = pinBType == SlotType::digitalInput
        //                   ? compBRef->inputConnections
        //                   : compBRef->outputConnections;
        //
        // // Erase connection from A -> B
        // pinsA[idxA].erase(
        //     std::remove_if(pinsA[idxA].begin(), pinsA[idxA].end(),
        //                    [&](auto &c) { return c.first == compB && c.second == idxB; }),
        //     pinsA[idxA].end());
        // // Erase connection from B -> A
        // pinsB[idxB].erase(
        //     std::remove_if(pinsB[idxB].begin(), pinsB[idxB].end(),
        //                    [&](auto &c) { return c.first == compA && c.second == idxA; }),
        //     pinsB[idxB].end());
        //
        // if (pinsA[idxA].empty()) {
        //     if (pinAType == SlotType::digitalOutput) {
        //         compARef->state.outputConnected[idxA] = false;
        //     } else {
        //         compARef->state.inputConnected[idxA] = false;
        //     }
        // }
        //
        // if (pinsB[idxB].empty()) {
        //     if (pinBType == SlotType::digitalOutput) {
        //         compBRef->state.outputConnected[idxB] = false;
        //     } else {
        //         compBRef->state.inputConnected[idxB] = false;
        //     }
        // }
        //
        // // Schedule next simulation on the appropriate side
        // const UUID toSchedule = pinAType == SlotType::digitalOutput ? compB : compA;
        //
        // updateNets({compA, compB});
        //
        // const auto &dc = m_simEngineState.getDigitalComponent(toSchedule);
        // scheduleEvent(toSchedule, UUID::null, m_currentSimTime + dc->definition->getSimDelay());
        //
        // BESS_INFO("Deleted connection");
    }

    std::vector<SlotState> SimulationEngine::getInputSlotsState(UUID compId) const {
        for (const auto &driver : m_simDrivers) {
            if (!driver->hasComponent(compId)) {
                continue;
            }

            // FIXME: Collapse inputs
            auto comp = driver->template getComponent<Drivers::Digital::DigitalSimComponent>(compId);

            if (comp) {
                return comp->getInputStates();
            }
        }

        return {};
    }

    bool SimulationEngine::simulateComponent(const UUID &compId, const std::vector<SlotState> &inputs) {
        return false;
        //         const auto &comp = m_simEngineState.getDigitalComponent(compId);
        //         BESS_ASSERT(comp,
        //                     std::format("Component {} is invalid", (uint64_t)compId));
        //         const auto &def = comp->definition;
        //         BESS_ASSERT(def,
        //                     std::format("Component definition of {} is invalid", (uint64_t)comp->id));
        // #ifdef BESS_ENABLE_LOG_EVENTS
        //         BESS_LOG_EVENT("Simulating {}, with delay {}ns", def->getName(), def->getSimDelay().count());
        //         BESS_LOG_EVENT("\tInputs:");
        //         for (auto &inp : inputs) {
        //             BESS_LOG_EVENT("\t\t{}", (int)inp.state);
        //         }
        //         BESS_LOG_EVENT("");
        // #endif // BESS_ENABLE_LOG_EVENTS
        //
        //         auto &simFunction = def->getSimulationFunction();
        //
        //         if (!simFunction) {
        //             BESS_ERROR("Component {} does not have a simulation function defined. Skipping simulation.", def->getName());
        //             assert(false && "Simulation function not defined for component");
        //             return false;
        //         }
        //
        //         comp->state.simError = false;
        //         auto oldState = comp->state;
        //         ComponentState newState;
        //         try {
        //             newState = def->getSimulationFunction()(inputs, m_currentSimTime, comp->state);
        //         } catch (std::exception &ex) {
        //             BESS_ERROR("Exception during simulation of component {}. Output won't be updated: {}",
        //                        def->getName(), ex.what());
        //             comp->state.simError = true;
        //             comp->state.errorMessage = ex.what();
        //             comp->state.isChanged = false;
        //         }
        //
        //         comp->state.inputStates = inputs;
        //
        //         BESS_LOG_EVENT("\tState changed: {}", newState.isChanged ? "YES" : "NO");
        //
        //         if (newState.isChanged && !comp->state.simError) {
        //             comp->state = newState;
        //             comp->definition->onStateChange(oldState, comp->state);
        //             comp->dispatchStateChange(oldState, newState);
        //             BESS_LOG_EVENT("\tOutputs changed to:");
        //             for (auto &outp : newState.outputStates) {
        //                 BESS_LOG_EVENT("\t\t{}", (bool)outp.state);
        //             }
        //         }
        //
        //         // FIXME: State monitor logic
        //         // if (auto *stateMonitor = m_registry.try_get<StateMonitorComponent>(e)) {
        //         //     stateMonitor->appendState(newState.inputStates[0].lastChangeTime,
        //         //                               newState.inputStates[0].state);
        //         // }
        //         //
        //
        //         return newState.isChanged;
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
        runDrivers();
        while (!m_stopFlag.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        //
        // auto state = Plugins::capturePyThreadState();
        // Plugins::releasePyThreadState(state);
        // BESS_INFO("[SimulationEngine] Simulation loop started");
        // m_currentSimTime = SimTime(0);
        //
        // while (!m_stopFlag.load()) {
        //     std::unique_lock queueLock(m_queueMutex);
        //
        //     m_queueCV.wait(queueLock, [&] { return m_stopFlag.load() || !m_eventSet.empty(); });
        //     if (m_stopFlag.load())
        //         break;
        //
        //     std::unique_lock stateLock(m_stateMutex);
        //     if (m_simState.load() == SimulationState::paused) {
        //         queueLock.unlock();
        //         m_stepFlag.store(false);
        //         m_stateCV.wait(stateLock, [&] { return m_stopFlag.load() ||
        //                                                m_simState.load() == SimulationState::running ||
        //                                                m_stepFlag.load(); });
        //         queueLock.lock();
        //     }
        //
        //     if (m_stopFlag.load())
        //         break;
        //
        //     if (m_eventSet.empty())
        //         continue;
        //
        //     auto deltaTime = m_eventSet.begin()->simTime - m_currentSimTime;
        //     m_currentSimTime = m_eventSet.begin()->simTime;
        //
        //     std::set<SimulationEvent> eventsToSim = {};
        //     while (!m_eventSet.empty() && m_eventSet.begin()->simTime == m_currentSimTime) {
        //         auto ev = *m_eventSet.begin();
        //         if (!eventsToSim.empty() && eventsToSim.rbegin()->schedulerId != ev.schedulerId)
        //             break;
        //         eventsToSim.insert(ev);
        //         m_eventSet.erase(m_eventSet.begin());
        //     }
        //
        //     BESS_LOG_EVENT("");
        //     BESS_LOG_EVENT("[SimulationEngine][t = {}ns][dt = {}ns] Picked {} events to simulate",
        //                    m_currentSimTime.count(), deltaTime.count(), eventsToSim.size());
        //
        //     std::unordered_map<UUID, std::vector<SlotState>> inputsMap = {};
        //
        //     for (auto &ev : eventsToSim) {
        //         inputsMap[ev.compId] = getInputSlotsState(ev.compId);
        //     }
        //     BESS_LOG_EVENT("[SimulationEngine] Selected {} unique entites to simulate", inputsMap.size());
        //
        //     m_isSimulating = true;
        //     for (auto &[compId, inputs] : inputsMap) {
        //         queueLock.unlock();
        //         stateLock.unlock();
        //
        //         {
        //             std::lock_guard regLock(m_registryMutex);
        //
        //             const bool changed = simulateComponent(compId, inputs);
        //             const auto &dc = m_simEngineState.getDigitalComponent(compId);
        //
        //             if (changed) {
        //                 scheduleDependantsOf(compId);
        //             }
        //
        //             if (dc->definition->getShouldAutoReschedule()) {
        //                 scheduleEvent(compId,
        //                               UUID::null,
        //                               dc->definition->getRescheduleTime(m_currentSimTime));
        //             }
        //         }
        //
        //         queueLock.lock();
        //         stateLock.lock();
        //     }
        //     m_isSimulating = false;
        //     m_queueCV.notify_all();
        //
        //     BESS_LOG_EVENT("[BessSimEngine] Sim Cycle End");
        //     BESS_LOG_EVENT("");
        //
        //     if (!m_eventSet.empty()) {
        //         m_queueCV.wait_until(queueLock, std::chrono::steady_clock::now() +
        //                                             (m_eventSet.begin()->simTime - m_currentSimTime));
        //     } else {
        //         BESS_DEBUG("[BessSimEngine] Event queue empty, waiting for new events");
        //         m_queueCV.notify_all();
        //     }
        // }
    }

    bool SimulationEngine::updateInputCount(const UUID &uuid, int n) {

        throw std::runtime_error("updateInputCount is not implemented yet");
        //
        // if (!m_simEngineState.isComponentValid(uuid))
        //     return false;
        //
        // const auto digiComp = m_simEngineState.getDigitalComponent(uuid);
        // // digiComp->updateInputCount(n);
        // return true;
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
        return {};
        // std::vector<UUID> graph;
        // std::set<UUID> visited;
        // std::vector<UUID> toVisit;
        // toVisit.push_back(start);
        //
        // while (!toVisit.empty()) {
        //     auto current = toVisit.back();
        //     toVisit.pop_back();
        //
        //     if (visited.contains(current))
        //         continue;
        //
        //     visited.insert(current);
        //     graph.push_back(current);
        //
        //     const auto &comp = m_simEngineState.getDigitalComponent(current);
        //
        //     for (const auto &pinConnections : comp->inputConnections) {
        //         for (const auto &conn : pinConnections) {
        //             if (conn.first != UUID::null && !visited.contains(conn.first)) {
        //                 toVisit.push_back(conn.first);
        //             }
        //         }
        //     }
        //
        //     for (const auto &pinConnections : comp->outputConnections) {
        //         for (const auto &conn : pinConnections) {
        //             if (conn.first != UUID::null && !visited.contains(conn.first)) {
        //                 toVisit.push_back(conn.first);
        //             }
        //         }
        //     }
        // }
        //
        // return graph;
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
        return {};
        // if (!m_nets.contains(netUuid))
        //     return {};
        //
        // BESS_INFO("\nGenerating truth table for net {}", (uint64_t)netUuid);
        // const auto &net = m_nets.at(netUuid);
        //
        // TruthTable truthTable;
        //
        // std::vector<UUID> components;
        //
        // std::vector<std::pair<UUID, int>> inputs;
        // std::vector<std::pair<UUID, int>> outputs;
        // std::unordered_map<UUID, size_t> inputSlotCounts;
        //
        // for (const auto &compUuid : net.getComponents()) {
        //     if (compUuid == UUID::null)
        //         continue;
        //
        //     const auto &comp = m_simEngineState.getDigitalComponent(compUuid);
        //     bool isInput = comp->definition->getBehaviorType() == ComponentBehaviorType::input;
        //     bool isOutput = comp->definition->getBehaviorType() == ComponentBehaviorType::output;
        //
        //     if (isInput) {
        //         for (int i = 0; i < comp->definition->getOutputSlotsInfo().count; i++) {
        //             inputs.emplace_back(compUuid, i);
        //             truthTable.inputUuids.push_back(compUuid);
        //         }
        //     } else if (isOutput) {
        //         for (int i = 0; i < comp->definition->getInputSlotsInfo().count; i++) {
        //             outputs.emplace_back(compUuid, i);
        //             truthTable.outputUuids.push_back(compUuid);
        //         }
        //     } else {
        //         components.emplace_back(compUuid);
        //     }
        // }
        //
        // if (inputs.size() == 0 || outputs.size() == 0) {
        //     BESS_WARN("Cannot generate truth table for net {} without input or output components",
        //               (uint64_t)netUuid);
        //     return {};
        // }
        //
        // BESS_INFO("Truth table will have {} inputs and {} outputs", inputs.size(), outputs.size());
        //
        // const size_t numInputs = inputs.size();
        // const size_t numCombinations = 1 << numInputs;
        //
        // std::vector<std::vector<LogicState>> tableData(numCombinations,
        //                                                std::vector<LogicState>(
        //                                                    numInputs + outputs.size(),
        //                                                    LogicState::low));
        //
        // BESS_INFO("Truth table dimensions: {} rows x {} columns",
        //           tableData.size(),
        //           tableData[0].size());
        //
        // BESS_INFO("Total combinations to simulate: {}", numCombinations);
        //
        // for (size_t comb = 0; comb < numCombinations; comb++) {
        //     BESS_INFO("Simulating combination {}/{}: ", comb + 1, numCombinations);
        //     size_t runningIdx = numInputs - 1, i = 0;
        //     for (const auto &[inpUuid, slotIdx] : inputs) {
        //         std::lock_guard lk(m_registryMutex);
        //         const LogicState state = (comb & (1 << i)) ? LogicState::high : LogicState::low;
        //         setOutputSlotState(inpUuid, slotIdx, state);
        //         tableData[comb][runningIdx] = state;
        //         runningIdx--;
        //         i++;
        //     }
        //
        //     BESS_INFO("Waiting for simulation to stabilize...");
        //
        //     // Wait for simulation to stabilize
        //     {
        //         std::unique_lock<std::mutex> lock(m_queueMutex);
        //         m_queueCV.wait(lock, [&] {
        //             return isSimStableLocked();
        //         });
        //     }
        //
        //     BESS_INFO("Simulation stabilized. Recording outputs");
        //
        //     // FIXME: Temporary sleep to ensure all events are processed
        //     std::this_thread::sleep_for(std::chrono::milliseconds(1));
        //
        //     std::lock_guard lk(m_queueMutex);
        //     for (size_t i = 0; i < outputs.size(); i++) {
        //         const auto states = getInputSlotsState(outputs[i].first);
        //         tableData[comb][numInputs + i] = states[outputs[i].second].state;
        //         BESS_DEBUG("Output component {} slot {} state: {}",
        //                    (uint64_t)outputs[i].first,
        //                    outputs[i].second,
        //                    (int)tableData[comb][numInputs + i]);
        //     }
        // }
        //
        // BESS_INFO("Truth table generation completed for net {}\n", (uint64_t)netUuid);
        // std::ranges::reverse(truthTable.inputUuids);
        // truthTable.table = tableData;
        //
        // return truthTable;
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

    std::shared_ptr<Drivers::Digital::DigitalSimComponent> SimulationEngine::getDigitalComponent(
        const UUID &uuid) const {
        return getComponent<Drivers::Digital::DigitalSimComponent>(uuid);
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

    void SimulationEngine::scheduleDependantsOf(const UUID &compId) {
        // const auto &dc = m_simEngineState.getDigitalComponent(compId);
        // if (!dc) {
        //     return;
        // }
        // for (auto &pin : dc->outputConnections) {
        //     const auto &keyView = pin | std::views::keys;
        //     const std::set<UUID> uniqueEntities = std::set<UUID>(keyView.begin(), keyView.end());
        //     for (auto &ent : uniqueEntities) {
        //         const auto dependant = m_simEngineState.getDigitalComponent(ent);
        //         if (!dependant || !dependant->definition) {
        //             continue;
        //         }
        //         const auto simDelay = dependant->definition->getSimDelay();
        //         scheduleEvent(ent,
        //                       compId,
        //                       m_currentSimTime + simDelay);
        //     }
        // }
    }

    void SimulationEngine::loadDrivers() {
        auto digDriver = std::make_shared<Drivers::Digital::DigitalSimDriver>();
        BESS_INFO("Loaded driver {}", digDriver->getName());
        m_simDrivers.emplace_back(std::move(digDriver));
    }

    void SimulationEngine::unloadDrivers() {
        const size_t n = m_simDrivers.size();
        m_simDrivers.clear();
        BESS_INFO("Unloaded all({}) drivers", n);
    }

    void SimulationEngine::initDrivers() {
        for (auto &driver : m_simDrivers) {
            driver->init();
            BESS_INFO("Initialized driver {}", driver->getName());
        }
    }

    void SimulationEngine::destroyDrivers() {
        for (auto &driver : m_simDrivers) {
            driver->destroy();
            BESS_INFO("Destroyed driver {}", driver->getName());
        }

        m_driverThreads.clear();
    }

    void SimulationEngine::runDrivers() {
        for (auto &driver : m_simDrivers) {
            m_driverThreads.emplace_back([driver]() { driver->run(); });
            BESS_INFO("Started driver thread for {}", driver->getName());
        }
    }

    void SimulationEngine::stopDrivers() {
        for (auto &driver : m_simDrivers) {
            driver->stop();
            BESS_INFO("Stopped driver {}", driver->getName());
        }

        for (auto &thread : m_driverThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
} // namespace Bess::SimEngine
