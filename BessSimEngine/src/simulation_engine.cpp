#include "simulation_engine.h"
#include "component_catalog.h"
#include "component_definition.h"
#include "component_types.h"
#include "entt/entity/fwd.hpp"
#include "entt_components.h"
#include "entt_registry_serializer.h"
#include "properties.h"
#include "simulation_engine_serializer.h"
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

    void initComponentCatalog() {
        ComponentCatalog::instance().registerComponent(ComponentDefinition(ComponentType::INPUT, "Input", "IO", 0, 1, [](entt::registry &, entt::entity) -> bool { return false; }, SimDelayMilliSeconds(100)));

        ComponentCatalog::instance().registerComponent({ComponentType::OUTPUT, "Output", "IO", 1, 0,
                                                        [](entt::registry &registry, entt::entity e) -> bool {
                                                            auto &gate = registry.get<GateComponent>(e);
                                                            bool newState = false;
                                                            if (!gate.inputPins.empty()) {
                                                                bool pinState = false;
                                                                for (const auto &conn : gate.inputPins[0]) {
                                                                    if (registry.valid(conn.first)) {
                                                                        auto &srcGate = registry.get<GateComponent>(conn.first);
                                                                        if (!srcGate.outputStates.empty()) {
                                                                            pinState = pinState || srcGate.outputStates[0];
                                                                        }
                                                                    }
                                                                }
                                                                newState = pinState;
                                                            }
                                                            bool changed = false;
                                                            for (auto state : gate.outputStates) {
                                                                if (state != newState) {
                                                                    state = newState;
                                                                    changed = true;
                                                                }
                                                            }
                                                            return changed;
                                                        },
                                                        SimDelayMilliSeconds(100)});

        ComponentDefinition andGate = {ComponentType::AND, "AND Gate", "Digital Gates", 2, 1,
                                       [](entt::registry &registry, entt::entity e) -> bool {
                                           auto &gate = registry.get<GateComponent>(e);
                                           std::vector<bool> pinValues;
                                           for (const auto &pin : gate.inputPins) {
                                               bool pinState = false;
                                               for (const auto &conn : pin) {
                                                   if (registry.valid(conn.first)) {
                                                       auto &srcGate = registry.get<GateComponent>(conn.first);
                                                       if (!srcGate.outputStates.empty()) {
                                                           pinState = pinState || srcGate.outputStates[conn.second];
                                                           if (pinState)
                                                               break;
                                                       }
                                                   }
                                               }
                                               pinValues.push_back(pinState);
                                           }
                                           bool newState = true;
                                           for (auto state : pinValues) {
                                               newState = newState && state;
                                               if (!newState)
                                                   break;
                                           }

                                           bool changed = false;
                                           for (auto state : gate.outputStates) {
                                               if (state != newState) {
                                                   state = newState;
                                                   changed = true;
                                               }
                                           }
                                           return changed;
                                       },
                                       SimDelayMilliSeconds(100)};

        andGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {3, 4, 5});

        ComponentCatalog::instance().registerComponent(andGate);

        ComponentCatalog::instance().registerComponent({ComponentType::OR, "OR Gate", "Digital Gates", 2, 1,
                                                        [](entt::registry &registry, entt::entity e) -> bool {
                                                            auto &gate = registry.get<GateComponent>(e);
                                                            std::vector<bool> pinValues;
                                                            for (const auto &pin : gate.inputPins) {
                                                                bool pinState = false;
                                                                for (const auto &conn : pin) {
                                                                    if (registry.valid(conn.first)) {
                                                                        auto &srcGate = registry.get<GateComponent>(conn.first);
                                                                        if (!srcGate.outputStates.empty()) {
                                                                            pinState = pinState || srcGate.outputStates[0];
                                                                        }
                                                                    }
                                                                }
                                                                pinValues.push_back(pinState);
                                                            }
                                                            bool newState = (pinValues.size() >= 2) ? (pinValues[0] || pinValues[1]) : false;
                                                            bool changed = false;
                                                            for (auto state : gate.outputStates) {
                                                                if (state != newState) {
                                                                    state = newState;
                                                                    changed = true;
                                                                }
                                                            }
                                                            return changed;
                                                        },
                                                        SimDelayMilliSeconds(100)});

        ComponentDefinition notGate = {ComponentType::NOT, "NOT Gate", "Digital Gates", 1, 1,
                                       [](entt::registry &registry, entt::entity e) -> bool {
                                           auto &gate = registry.get<GateComponent>(e);
                                           std::vector<bool> newStates = {};
                                           if (!gate.inputPins.empty()) {
                                               for (const auto &pin : gate.inputPins) {
                                                   bool pinState = false;
                                                   for (const auto &conn : pin) {
                                                       if (registry.valid(conn.first)) {
                                                           auto &srcGate = registry.get<GateComponent>(conn.first);
                                                           if (!srcGate.outputStates.empty()) {
                                                               pinState = pinState || srcGate.outputStates[0];
                                                           }
                                                       }
                                                   }
                                                   newStates.emplace_back(!pinState);
                                               }
                                           }
                                           bool changed = false;
                                           for (int i = 0; i < newStates.size(); i++) {
                                               if (gate.outputStates[i] != newStates[i]) {
                                                   changed = true;
                                                   break;
                                               }
                                           }
                                           if (changed)
                                               gate.outputStates = newStates;
                                           return changed;
                                       },
                                       SimDelayMilliSeconds(100)};
        notGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {2, 3, 4, 5, 6});
        ComponentCatalog::instance().registerComponent(notGate);

        ComponentCatalog::instance().registerComponent({ComponentType::XOR, "XOR Gate", "Digital Gates", 2, 1,
                                                        [](entt::registry &registry, entt::entity e) -> bool {
                                                            auto &gate = registry.get<GateComponent>(e);
                                                            std::vector<bool> pinValues;
                                                            for (const auto &pin : gate.inputPins) {
                                                                bool pinState = false;
                                                                for (const auto &conn : pin) {
                                                                    if (registry.valid(conn.first)) {
                                                                        auto &srcGate = registry.get<GateComponent>(conn.first);
                                                                        if (!srcGate.outputStates.empty()) {
                                                                            pinState = pinState || srcGate.outputStates[0];
                                                                        }
                                                                    }
                                                                }
                                                                pinValues.push_back(pinState);
                                                            }
                                                            bool newState = (pinValues.size() >= 2) ? (pinValues[0] ^ pinValues[1]) : false;
                                                            bool changed = false;
                                                            for (auto state : gate.outputStates) {
                                                                if (state != newState) {
                                                                    state = newState;
                                                                    changed = true;
                                                                }
                                                            }
                                                            return changed;
                                                        },
                                                        SimDelayMilliSeconds(100)});

        ComponentCatalog::instance().registerComponent(ComponentDefinition(ComponentType::XNOR, "XNOR Gate", "Digital Gates", 2, 1, [](entt::registry &registry, entt::entity e) -> bool {
                                                            auto &gate = registry.get<GateComponent>(e);
                                                            std::vector<bool> pinValues;
                                                            for (const auto &pin : gate.inputPins) {
                                                                bool pinState = false;
                                                                for (const auto &conn : pin) {
                                                                    if (registry.valid(conn.first)) {
                                                                        auto &srcGate = registry.get<GateComponent>(conn.first);
                                                                        if (!srcGate.outputStates.empty()) {
                                                                            pinState = pinState || srcGate.outputStates[0];
                                                                        }
                                                                    }
                                                                }
                                                                pinValues.push_back(pinState);
                                                            }
                                                            bool newState = (pinValues.size() >= 2) ? !(pinValues[0] ^ pinValues[1]) : false;
                                                            bool changed = false;
                                                            for (auto state : gate.outputStates) {
                                                                if (state != newState) {
                                                                    state = newState;
                                                                    changed = true;
                                                                }
                                                            }
                                                            return changed; }, SimDelayMilliSeconds(100)));

        auto nandGate = ComponentDefinition(ComponentType::NAND, "NAND Gate", "Digital Gates", 2, 1, [](entt::registry &registry, entt::entity e) -> bool {
                                           auto &gate = registry.get<GateComponent>(e);
                                           std::vector<bool> pinValues;
                                           for (const auto &pin : gate.inputPins) {
                                               bool pinState = false;
                                               for (const auto &conn : pin) {
                                                   if (registry.valid(conn.first)) {
                                                       auto &srcGate = registry.get<GateComponent>(conn.first);
                                                       if (!srcGate.outputStates.empty()) {
                                                           pinState = pinState || srcGate.outputStates[conn.second];
                                                           if (pinState)
                                                               break;
                                                       }
                                                   }
                                               }
                                               pinValues.push_back(pinState);
                                           }
                                           bool newState = true;
                                           for (auto state : pinValues) {
                                               newState = newState && state;
                                               if (!newState)
                                                   break;
                                           }
					   newState = !newState;

                                           bool changed = false;
                                           for (auto state : gate.outputStates) {
                                               if (state != newState) {
                                                   state = newState;
                                                   changed = true;
                                               }
                                           }
                                           return changed; }, SimDelayMilliSeconds(100));
        nandGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {3, 4, 5});
        ComponentCatalog::instance().registerComponent(nandGate);

        ComponentCatalog::instance().registerComponent(ComponentDefinition(ComponentType::NOR, "NOR Gate", "Digital Gates", 2, 1, [](entt::registry &registry, entt::entity e) -> bool {
                                                            auto &gate = registry.get<GateComponent>(e);
                                                            std::vector<bool> pinValues;
                                                            for (const auto &pin : gate.inputPins) {
                                                                bool pinState = false;
                                                                for (const auto &conn : pin) {
                                                                    if (registry.valid(conn.first)) {
                                                                        auto &srcGate = registry.get<GateComponent>(conn.first);
                                                                        if (!srcGate.outputStates.empty()) {
                                                                            pinState = pinState || srcGate.outputStates[0];
                                                                        }
                                                                    }
                                                                }
                                                                pinValues.push_back(pinState);
                                                            }
                                                            bool newState = (pinValues.size() >= 2) ? !(pinValues[0] || pinValues[1]) : false;
                                                            bool changed = false;
                                                            for (auto state : gate.outputStates) {
                                                                if (state != newState) {
                                                                    state = newState;
                                                                    changed = true;
                                                                }
                                                            }
                                                            return changed; }, SimDelayMilliSeconds(100)));
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

    void SimulationEngine::deleteConnection(entt::entity componentA, PinType pinAType, int idxA, entt::entity componentB, PinType pinBType, int idxB) {
        auto &compA = m_registry.get<GateComponent>(componentA);
        auto &compB = m_registry.get<GateComponent>(componentB);

        auto &pinA = (pinAType == PinType::input) ? compA.inputPins[idxA] : compA.outputPins[idxA];
        auto &pinB = (pinBType == PinType::input) ? compB.inputPins[idxB] : compB.outputPins[idxB];
        pinA.erase(
            std::remove_if(pinA.begin(), pinA.end(),
                           [componentB, idxB](const std::pair<entt::entity, int> &conn) {
                               return conn.first == componentB && conn.second == idxB;
                           }),
            pinA.end());

        pinB.erase(
            std::remove_if(pinB.begin(), pinB.end(),
                           [componentA, idxA](const std::pair<entt::entity, int> &conn) {
                               return conn.first == componentA && conn.second == idxA;
                           }),
            pinB.end());

        if (pinAType == PinType::output) {
            scheduleEvent(componentB, std::chrono::steady_clock::now() + compB.delay);
        } else {
            scheduleEvent(componentA, std::chrono::steady_clock::now() + compA.delay);
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

    entt::entity SimulationEngine::addComponent(ComponentType type, int inputCount, int outputCount) {
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

        m_registry.emplace<IdComponent>(ent);
        m_registry.emplace<GateComponent>(ent, type, inputCount, outputCount, def->delay);
        std::cout << "[SimEngine] Added component " << (uint64_t)ent << std::endl;
        scheduleEvent(ent, std::chrono::steady_clock::now() + def->delay);
        return ent;
    }

    bool SimulationEngine::connectComponent(entt::entity src, int srcPin, PinType srcPinType, entt::entity dst, int dstPin, PinType dstPinType) {
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
            if (conn.first == dst && conn.second == dstPin) {
                std::cout << "Connection already exists." << std::endl;
                return false;
            }
        }
        srcPins[srcPin].emplace_back(dst, dstPin);
        dstPins[dstPin].emplace_back(src, srcPin);

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

    void SimulationEngine::deleteComponent(entt::entity component) {
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
                                       [component](const std::pair<entt::entity, int> &conn) {
                                           return conn.first == component;
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
                                       [component](const std::pair<entt::entity, int> &conn) {
                                           return conn.first == component;
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

    bool SimulationEngine::getDigitalPinState(entt::entity entity, PinType type, int idx) {
        assert(m_registry.valid(entity));
        auto &gateComp = m_registry.get<GateComponent>(entity);
        assert(gateComp.type == ComponentType::INPUT);
        return gateComp.outputStates[0];
    }

    void SimulationEngine::setDigitalInput(entt::entity entity, bool value) {
        assert(m_registry.valid(entity));
        auto &gateComp = m_registry.get<GateComponent>(entity);
        assert(gateComp.type == ComponentType::INPUT);

        if (gateComp.outputStates[0] == value)
            return;

        gateComp.outputStates[0] = value;
        for (auto &conn : gateComp.outputPins[0]) {
            auto &connComp = m_registry.get<GateComponent>(conn.first);
            scheduleEvent(conn.first, std::chrono::steady_clock::now() + std::chrono::milliseconds(connComp.delay));
        }
    }

    ComponentState SimulationEngine::getComponentState(entt::entity entity) {
        assert(m_registry.valid(entity));
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
                auto &connComp = m_registry.get<GateComponent>(conn.first);
                if (!connComp.outputStates[conn.second])
                    continue;
                state = true;
                break;
            }
            states.emplace_back(state);
        }
        return states;
    }

    ComponentType SimulationEngine::getComponentType(entt::entity entity) {
        assert(m_registry.valid(entity));
        auto &gateComp = m_registry.get<GateComponent>(entity);
        return gateComp.type;
    }

    bool SimulationEngine::simulateComponent(entt::entity e) {
        auto &comp = m_registry.get<GateComponent>(e);
        const auto *def = ComponentCatalog::instance().getComponentDefinition(comp.type);
        std::cout << "[SimEngine] Simulating " << def->name << std::endl;
        if (def && def->simulationFunction) {
            bool val = def->simulationFunction(m_registry, e);
            return val;
        }
        return false;
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
                    bool changed = simulateComponent(nextEvent.entity);
                    // If state changed, schedule simulation events for all connected gates.
                    if (changed) {
                        auto &comp = m_registry.get<GateComponent>(nextEvent.entity);
                        for (const auto &pin : comp.outputPins) {
                            for (const auto &conn : pin) {
                                if (m_registry.valid(conn.first)) {
                                    auto &dstGate = m_registry.get<GateComponent>(conn.first);
                                    auto scheduledTime = std::chrono::steady_clock::now() + dstGate.delay;
                                    scheduleEvent(conn.first, scheduledTime);
                                }
                            }
                        }
                    }
                }
            } else {
                cv.wait_until(lock, nextEvent.time);
            }
        }
    }
} // namespace Bess::SimEngine
