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

    SimulationEngine &SimulationEngine::instance() {
        static SimulationEngine inst;
        return inst;
    }

    SimulationEngine::SimulationEngine() {
        initComponentCatalog();
        simThread = std::thread(&SimulationEngine::run, this);
    }

    SimulationEngine::~SimulationEngine() {
        stopFlag.store(true);
        cv.notify_all();
        if (simThread.joinable())
            simThread.join();
    }

    void SimulationEngine::scheduleEvent(entt::entity e, entt::entity schedulerEntity, SimDelayMilliSeconds simTime) {

        std::lock_guard lk(queueMutex);
        static uint64_t eventId = 0;
        SimulationEvent ev{simTime, e, schedulerEntity, eventId++};
        eventSet.insert(ev);
        cv.notify_all();
    }

    void SimulationEngine::clearEventsForEntity(entt::entity e) {
        std::lock_guard lk(queueMutex);
        for (auto it = eventSet.begin(); it != eventSet.end();) {
            if (it->entity == e) {
                it = eventSet.erase(it);
            } else {
                ++it;
            }
        }
    }

    entt::entity SimulationEngine::getEntityWithUuid(const UUID &uuid) const {
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
        uuidMap.emplace(idComp.uuid, ent);
        const auto *def = ComponentCatalog::instance().getComponentDefinition(type);
        inputCount = (inputCount < 0 ? def->inputCount : inputCount);
        outputCount = (outputCount < 0 ? def->outputCount : outputCount);
        if (type == ComponentType::NOT) {
            outputCount = inputCount;
        }
        auto &digi = m_registry.emplace<DigitalComponent>(ent, type, inputCount, outputCount, def->delay);
        if (type == ComponentType::FLIP_FLOP_JK || type == ComponentType::FLIP_FLOP_SR ||
            type == ComponentType::FLIP_FLOP_D || type == ComponentType::FLIP_FLOP_T) {
            m_registry.emplace<FlipFlopComponent>(ent, FlipFlopType(type), 1);
        }
        scheduleEvent(ent, entt::null, currentSimTime + def->delay);
        std::cout << "[SimEngine] Added component " << def->name << std::endl;
        return idComp.uuid;
    }

    bool SimulationEngine::connectComponent(const UUID &src, int srcPin, PinType srcType,
                                            const UUID &dst, int dstPin, PinType dstType) {
        auto srcEnt = getEntityWithUuid(src);
        auto dstEnt = getEntityWithUuid(dst);
        if (!m_registry.valid(srcEnt) || !m_registry.valid(dstEnt))
            return false;

        if (srcType == dstType) {
            std::cout << "Cannot connect pins of the same type i.e. input -> input or output -> output" << std::endl;
            return false;
        }

        auto &srcComp = m_registry.get<DigitalComponent>(srcEnt);
        auto &dstComp = m_registry.get<DigitalComponent>(dstEnt);
        auto &outPins = (srcType == PinType::output ? srcComp.outputPins : srcComp.inputPins);
        auto &inPins = (dstType == PinType::input ? dstComp.inputPins : dstComp.outputPins);

        if (srcPin < 0 || srcPin >= static_cast<int>(outPins.size())) {
            std::cout << "Invalid source pin index. Valid range: 0 to "
                      << (srcComp.outputPins.size() - 1) << std::endl;
            return false;
        }
        if (dstPin < 0 || dstPin >= static_cast<int>(inPins.size())) {
            std::cout << "Invalid destination pin index. Valid range: 0 to "
                      << (dstComp.inputPins.size() - 1) << std::endl;
            return false;
        }
        // Check for duplicate connection.
        for (const auto &conn : outPins[srcPin]) {
            if (conn.first == dst && conn.second == dstPin) {
                std::cout << "Connection already exists." << std::endl;
                return false;
            }
        }

        outPins[srcPin].emplace_back(dst, dstPin);
        inPins[dstPin].emplace_back(src, srcPin);
        scheduleEvent(dstEnt, srcEnt, currentSimTime + dstComp.delay);
        std::cout << "[SimEngine] Connected components" << std::endl;
        return true;
    }

    void SimulationEngine::deleteComponent(const UUID &uuid) {
        auto ent = getEntityWithUuid(uuid);
        if (ent == entt::null)
            return;

        clearEventsForEntity(ent);
        std::vector<entt::entity> affected;

        {
            std::lock_guard lk(registryMutex);
            auto view = m_registry.view<DigitalComponent>();
            for (auto other : view) {
                if (other == ent)
                    continue;
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
            uuidMap.erase(uuid);
            m_registry.destroy(ent);
        }

        for (auto e : affected) {
            if (m_registry.valid(e)) {
                auto &dc = m_registry.get<DigitalComponent>(e);
                scheduleEvent(e, entt::null, currentSimTime + dc.delay);
            }
        }

        std::cout << "[SimEngine] Deleted component" << std::endl;
    }

    bool SimulationEngine::getDigitalPinState(const UUID &uuid, PinType type, int idx) {
        auto ent = getEntityWithUuid(uuid);
        auto &comp = m_registry.get<DigitalComponent>(ent);
        return comp.outputStates[idx];
    }

    void SimulationEngine::setDigitalInput(const UUID &uuid, bool value) {
        auto ent = getEntityWithUuid(uuid);
        if (m_registry.all_of<ClockComponent>(ent))
            return;

        auto &comp = m_registry.get<DigitalComponent>(ent);

        assert(comp.type == ComponentType::INPUT);

        if (comp.outputStates[0] == value)
            return;
        comp.outputStates[0] = value;
        for (auto &conn : comp.outputPins[0]) {
            auto dest = getEntityWithUuid(conn.first);
            if (dest != entt::null) {
                auto &dc = m_registry.get<DigitalComponent>(dest);
                scheduleEvent(dest, ent, currentSimTime + dc.delay);
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
        scheduleEvent(ent, entt::null, currentSimTime);
        return hadClock != enable;
    }

    ComponentState SimulationEngine::getComponentState(const UUID &uuid) {
        auto ent = getEntityWithUuid(uuid);
        ComponentState st;
        assert(m_registry.all_of<DigitalComponent>(ent));
        auto &comp = m_registry.get<DigitalComponent>(ent);
        st.inputStates = getInputPinsState(ent);
        st.outputStates = comp.outputStates;
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

        std::lock_guard lk(registryMutex);
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
        scheduleEvent(toSchedule, entt::null, currentSimTime + dc.delay);
        std::cout << "[SimEngine] Deleted connection" << std::endl;
    }

    std::vector<bool> SimulationEngine::getInputPinsState(entt::entity ent) const {
        std::vector<bool> states;
        auto &comp = m_registry.get<DigitalComponent>(ent);
        for (auto &pin : comp.inputPins) {
            bool s = false;
            for (auto &conn : pin) {
                auto e = getEntityWithUuid(conn.first);
                if (e == entt::null)
                    continue;
                auto &dc = m_registry.get<DigitalComponent>(e);
                s = s || dc.outputStates[conn.second];
                if (s)
                    break;
            }
            states.push_back(s);
        }
        return states;
    }

    bool SimulationEngine::simulateComponent(entt::entity e, const std::vector<bool> &inputs) {
        auto &comp = m_registry.get<DigitalComponent>(e);
        const auto *def = ComponentCatalog::instance().getComponentDefinition(comp.type);
        if (def && def->simulationFunction) {
            return def->simulationFunction(
                m_registry, e, inputs,
                std::bind(&SimulationEngine::getEntityWithUuid, this, std::placeholders::_1));
        }
        return false;
    }

    void SimulationEngine::run() {
        std::cout << "[SimEngine] Simulation loop started" << std::endl;
        currentSimTime = std::chrono::milliseconds(0);
        while (!stopFlag.load()) {
            std::unique_lock lk(queueMutex);
            cv.wait(lk, [&] { return stopFlag.load() || !eventSet.empty(); });
            if (stopFlag.load())
                break;

            if (eventSet.empty())
                continue;

            currentSimTime = eventSet.begin()->simTime;

            std::set<SimulationEvent> eventsToSim = {};
            while (!eventSet.empty() && eventSet.begin()->simTime == currentSimTime) {
                auto ev = *eventSet.begin();
                if (!eventsToSim.empty() && eventsToSim.rbegin()->schedulerEntity != ev.schedulerEntity)
                    break;
                eventsToSim.insert(ev);
                eventSet.erase(eventSet.begin());
            }

            std::unordered_map<entt::entity, std::vector<bool>> inputsMap = {};

            for (auto &ev : eventsToSim) {
                inputsMap[ev.entity] = getInputPinsState(ev.entity);
            }

            for (auto &ev : eventsToSim) {
                lk.unlock();

                {
                    std::lock_guard regLock(registryMutex);
                    if (!m_registry.valid(ev.entity))
                        continue;

                    bool hasClk = m_registry.all_of<ClockComponent>(ev.entity);
                    auto inputs = inputsMap[ev.entity];
                    bool changed = simulateComponent(ev.entity, inputs);

                    if (changed || hasClk) {
                        auto &dc = m_registry.get<DigitalComponent>(ev.entity);

                        for (auto &pin : dc.outputPins) {
                            std::set<entt::entity> uniqueEntites{};
                            std::unordered_map<entt::entity, std::vector<bool>> inps = {};
                            for (auto &c : pin) {
                                auto d = getEntityWithUuid(c.first);
                                if (d == entt::null || uniqueEntites.find(d) != uniqueEntites.end())
                                    continue;

                                uniqueEntites.insert(d);
                                inps[d] = getInputPinsState(d);
                            }

                            for (auto &ent : uniqueEntites) {
                                auto &destDc = m_registry.get<DigitalComponent>(ent);
                                scheduleEvent(ent, ev.entity, currentSimTime + destDc.delay);
                            }
                        }
                    }

                    if (hasClk) {
                        auto &clk = m_registry.get<ClockComponent>(ev.entity);
                        auto &dc = m_registry.get<DigitalComponent>(ev.entity);
                        bool isHigh = dc.outputStates[0];
                        dc.outputStates[0] = !isHigh;
                        clk.high = dc.outputStates[0];
                        scheduleEvent(ev.entity, entt::null, currentSimTime + clk.getNextDelay());
                    }
                }

                lk.lock();
            }

            if (!eventSet.empty()) {
                cv.wait_until(lk, std::chrono::steady_clock::now() +
                                      (eventSet.begin()->simTime - currentSimTime));
            }
        }
    }

} // namespace Bess::SimEngine
