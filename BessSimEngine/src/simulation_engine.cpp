#include "simulation_engine.h"
#include "component_catalog.h"
#include "entt/entity/fwd.hpp"
#include "entt_components.h"
#include "init_components.h"
#include "logger.h"
#include "types.h"
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
        m_stopFlag.store(true);
        m_queueCV.notify_all();
        m_stateCV.notify_all();
        if (m_simThread.joinable())
            m_simThread.join();
    }

    void SimulationEngine::scheduleEvent(entt::entity e, entt::entity schedulerEntity, SimDelayNanoSeconds simTime) {
        const auto &digiComp = m_registry.get<DigitalComponent>(e);
        if (digiComp.type == ComponentType::OUTPUT)
            return;

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

    const UUID &SimulationEngine::addComponent(ComponentType type, int inputCount, int outputCount) {
        auto ent = m_registry.create();
        auto &idComp = m_registry.emplace<IdComponent>(ent);
        m_uuidMap.emplace(idComp.uuid, ent);
        const auto def = ComponentCatalog::instance().getComponentDefinition(type);
        inputCount = (inputCount < 0 ? def->inputCount : inputCount);
        outputCount = (outputCount < 0 ? def->outputCount : outputCount);
        if (type == ComponentType::NOT) {
            outputCount = inputCount;
        }
        auto &digi = m_registry.emplace<DigitalComponent>(ent, type, inputCount, outputCount,
                                                          def->delay, def->getExpressions(inputCount));
        if (type == ComponentType::FLIP_FLOP_JK || type == ComponentType::FLIP_FLOP_SR ||
            type == ComponentType::FLIP_FLOP_D || type == ComponentType::FLIP_FLOP_T) {
            m_registry.emplace<FlipFlopComponent>(ent, ComponentType(type), 1);
        } else if (type == ComponentType::STATE_MONITOR) {
            m_registry.emplace<StateMonitorComponent>(ent);
        }
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

        if (srcType == dstType && srcComp.type != ComponentType::STATE_MONITOR && dstComp.type != ComponentType::STATE_MONITOR) {
            BESS_SE_WARN("Cannot connect pins of the same type i.e. input -> input or output -> output");
            return false;
        }

        auto &outPins = (srcType == PinType::output ? srcComp.outputPins : srcComp.inputPins);
        auto &inPins = (dstType == PinType::input ? dstComp.inputPins : dstComp.outputPins);

        if (srcPin < 0 || srcPin >= static_cast<int>(outPins.size())) {
            BESS_SE_WARN("Invalid source pin index. Valid range: 0 to {}",
                         srcComp.outputPins.size() - 1);
            return false;
        }
        if (dstPin < 0 || dstPin >= static_cast<int>(inPins.size())) {
            BESS_SE_WARN("Invalid source pin index. Valid range: 0 to {}",
                         srcComp.inputPins.size() - 1);
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
            StateMonitorComponent *stateMonitorComp = nullptr;
            PinType type = srcType;
            ComponentPin pin;
            if (srcComp.type == ComponentType::STATE_MONITOR) {
                stateMonitorComp = m_registry.try_get<StateMonitorComponent>(srcEnt);
                type = srcType;
                pin = {dst, dstPin};
            } else if (dstComp.type == ComponentType::STATE_MONITOR) {
                stateMonitorComp = m_registry.try_get<StateMonitorComponent>(dstEnt);
                type = dstType;
                pin = {src, srcPin};
            }

            if (stateMonitorComp) {
                stateMonitorComp->attacthTo(pin, type);
                BESS_SE_INFO("Attached state monitor");
            }
        }

        outPins[srcPin].emplace_back(dst, dstPin);
        inPins[dstPin].emplace_back(src, srcPin);
        scheduleEvent(dstEnt, srcEnt, m_currentSimTime + dstComp.delay);
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
                for (auto &pin : comp.inputPins) {
                    size_t before = pin.size();
                    pin.erase(std::remove_if(pin.begin(), pin.end(),
                                             [&](auto &c) { return c.first == uuid; }),
                              pin.end());
                    if (pin.size() < before)
                        lost = true;
                }
                if (lost)
                    affected.push_back(other);
                for (auto &pin : comp.outputPins) {
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
                scheduleEvent(e, entt::null, m_currentSimTime + dc.delay);
            }
        }

        BESS_SE_INFO("Deleted component");
    }

    bool SimulationEngine::getDigitalPinState(const UUID &uuid, PinType type, int idx) {
        auto ent = getEntityWithUuid(uuid);
        auto &comp = m_registry.get<DigitalComponent>(ent);
        return comp.outputStates[idx].state == LogicState::high;
    }

    ConnectionBundle SimulationEngine::getConnections(const UUID &uuid) {
        auto ent = getEntityWithUuid(uuid);
        auto &comp = m_registry.get<DigitalComponent>(ent);
        return {comp.inputPins, comp.outputPins};
    }

    void SimulationEngine::setDigitalInput(const UUID &uuid, bool value) {
        auto ent = getEntityWithUuid(uuid);
        if (m_registry.all_of<ClockComponent>(ent))
            return;

        auto &comp = m_registry.get<DigitalComponent>(ent);

        assert(comp.type == ComponentType::INPUT);

        auto logic = value ? LogicState::high : LogicState::low;
        if (comp.outputStates[0].state == logic)
            return;
        comp.outputStates[0].state = logic;
        comp.outputStates[0].lastChangeTime = m_currentSimTime;
        for (auto &conn : comp.outputPins[0]) {
            auto dest = getEntityWithUuid(conn.first);
            if (dest != entt::null) {
                auto &dc = m_registry.get<DigitalComponent>(dest);
                scheduleEvent(dest, ent, m_currentSimTime + dc.delay);
            }
        }
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

    ComponentState SimulationEngine::getComponentState(const UUID &uuid) {
        auto ent = getEntityWithUuid(uuid);
        assert(m_registry.all_of<DigitalComponent>(ent));
        const auto &connectedStatus = getIOPinsConnectedState(ent);
        const auto &comp = m_registry.get<DigitalComponent>(ent);
        ComponentState st{
            .inputStates = comp.inputStates,
            .inputConnected = connectedStatus.first,
            .outputStates = comp.outputStates,
            .outputConnected = connectedStatus.second,
            .auxData = comp.auxData};
        return st;
    }

    ComponentType SimulationEngine::getComponentType(const UUID &uuid) {
        auto ent = getEntityWithUuid(uuid);
        if (auto *comp = m_registry.try_get<DigitalComponent>(ent)) {
            return comp->type;
        }
        throw std::runtime_error("Digital Component was not found for entity with uuid " + std::to_string(uuid));
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

        auto &pinsA = (pinAType == PinType::input ? compARef.inputPins : compARef.outputPins);
        auto &pinsB = (pinBType == PinType::input ? compBRef.inputPins : compBRef.outputPins);

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

        // Schedule next simulation on the appropriate side
        entt::entity toSchedule = (pinAType == PinType::output ? entB : entA);
        auto &dc = m_registry.get<DigitalComponent>(toSchedule);
        scheduleEvent(toSchedule, entt::null, m_currentSimTime + dc.delay);

        m_connectionsCache.erase(entA);
        m_connectionsCache.erase(entB);

        BESS_SE_INFO("Deleted connection");
    }

    const std::pair<std::vector<bool>, std::vector<bool>> &SimulationEngine::getIOPinsConnectedState(entt::entity e) {
        if (m_connectionsCache.contains(e))
            return m_connectionsCache.at(e);

        auto &comp = m_registry.get<DigitalComponent>(e);
        auto &[iPinsState, oPinsState] = m_connectionsCache[e];

        for (auto &pin : comp.inputPins) {
            iPinsState.push_back(!pin.empty());
        }

        for (auto &pin : comp.outputPins) {
            oPinsState.push_back(!pin.empty());
        }
        return m_connectionsCache.at(e);
    }

    std::vector<PinState> SimulationEngine::getInputPinsState(entt::entity ent) const {
        std::vector<PinState> states;

        const auto &comp = m_registry.get<DigitalComponent>(ent);

        for (const auto &pinConnections : comp.inputPins) {
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
                const auto &sourcePin = sourceComponent.outputStates[conn.second];

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
        const auto &comp = m_registry.get<DigitalComponent>(e);
        const auto def = ComponentCatalog::instance().getComponentDefinition(comp.type);
        BESS_SE_LOG_EVENT("Simulating {}", def->name);
        BESS_SE_LOG_EVENT("\tInputs:");
        for (auto &inp : inputs) {
            BESS_SE_LOG_EVENT("\t\t{}", (int)inp.state);
        }
        BESS_SE_LOG_EVENT("");
        if (def && def->simulationFunction) {
            return def->simulationFunction(
                m_registry, e, inputs, m_currentSimTime,
                std::bind(&SimulationEngine::getEntityWithUuid, this, std::placeholders::_1));
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

                        for (auto &pin : dc.outputPins) {
                            std::set<entt::entity> uniqueEntites{};
                            std::unordered_map<entt::entity, std::vector<PinState>> inps = {};
                            for (auto &c : pin) {
                                auto d = getEntityWithUuid(c.first);
                                if (d == entt::null || uniqueEntites.find(d) != uniqueEntites.end())
                                    continue;

                                uniqueEntites.insert(d);
                                inps[d] = getInputPinsState(d);
                                auto &dc_ = m_registry.get<DigitalComponent>(d);
                                dc_.inputStates = getInputPinsState(d);
                            }

                            for (auto &ent : uniqueEntites) {
                                auto &destDc = m_registry.get<DigitalComponent>(ent);
                                scheduleEvent(ent, entity, m_currentSimTime + dc.delay);
                            }
                        }
                    }

                    if (hasClk) {
                        auto &clk = m_registry.get<ClockComponent>(entity);
                        auto &dc = m_registry.get<DigitalComponent>(entity);
                        bool isHigh = (bool)dc.outputStates[0];
                        dc.outputStates[0] = isHigh ? PinState(LogicState::low, m_currentSimTime) : PinState(LogicState::high, m_currentSimTime);
                        clk.high = (bool)dc.outputStates[0];
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
        auto ent = getEntityWithUuid(uuid);
        if (!m_registry.all_of<DigitalComponent>(ent))
            return false;

        auto &digiComp = m_registry.get<DigitalComponent>(ent);
        digiComp.updateInputCount(n);
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
