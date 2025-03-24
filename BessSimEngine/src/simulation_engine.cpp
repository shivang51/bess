#include "simulation_engine.h"
#include "component_catalog.h"
#include "component_definition.h"
#include "component_types.h"
#include "entt/entity/fwd.hpp"
#include "entt_components.h"
#include "init_components.h"
#include "types.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>

namespace Bess::SimEngine {

    // Global mutex to protect registry access.
    static std::mutex registryMutex;

    bool SimulationEvent::operator<(const SimulationEvent &other) const {
        // Earlier times get higher priority.
        return time > other.time;
    }

    SimulationEngine::SimulationEngine()
        : stopFlag(false) {
        simThread = std::thread(&SimulationEngine::run, this);
        initComponentCatalog();
    }

    SimulationEngine::~SimulationEngine() {
        stop();
        if (simThread.joinable()) {
            simThread.join();
        }
    }

    SimulationEngine &SimulationEngine::instance() {
        static SimulationEngine instance;
        return instance;
    }

    void SimulationEngine::deleteConnection(const UUID &componentA, PinType pinAType, int idxA, const UUID &componentB, PinType pinBType, int idxB) {
        auto componentAEntt = getEntityWithUuid(componentA);
        auto componentBEntt = getEntityWithUuid(componentB);
        auto &compA = m_registry.get<GateComponent>(componentAEntt);
        auto &compB = m_registry.get<GateComponent>(componentBEntt);
        auto &pinA = (pinAType == PinType::input) ? compA.inputPins[idxA] : compA.outputPins[idxA];
        auto &pinB = (pinBType == PinType::input) ? compB.inputPins[idxB] : compB.outputPins[idxB];
        pinA.erase(
            std::remove_if(pinA.begin(), pinA.end(),
                           [componentB, idxB](const std::pair<UUID, int> &conn) {
                               return conn.first == componentB && conn.second == idxB;
                           }),
            pinA.end());

        pinB.erase(
            std::remove_if(pinB.begin(), pinB.end(),
                           [componentA, idxA](const std::pair<UUID, int> &conn) {
                               return conn.first == componentA && conn.second == idxA;
                           }),
            pinB.end());

        if (pinAType == PinType::output) {
            scheduleEvent(componentBEntt, std::chrono::steady_clock::now() + compB.delay);
        } else {
            scheduleEvent(componentAEntt, std::chrono::steady_clock::now() + compA.delay);
        }

        std::cout << "[SimEngine] Deleted connection" << std::endl;
    }

    void SimulationEngine::stop() {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stopFlag = true;
        }
        cv.notify_all();
    }

    void SimulationEngine::scheduleEvent(entt::entity entity, std::chrono::steady_clock::time_point time) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            eventQueue.push({time, entity});
        }
        cv.notify_all();
    }

    const UUID &SimulationEngine::addComponent(ComponentType type, int inputCount, int outputCount) {
        auto ent = m_registry.create();
        const auto *def = ComponentCatalog::instance().getComponentDefinition(type);
        inputCount = inputCount == -1 ? def->inputCount : inputCount;
        outputCount = outputCount == -1 ? def->outputCount : outputCount;
        assert(def->inputCount <= inputCount);
        assert(def->outputCount <= outputCount);

        if (type == ComponentType::NOT) {
            inputCount = std::max(inputCount, outputCount);
            outputCount = inputCount;
        }

        auto &idComp = m_registry.emplace<IdComponent>(ent);
        m_registry.emplace<GateComponent>(ent, type, inputCount, outputCount, def->delay);
        std::cout << "[SimEngine] Added component " << (uint64_t)ent << std::endl;
        scheduleEvent(ent, std::chrono::steady_clock::now() + def->delay);
        return idComp.uuid;
    }

    bool SimulationEngine::connectComponent(const UUID &srcUuid, int srcPin, PinType srcPinType, const UUID &dstUuid, int dstPin, PinType dstPinType) {
        auto src = getEntityWithUuid(srcUuid);
        auto dst = getEntityWithUuid(dstUuid);
        if (!m_registry.valid(src) || !m_registry.valid(dst)) {
            std::cout << "Invalid gate id(s)." << std::endl;
            return false;
        }

        if (src == dst) {
            std::cout << "Cannot connect pins of the same gate." << std::endl;
            return false;
        }

        if (srcPinType == dstPinType) {
            std::cout << "Cannot connect pins of the same type i.e. input -> input or output -> output" << std::endl;
            return false;
        }

        auto &srcGate = m_registry.get<GateComponent>(src);
        auto &dstGate = m_registry.get<GateComponent>(dst);

        auto &srcPins = srcPinType == PinType::input ? srcGate.inputPins : srcGate.outputPins;
        auto &dstPins = dstPinType == PinType::input ? dstGate.inputPins : dstGate.outputPins;

        if (srcPin < 0 || srcPin >= static_cast<int>(srcPins.size())) {
            std::cout << "Invalid source pin index. Valid range: 0 to "
                      << (srcGate.outputPins.size() - 1) << std::endl;
            return false;
        }
        if (dstPin < 0 || dstPin >= static_cast<int>(dstPins.size())) {
            std::cout << "Invalid destination pin index. Valid range: 0 to "
                      << (dstGate.inputPins.size() - 1) << std::endl;
            return false;
        }
        // Check for duplicate connection.
        for (const auto &conn : srcPins[srcPin]) {
            if (conn.first == dstUuid && conn.second == dstPin) {
                std::cout << "Connection already exists." << std::endl;
                return false;
            }
        }
        srcPins[srcPin].emplace_back(dstUuid, dstPin);
        dstPins[dstPin].emplace_back(srcUuid, srcPin);

        auto srcPinTypeName = (srcPinType == PinType::input ? "input" : "output");
        auto dstPinTypeName = (dstPinType == PinType::input ? "input" : "output");

        std::cout << "Connected gate " << static_cast<uint32_t>(src)
                  << " (" << srcPinTypeName << " " << srcPin << ") to gate "
                  << static_cast<uint32_t>(dst) << " (" << dstPinTypeName << " " << dstPin << ")." << std::endl;

        auto gateToSim = (srcPinType == PinType::input) ? srcGate : dstGate;
        auto enttToSim = (srcPinType == PinType::input) ? src : dst;

        scheduleEvent(enttToSim, std::chrono::steady_clock::now() + gateToSim.delay);

        return true;
    }

    void SimulationEngine::deleteComponent(const UUID &componentUuid) {
        auto component = getEntityWithUuid(componentUuid);

        // Remove any scheduled events for the gate.
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            std::vector<SimulationEvent> remaining;
            while (!eventQueue.empty()) {
                SimulationEvent ev = eventQueue.top();
                eventQueue.pop();
                if (ev.entity != component) {
                    remaining.push_back(ev);
                }
            }
            for (const auto &ev : remaining) {
                eventQueue.push(ev);
            }
        }

        // gates which needs to be simulated after the deletion
        std::unordered_set<entt::entity> affectedGates;

        // Remove all connections to/from the gate in other components.
        {
            std::lock_guard<std::mutex> lock(registryMutex);
            // Iterate over all gate components.
            auto view = m_registry.view<GateComponent>();
            for (auto other : view) {
                if (other == component)
                    continue;
                auto &comp = view.get<GateComponent>(other);
                // Remove any connection in inputPins where the source is the gate to be deleted.
                for (auto &inPin : comp.inputPins) {
                    int n = inPin.size();
                    inPin.erase(
                        std::remove_if(inPin.begin(), inPin.end(),
                                       [componentUuid](const std::pair<UUID, int> &conn) {
                                           return conn.first == componentUuid;
                                       }),
                        inPin.end());
                    if (inPin.size() < n) {
                        affectedGates.insert(other);
                    }
                }
                // Remove any connection in outputPins where the destination is the gate to be deleted.
                for (auto &outPin : comp.outputPins) {
                    outPin.erase(
                        std::remove_if(outPin.begin(), outPin.end(),
                                       [componentUuid](const std::pair<UUID, int> &conn) {
                                           return conn.first == componentUuid;
                                       }),
                        outPin.end());
                }
            }
            // Finally, destroy the gate entity.
            if (m_registry.valid(component)) {
                m_registry.destroy(component);
            }
        }

        // For each gate that lost an input connection, schedule a simulation event.
        for (auto entity : affectedGates) {
            // Lock registryMutex briefly to retrieve the delay value.
            std::lock_guard<std::mutex> lock(registryMutex);
            if (m_registry.valid(entity)) {
                auto &comp = m_registry.get<GateComponent>(entity);
                scheduleEvent(entity, std::chrono::steady_clock::now() + comp.delay);
            }
        }
        std::cout << "[SimEngine] Gate " << static_cast<uint32_t>(component) << " deleted successfully." << std::endl;
    }

    bool SimulationEngine::getDigitalPinState(const UUID &entityUuid, PinType type, int idx) {
        auto entity = getEntityWithUuid(entityUuid);
        auto &gateComp = m_registry.get<GateComponent>(entity);
        assert(gateComp.type == ComponentType::INPUT);
        return gateComp.outputStates[0];
    }

    void SimulationEngine::setDigitalInput(const UUID &entityUuid, bool value) {
        auto entity = getEntityWithUuid(entityUuid);
        bool isClocked = m_registry.all_of<ClockComponent>(entity);
        if (isClocked)
            return;
        auto &gateComp = m_registry.get<GateComponent>(entity);
        assert(gateComp.type == ComponentType::INPUT);

        if (gateComp.outputStates[0] == value)
            return;

        gateComp.outputStates[0] = value;
        for (auto &conn : gateComp.outputPins[0]) {
            auto e = getEntityWithUuid(conn.first);
            auto &connComp = m_registry.get<GateComponent>(e);
            scheduleEvent(e, std::chrono::steady_clock::now() + std::chrono::milliseconds(connComp.delay));
        }
    }

    ComponentState SimulationEngine::getComponentState(const UUID &entityUuid) {
        auto entity = getEntityWithUuid(entityUuid);
        ComponentState state;
        state.inputStates = getInputPinsState(entity);
        auto &gateComp = m_registry.get<GateComponent>(entity);
        state.outputStates = gateComp.outputStates;
        return state;
    }

    std::vector<bool> SimulationEngine::getInputPinsState(entt::entity entity) {
        std::vector<bool> states;
        auto &gateComp = m_registry.get<GateComponent>(entity);
        for (auto &pin : gateComp.inputPins) {
            bool state = false;
            for (auto &conn : pin) {
                auto &connComp = m_registry.get<GateComponent>(getEntityWithUuid(conn.first));
                if (!connComp.outputStates[conn.second])
                    continue;
                state = true;
                break;
            }
            states.emplace_back(state);
        }
        return states;
    }

    bool SimulationEngine::updateClock(const UUID &uuid, bool shouldClock, float frequency, FrequencyUnit unit) {
        auto entt = getEntityWithUuid(uuid);
        assert(m_registry.valid(entt));
        bool hasClockComp = m_registry.all_of<ClockComponent>(entt);
        bool changed = false;
        if (shouldClock) {
            std::cout << "[SimEngine] Adding or Updating Clock" << std::endl;
            ClockComponent &clockComp = m_registry.get_or_emplace<ClockComponent>(entt);
            changed = !hasClockComp || frequency != clockComp.frequency || clockComp.frequencyUnit != unit;
            clockComp.frequency = frequency;
            clockComp.frequencyUnit = unit;
        } else if (hasClockComp) {
            std::cout << "[SimEngine] Removing Clock" << std::endl;
            m_registry.remove<ClockComponent>(entt);
            changed = true;
        }

        if (changed) {
            clearEventsForEntity(entt);
            scheduleEvent(entt, std::chrono::steady_clock::now());
        }
        return changed;
    }

    ComponentType SimulationEngine::getComponentType(const UUID &entityUuid) {
        auto entity = getEntityWithUuid(entityUuid);
        assert(m_registry.valid(entity));
        auto &gateComp = m_registry.get<GateComponent>(entity);
        return gateComp.type;
    }

    bool SimulationEngine::simulateComponent(entt::entity e) {
        auto &comp = m_registry.get<GateComponent>(e);
        const auto *def = ComponentCatalog::instance().getComponentDefinition(comp.type);
        std::cout << "[SimEngine] Simulating " << def->name << std::endl;
        if (def && def->simulationFunction) {
            bool val = def->simulationFunction(m_registry, e, std::bind(&SimulationEngine::getEntityWithUuid, this, std::placeholders::_1));
            return val;
        }
        return false;
    }

    void SimulationEngine::clearEventsForEntity(entt::entity entity) {
        std::lock_guard<std::mutex> lock(queueMutex);

        std::vector<SimulationEvent> remainingEvents;

        while (!eventQueue.empty()) {
            const SimulationEvent ev = eventQueue.top();
            eventQueue.pop();
            if (ev.entity != entity) {
                remainingEvents.push_back(ev);
            }
        }

        for (const auto &ev : remainingEvents) {
            eventQueue.push(ev);
        }

        std::cout << "[SimEngine] Removed all events for entity "
                  << static_cast<uint32_t>(entity) << std::endl;
    }

    void SimulationEngine::run() {
        std::cout << "[SimEngine] Simulation Loop Started" << std::endl;
        while (true) {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (stopFlag)
                break;

            if (eventQueue.empty()) {
                cv.wait(lock, [this]() { return stopFlag || !eventQueue.empty(); });
                if (stopFlag)
                    break;
            }

            auto now = std::chrono::steady_clock::now();
            SimulationEvent nextEvent = eventQueue.top();
            if (now >= nextEvent.time) {
                eventQueue.pop();
                lock.unlock();

                // Protect registry access.
                {
                    std::lock_guard<std::mutex> regLock(registryMutex);
                    if (!m_registry.valid(nextEvent.entity))
                        continue;
                    bool hasClockComp = m_registry.all_of<ClockComponent>(nextEvent.entity);
                    bool changed = simulateComponent(nextEvent.entity);
                    // If state changed, schedule simulation events for all connected gates.
                    if (changed) {
                        auto &comp = m_registry.get<GateComponent>(nextEvent.entity);
                        for (const auto &pin : comp.outputPins) {
                            for (const auto &conn : pin) {
                                auto e = getEntityWithUuid(conn.first);
                                if (m_registry.valid(e)) {
                                    auto &dstGate = m_registry.get<GateComponent>(e);
                                    auto scheduledTime = std::chrono::steady_clock::now() + dstGate.delay;
                                    scheduleEvent(e, scheduledTime);
                                }
                            }
                        }
                    }

                    if (hasClockComp) {
                        auto &comp = m_registry.get<ClockComponent>(nextEvent.entity);
                        auto time = comp.getTimeInMS();

                        auto &gateComp = m_registry.get<GateComponent>(nextEvent.entity);
                        assert(gateComp.type == ComponentType::INPUT);

                        bool isHigh = gateComp.outputStates[0];
                        gateComp.outputStates[0] = !isHigh;

                        float dutyTime = comp.dutyCycle;

                        if (!isHigh)
                            dutyTime = 1.f - dutyTime;

                        auto changeTime = (int)(time.count() * dutyTime);
                        time = SimDelayMilliSeconds(changeTime);

                        auto scheduledTime = std::chrono::steady_clock::now() + time;
                        scheduleEvent(nextEvent.entity, scheduledTime);
                    }

                    /*std::cout << "[SimEngine] Sim Queue Size " << eventQueue.size() << std::endl;*/
                }
            } else {
                cv.wait_until(lock, nextEvent.time);
            }
        }
    }

    entt::entity SimulationEngine::getEntityWithUuid(const UUID &uuid) {
        auto view = m_registry.view<IdComponent>();
        for (const auto &ent : view) {
            auto &idComp = view.get<IdComponent>(ent);
            if (idComp.uuid == uuid)
                return ent;
        }
        return entt::null;
    }

    const UUID &SimulationEngine::getUuidOfEntity(entt::entity ent) {
        auto &idComp = m_registry.get<IdComponent>(ent);
        return idComp.uuid;
    }
} // namespace Bess::SimEngine
