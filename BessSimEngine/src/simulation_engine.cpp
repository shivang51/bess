#include "simulation_engine.h"
#include "component_catalog.h"
#include "entt/entity/fwd.hpp"
#include "entt_components.h"
#include "init_components.h"
#include "logger.h"
#include "types.h"

#include "plugin_manager.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>

// #define BESS_SE_ENABLE_LOG_EVENTS

#ifdef BESS_SE_ENABLE_LOG_EVENTS
    #define BESS_SE_LOG_EVENT(...) BESS_SE_TRACE(__VA_ARGS__);
#else
    #define BESS_SE_LOG_EVENT(...)
#endif // !BESS_SE_LOG_EVENT

namespace Bess::SimEngine {
    SimulationEngine &SimulationEngine::instance() {
        static SimulationEngine inst;
        return inst;
    }

    SimulationEngine::SimulationEngine() {
        Logger::getInstance().initLogger("BessSimEngine");
        initComponentCatalog();
        const auto &pluginMangaer = Plugins::PluginManager::getInstance();
        for (const auto &plugin : pluginMangaer.getLoadedPlugins()) {
            const auto comps = plugin.second->onComponentsRegLoad();
            for (const auto &comp : comps) {
                ComponentCatalog::instance().registerComponent(comp);
            }
            BESS_SE_INFO("Registered {} components from plugin {}", comps.size(), plugin.first);
        }
        Plugins::savePyThreadState();
        m_simThread = std::thread(&SimulationEngine::run, this);
    }

    void SimulationEngine::clear() {
        std::lock_guard lkEventQueue(m_queueMutex);
        m_eventSet.clear();

        std::lock_guard lkRegistry(m_registryMutex);
        m_registry.clear();
        m_uuidMap.clear();
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
    }

    void SimulationEngine::scheduleEvent(entt::entity e, entt::entity schedulerEntity, SimDelayNanoSeconds simTime) {
        const auto &digiComp = m_registry.get<DigitalComponent>(e);
        std::lock_guard lk(m_queueMutex);
        static uint64_t eventId = 0;
        SimulationEvent ev{simTime, e, schedulerEntity, eventId++};
        m_eventSet.insert(ev);
        m_queueCV.notify_all();
    }

    void SimulationEngine::clearEventsForEntity(entt::entity e) {
        std::lock_guard lk(m_queueMutex);
        std::erase_if(m_eventSet, [e](auto it) {
            return it.entity == e;
        });
    }

    entt::entity SimulationEngine::getEntityWithUuid(const UUID &uuid) const {
        if (m_uuidMap.contains(uuid))
            return m_uuidMap.at(uuid);

        auto view = m_registry.view<IdComponent>();
        for (const auto &ent : view) {
            auto &idComp = view.get<IdComponent>(ent);
            if (idComp.uuid == uuid)
                return ent;
        }
        return entt::null;
    }

    const UUID &SimulationEngine::addComponent(uint64_t defHash, int inputCount, int outputCount) {
        auto ent = m_registry.create();
        auto &idComp = m_registry.emplace<IdComponent>(ent);
        m_uuidMap.emplace(idComp.uuid, ent);
        const auto &def = ComponentCatalog::instance().getComponentDefinition(defHash);
        inputCount = (inputCount < 0 ? def->inputCount : inputCount);
        outputCount = (outputCount < 0 ? def->outputCount : outputCount);
        const auto &digi = m_registry.emplace<DigitalComponent>(ent, *(def.get()));
        if (def->auxData.type() == typeid(FlipFlopAuxData)) {
            const auto &ffData = std::any_cast<const FlipFlopAuxData &>(def->auxData);
            m_registry.emplace<FlipFlopComponent>(ent, 1);
        }
        // else if (type == ComponentType::STATE_MONITOR) {
        //         m_registry.emplace<StateMonitorComponent>(ent);
        //     }
        scheduleEvent(ent, entt::null, m_currentSimTime + def->delay);

        BESS_SE_INFO("Added component {}", def->name);
        return idComp.uuid;
    }

    bool SimulationEngine::connectComponent(const UUID &src, int srcPin, PinType srcType,
                                            const UUID &dst, int dstPin, PinType dstType, bool overrideConn) {
        auto srcEnt = getEntityWithUuid(src);
        auto dstEnt = getEntityWithUuid(dst);
        if (!m_registry.valid(srcEnt) || !m_registry.valid(dstEnt))
            return false;

        auto &srcComp = m_registry.get<DigitalComponent>(srcEnt);
        auto &dstComp = m_registry.get<DigitalComponent>(dstEnt);

        if (srcType == dstType) {
            BESS_SE_WARN("Cannot connect pins of the same type i.e. input -> input or output -> output");
            return false;
        }

        auto &outPins = (srcType == PinType::output ? srcComp.outputConnections : srcComp.inputConnections);
        auto &inPins = (dstType == PinType::input ? dstComp.inputConnections : dstComp.outputConnections);

        if (srcPin < 0 || srcPin >= static_cast<int>(outPins.size())) {
            BESS_SE_WARN("Invalid source pin index. Valid range: 0 to {}",
                         srcComp.outputConnections.size() - 1);
            return false;
        }
        if (dstPin < 0 || dstPin >= static_cast<int>(inPins.size())) {
            BESS_SE_WARN("Invalid source pin index. Valid range: 0 to {}",
                         srcComp.inputConnections.size() - 1);
            return false;
        }

        // Check for duplicate connection.
        auto &conns = outPins[srcPin];
        for (auto it = conns.begin(); it != conns.end(); ++it) {
            if (it->first == dst && it->second == dstPin) {
                if (overrideConn) {
                    conns.erase(it);
                    break;
                }
                BESS_SE_WARN("Connection already exists, skipping");
                return false;
            }
        }

        {
            // StateMonitorComponent *stateMonitorComp = nullptr;
            // PinType type = srcType;
            // ComponentPin pin;
            // if (srcComp.definition.type == ComponentType::STATE_MONITOR) {
            //     stateMonitorComp = m_registry.try_get<StateMonitorComponent>(srcEnt);
            //     type = srcType;
            //     pin = {dst, dstPin};
            // } else if (dstComp.definition.type == ComponentType::STATE_MONITOR) {
            //     stateMonitorComp = m_registry.try_get<StateMonitorComponent>(dstEnt);
            //     type = dstType;
            //     pin = {src, srcPin};
            // }
            //
            // if (stateMonitorComp) {
            //     stateMonitorComp->attacthTo(pin, type);
            //     BESS_SE_INFO("Attached state monitor");
            // }
        }

        outPins[srcPin].emplace_back(dst, dstPin);
        inPins[dstPin].emplace_back(src, srcPin);

        if (srcType == PinType::output) {
            srcComp.state.outputConnected[srcPin] = true;
            dstComp.state.inputConnected[dstPin] = true;
        } else {
            srcComp.state.inputConnected[dstPin] = true;
            dstComp.state.outputConnected[srcPin] = true;
        }

        scheduleEvent(dstEnt, srcEnt, m_currentSimTime + dstComp.definition.delay);
        m_connectionsCache.erase(dstEnt);
        m_connectionsCache.erase(srcEnt);
        BESS_SE_INFO("Connected components");
        return true;
    }

    void SimulationEngine::deleteComponent(const UUID &uuid) {
        auto ent = getEntityWithUuid(uuid);
        if (ent == entt::null)
            return;

        clearEventsForEntity(ent);
        std::vector<entt::entity> affected;

        {
            std::lock_guard lk(m_registryMutex);
            auto view = m_registry.view<DigitalComponent>();
            for (auto other : view) {
                if (other == ent)
                    continue;
                m_connectionsCache.erase(other);
                auto &comp = view.get<DigitalComponent>(other);
                bool lost = false;
                for (auto &pin : comp.inputConnections) {
                    size_t before = pin.size();
                    pin.erase(std::remove_if(pin.begin(), pin.end(),
                                             [&](auto &c) { return c.first == uuid; }),
                              pin.end());
                    if (pin.size() < before)
                        lost = true;
                }
                if (lost)
                    affected.push_back(other);
                for (auto &pin : comp.outputConnections) {
                    pin.erase(std::remove_if(pin.begin(), pin.end(),
                                             [&](auto &c) { return c.first == uuid; }),
                              pin.end());
                }
            }
            m_uuidMap.erase(uuid);
            auto &comp = view.get<DigitalComponent>(ent);
            m_registry.destroy(ent);
            m_connectionsCache.erase(ent);
        }

        for (auto e : affected) {
            if (m_registry.valid(e)) {
                auto &dc = m_registry.get<DigitalComponent>(e);
                scheduleEvent(e, entt::null, m_currentSimTime + dc.definition.delay);
            }
        }

        BESS_SE_INFO("Deleted component");
    }

    PinState SimulationEngine::getDigitalPinState(const UUID &uuid, PinType type, int idx) {
        auto ent = getEntityWithUuid(uuid);
        auto &comp = m_registry.get<DigitalComponent>(ent);
        return comp.state.outputStates[idx];
    }

    ConnectionBundle SimulationEngine::getConnections(const UUID &uuid) {
        auto ent = getEntityWithUuid(uuid);
        auto &comp = m_registry.get<DigitalComponent>(ent);
        return {comp.inputConnections, comp.outputConnections};
    }

    void SimulationEngine::setInputPinState(const UUID &uuid, int pinIdx, LogicState state) {
        auto ent = getEntityWithUuid(uuid);
        if (m_registry.all_of<ClockComponent>(ent))
            return;

        auto &comp = m_registry.get<DigitalComponent>(ent);

        comp.state.inputStates[pinIdx].state = state;
        comp.state.inputStates[pinIdx].lastChangeTime = m_currentSimTime;
        scheduleEvent(ent, entt::null, m_currentSimTime + comp.definition.delay);
    }

    void SimulationEngine::setOutputPinState(const UUID &uuid, int pinIdx, LogicState state) {
        auto ent = getEntityWithUuid(uuid);
        if (m_registry.all_of<ClockComponent>(ent))
            return;

        auto &comp = m_registry.get<DigitalComponent>(ent);

        comp.state.outputStates[pinIdx].state = state;
        comp.state.outputStates[pinIdx].lastChangeTime = m_currentSimTime;
        scheduleEvent(ent, entt::null, m_currentSimTime + comp.definition.delay);
    }

    void SimulationEngine::invertInputPinState(const UUID &uuid, int pinIdx) {
        auto ent = getEntityWithUuid(uuid);
        if (m_registry.all_of<ClockComponent>(ent))
            return;

        auto &comp = m_registry.get<DigitalComponent>(ent);

        auto state = comp.state.inputStates[pinIdx].state == LogicState::high ? LogicState::low : LogicState::high;
        comp.state.inputStates[pinIdx].state = state;
        comp.state.inputStates[pinIdx].lastChangeTime = m_currentSimTime;
        scheduleEvent(ent, entt::null, m_currentSimTime + comp.definition.delay);
    }

    bool SimulationEngine::updateClock(const UUID &uuid, bool enable, float freq, FrequencyUnit unit) {
        auto ent = getEntityWithUuid(uuid);
        bool hadClock = m_registry.all_of<ClockComponent>(ent);
        if (enable) {
            m_registry.get_or_emplace<ClockComponent>(ent).setup(freq, unit);
        } else if (hadClock) {
            m_registry.remove<ClockComponent>(ent);
        }
        clearEventsForEntity(ent);
        scheduleEvent(ent, entt::null, m_currentSimTime);
        return hadClock != enable;
    }

    const ComponentState &SimulationEngine::getComponentState(const UUID &uuid) {
        auto ent = getEntityWithUuid(uuid);
        assert(m_registry.all_of<DigitalComponent>(ent));
        const auto &connectedStatus = getIOPinsConnectedState(ent);
        const auto &comp = m_registry.get<DigitalComponent>(ent);
        return comp.state;
    }

    void SimulationEngine::deleteConnection(const UUID &compA, PinType pinAType, int idxA,
                                            const UUID &compB, PinType pinBType, int idxB) {
        auto entA = getEntityWithUuid(compA);
        auto entB = getEntityWithUuid(compB);
        if (!m_registry.valid(entA) || !m_registry.valid(entB))
            return;

        std::lock_guard lk(m_registryMutex);
        auto &compARef = m_registry.get<DigitalComponent>(entA);
        auto &compBRef = m_registry.get<DigitalComponent>(entB);

        auto &pinsA = (pinAType == PinType::input ? compARef.inputConnections : compARef.outputConnections);
        auto &pinsB = (pinBType == PinType::input ? compBRef.inputConnections : compBRef.outputConnections);

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
            if (pinAType == PinType::output) {
                compARef.state.outputConnected[idxA] = false;
            } else {
                compARef.state.inputConnected[idxA] = false;
            }
        }

        if (pinsB[idxB].empty()) {
            if (pinBType == PinType::output) {
                compBRef.state.outputConnected[idxB] = false;
            } else {
                compBRef.state.inputConnected[idxB] = false;
            }
        }

        // Schedule next simulation on the appropriate side
        entt::entity toSchedule = (pinAType == PinType::output ? entB : entA);
        auto &dc = m_registry.get<DigitalComponent>(toSchedule);
        scheduleEvent(toSchedule, entt::null, m_currentSimTime + dc.definition.delay);

        m_connectionsCache.erase(entA);
        m_connectionsCache.erase(entB);

        BESS_SE_INFO("Deleted connection");
    }

    const std::pair<std::vector<bool>, std::vector<bool>> &SimulationEngine::getIOPinsConnectedState(entt::entity e) {
        if (m_connectionsCache.contains(e))
            return m_connectionsCache.at(e);

        auto &comp = m_registry.get<DigitalComponent>(e);
        auto &[iPinsState, oPinsState] = m_connectionsCache[e];

        for (auto &pin : comp.inputConnections) {
            iPinsState.push_back(!pin.empty());
        }

        for (auto &pin : comp.outputConnections) {
            oPinsState.push_back(!pin.empty());
        }
        return m_connectionsCache.at(e);
    }

    std::vector<PinState> SimulationEngine::getInputPinsState(entt::entity ent) const {
        std::vector<PinState> states;

        const auto &comp = m_registry.get<DigitalComponent>(ent);

        for (const auto &pinConnections : comp.inputConnections) {
            PinState aggregatedPinState = {LogicState::low, SimTime(0)};

            if (pinConnections.empty()) {
                states.push_back(aggregatedPinState);
                continue;
            }

            // Iterate over all the output pins connected to this single input pin
            for (const auto &conn : pinConnections) {
                auto sourceEntity = getEntityWithUuid(conn.first);
                if (sourceEntity == entt::null)
                    continue;

                const auto &sourceComponent = m_registry.get<DigitalComponent>(sourceEntity);
                const auto &sourcePin = sourceComponent.state.outputStates[conn.second];

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

    bool SimulationEngine::simulateComponent(entt::entity e, const std::vector<PinState> &inputs) {
        auto &comp = m_registry.get<DigitalComponent>(e);
        const auto &def = comp.definition;
        BESS_SE_LOG_EVENT("Simulating {}, with delay {}ns", def.name, def.delay.count());
        BESS_SE_LOG_EVENT("\tInputs:");
        for (auto &inp : inputs) {
            BESS_SE_LOG_EVENT("\t\t{}", (int)inp.state);
        }
        BESS_SE_LOG_EVENT("");
        if (def.simulationFunction) {
            const auto newState = def.simulationFunction(inputs, m_currentSimTime, comp.state);

            BESS_SE_LOG_EVENT("\tState changed: {}", newState.isChanged ? "YES" : "NO");
            if (newState.isChanged) {
                comp.state = newState;
            }
            // print outputs of the new state
            BESS_SE_LOG_EVENT("\tOutputs changed to:");
            for (auto &outp : newState.outputStates) {
                BESS_SE_LOG_EVENT("\t\t{}", (bool)outp.state);
            }
            comp.state.inputStates = inputs;

            return newState.isChanged;
        }
        return false;
    }

    SimulationState SimulationEngine::getSimulationState() {
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
        return std::chrono::time_point_cast<std::chrono::milliseconds>(m_realWorldClock.now()).time_since_epoch();
    }

    std::chrono::seconds SimulationEngine::getSimulationTimeS() {
        return std::chrono::time_point_cast<std::chrono::seconds>(m_realWorldClock.now()).time_since_epoch();
    }

    void SimulationEngine::run() {
        auto state = Plugins::createPyThreadState();
        Plugins::releasePyThreadState(state);
        BESS_SE_INFO("[SimulationEngine] Simulation loop started");
        m_currentSimTime = SimTime(0);
        m_realWorldClock = std::chrono::steady_clock();

        while (!m_stopFlag.load()) {
            std::unique_lock queueLock(m_queueMutex);

            m_queueCV.wait(queueLock, [&] { return m_stopFlag.load() || !m_eventSet.empty(); });
            if (m_stopFlag.load())
                break;

            std::unique_lock stateLock(m_stateMutex);
            if (m_simState.load() == SimulationState::paused) {
                queueLock.unlock();
                m_stepFlag.store(false);
                m_stateCV.wait(stateLock, [&] { return m_stopFlag.load() || m_simState.load() == SimulationState::running || m_stepFlag.load(); });
                queueLock.lock();
            }

            if (m_stopFlag.load())
                break;

            if (m_eventSet.empty())
                continue;

            auto deltaTime = m_eventSet.begin()->simTime - m_currentSimTime;
            m_currentSimTime = m_eventSet.begin()->simTime;

            std::set<SimulationEvent> eventsToSim = {};
            while (!m_eventSet.empty() && m_eventSet.begin()->simTime == m_currentSimTime) {
                auto ev = *m_eventSet.begin();
                if (!eventsToSim.empty() && eventsToSim.rbegin()->schedulerEntity != ev.schedulerEntity)
                    break;
                eventsToSim.insert(ev);
                m_eventSet.erase(m_eventSet.begin());
            }

            BESS_SE_LOG_EVENT("");
            BESS_SE_LOG_EVENT("[SimulationEngine][t = {}ns][dt = {}ns] Picked {} events to simulate", m_currentSimTime.count(), deltaTime.count(), eventsToSim.size());

            std::unordered_map<entt::entity, std::vector<PinState>> inputsMap = {};

            for (auto &ev : eventsToSim) {
                inputsMap[ev.entity] = getInputPinsState(ev.entity);
            }
            BESS_SE_LOG_EVENT("[SimulationEngine] Selected {} unique entites to simulate", inputsMap.size());

            for (auto &[entity, inputs] : inputsMap) {
                queueLock.unlock();
                stateLock.unlock();

                {
                    std::lock_guard regLock(m_registryMutex);
                    if (!m_registry.valid(entity))
                        continue;

                    bool hasClk = m_registry.all_of<ClockComponent>(entity);
                    bool changed = simulateComponent(entity, inputs);

                    if (changed || hasClk) {
                        auto &dc = m_registry.get<DigitalComponent>(entity);

                        for (auto &pin : dc.outputConnections) {
                            std::set<entt::entity> uniqueEntities;
                            for (const auto &key : pin | std::views::keys) {
                                auto d = getEntityWithUuid(key);
                                uniqueEntities.insert(d);
                            }

                            for (auto &ent : uniqueEntities) {
                                scheduleEvent(ent, entity, m_currentSimTime + dc.definition.delay);
                            }
                        }
                    }

                    if (hasClk) {
                        auto &clk = m_registry.get<ClockComponent>(entity);
                        auto &dc = m_registry.get<DigitalComponent>(entity);
                        bool isHigh = (bool)dc.state.outputStates[0];
                        dc.state.outputStates[0] = isHigh ? PinState(LogicState::low, m_currentSimTime) : PinState(LogicState::high, m_currentSimTime);
                        clk.high = (bool)dc.state.outputStates[0];
                        scheduleEvent(entity, entt::null, m_currentSimTime + clk.getNextDelay());
                    }
                }

                queueLock.lock();
                stateLock.lock();
            }

            BESS_SE_LOG_EVENT("[BessSimEngine] Sim Cycle End");
            BESS_SE_LOG_EVENT("");

            if (!m_eventSet.empty()) {
                m_queueCV.wait_until(queueLock, std::chrono::steady_clock::now() +
                                                    (m_eventSet.begin()->simTime - m_currentSimTime));
            }
        }
    }

    Commands::CommandsManager &SimulationEngine::getCmdManager() {
        return m_cmdManager;
    }

    bool SimulationEngine::updateInputCount(const UUID &uuid, int n) {
        throw std::runtime_error("updateInputCount is not implemented yet");
        return false;
        auto ent = getEntityWithUuid(uuid);
        if (!m_registry.all_of<DigitalComponent>(ent))
            return false;

        auto &digiComp = m_registry.get<DigitalComponent>(ent);
        // digiComp.updateInputCount(n);
        return true;
    }

    std::vector<std::pair<float, bool>> SimulationEngine::getStateMonitorData(UUID uuid) {
        auto ent = getEntityWithUuid(uuid);
        if (!m_registry.try_get<StateMonitorComponent>(ent)) {
            BESS_SE_WARN("[SimulationEngine] getStateMonitorData was called on entity with no StateMonitorComponent: {}", (uint64_t)uuid);
            return {};
        }

        auto &comp = m_registry.get<StateMonitorComponent>(ent);
        return comp.timestepedBoolData;
    }

} // namespace Bess::SimEngine
