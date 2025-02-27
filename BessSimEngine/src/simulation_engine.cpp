#include "simulation_engine.h"
#include "component_catalog.h"
#include "entt/entity/fwd.hpp"
#include "gate.h"
#include <chrono>
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
        ComponentCatalog::instance().registerComponent({ComponentType::INPUT,
                                                        "Input",
                                                        "IO", // Category for input/output components.
                                                        0,
                                                        1,
                                                        [](entt::registry &, entt::entity) -> bool {
                                                            // INPUT component: output is controlled externally.
                                                            // No simulation update is performed.
                                                            return false;
                                                        }});

        // Register OUTPUT component (under "IO").
        ComponentCatalog::instance().registerComponent({ComponentType::OUTPUT,
                                                        "Output",
                                                        "IO", // Category for input/output components.
                                                        1,
                                                        0, // Typically, an output element might not drive any output pins.
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
                                                            // Even though output count is zero, if there were outputs, we would update them.
                                                            for (auto state : gate.outputStates) {
                                                                if (state != newState) {
                                                                    state = newState;
                                                                    changed = true;
                                                                }
                                                            }
                                                            return changed;
                                                        }});

        // Register AND gate (under "Gates").
        ComponentCatalog::instance().registerComponent({ComponentType::AND,
                                                        "AND Gate",
                                                        "Gates", // Category for standard logic gates.
                                                        2,
                                                        1,
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
                                                            bool newState = (pinValues.size() >= 2) ? (pinValues[0] && pinValues[1]) : false;
                                                            bool changed = false;
                                                            for (auto state : gate.outputStates) {
                                                                if (state != newState) {
                                                                    state = newState;
                                                                    changed = true;
                                                                }
                                                            }
                                                            return changed;
                                                        }});

        // Register OR gate (under "Gates").
        ComponentCatalog::instance().registerComponent({ComponentType::OR,
                                                        "OR Gate",
                                                        "Gates", // Category for standard logic gates.
                                                        2,
                                                        1,
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
                                                        }});

        // Register NOT gate (under "Gates").
        ComponentCatalog::instance().registerComponent({ComponentType::NOT,
                                                        "NOT Gate",
                                                        "Gates", // Category for standard logic gates.
                                                        1,
                                                        1,
                                                        [](entt::registry &registry, entt::entity e) -> bool {
                                                            auto &gate = registry.get<GateComponent>(e);
                                                            bool newState = true;
                                                            if (!gate.inputPins.empty()) {
                                                                bool pinState = false;
                                                                // For NOT, we only use the first input pin.
                                                                for (const auto &conn : gate.inputPins[0]) {
                                                                    if (registry.valid(conn.first)) {
                                                                        auto &srcGate = registry.get<GateComponent>(conn.first);
                                                                        if (!srcGate.outputStates.empty()) {
                                                                            pinState = pinState || srcGate.outputStates[0];
                                                                        }
                                                                    }
                                                                }
                                                                newState = !pinState;
                                                            }
                                                            bool changed = false;
                                                            if (gate.outputStates[0] != newState) {
                                                                gate.outputStates[0] = newState;
                                                                changed = true;
                                                            }
                                                            return changed;
                                                        }});
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

    entt::entity SimulationEngine::addComponent(ComponentType type) {
        auto ent = registry.create();
        auto &gateComp = registry.emplace<GateComponent>(ent);
        const auto *def = ComponentCatalog::instance().getComponent(type);
        gateComp.type = type;
        gateComp.inputPins.resize(def->inputCount);
        gateComp.outputPins.resize(def->outputCount);
        gateComp.outputStates.resize(def->outputCount, false);
        gateComp.delay = std::chrono::milliseconds(100);
        std::cout << "[+] Added component " << (uint64_t)ent << std::endl;
        scheduleEvent(ent, std::chrono::steady_clock::now() + gateComp.delay);

        return ent;
    }

    bool SimulationEngine::connectComponent(entt::entity src, int srcPin, PinType srcPinType, entt::entity dst, int dstPin, PinType dstPinType) {
        if (!registry.valid(src) || !registry.valid(dst)) {
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

        auto &srcGate = registry.get<GateComponent>(src);
        auto &dstGate = registry.get<GateComponent>(dst);

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
            auto view = registry.view<GateComponent>();
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
            if (registry.valid(component)) {
                registry.destroy(component);
            }
        }

        // For each gate that lost an input connection, schedule a simulation event.
        for (auto entity : affectedGates) {
            // Lock registryMutex briefly to retrieve the delay value.
            std::lock_guard<std::mutex> lock(registryMutex);
            if (registry.valid(entity)) {
                auto &comp = registry.get<GateComponent>(entity);
                scheduleEvent(entity, std::chrono::steady_clock::now() + comp.delay);
            }
        }
        std::cout << "[+] Gate " << static_cast<uint32_t>(component) << " deleted successfully." << std::endl;
    }

    ComponentState SimulationEngine::getComponentState(entt::entity entity) {
        assert(registry.valid(entity));
        ComponentState state;
        state.inputStates = getInputPinsState(entity);
        auto &gateComp = registry.get<GateComponent>(entity);
        state.outputStates = gateComp.outputStates;
        return state;
    }

    std::vector<bool> SimulationEngine::getInputPinsState(entt::entity entity) {
        std::vector<bool> states;
        auto &gateComp = registry.get<GateComponent>(entity);
        for (auto &pin : gateComp.inputPins) {
            bool state = false;
            for (auto &conn : pin) {
                auto &connComp = registry.get<GateComponent>(conn.first);
                if (!connComp.outputStates[conn.second])
                    continue;
                state = true;
                break;
            }
            states.emplace_back(state);
        }
        return states;
    }

    bool SimulationEngine::simulateComponent(entt::entity e) {
        auto &comp = registry.get<GateComponent>(e);
        const auto *def = ComponentCatalog::instance().getComponent(comp.type);
        std::cout << "[+] Simulating " << def->name << std::endl;
        if (def && def->simulationFunction) {
            bool val = def->simulationFunction(registry, e);
            return val;
        }
        return false;
    }

    void SimulationEngine::run() {
        std::cout << "[+] Simulation Loop Started" << std::endl;
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
                    if (!registry.valid(nextEvent.entity))
                        continue;
                    bool changed = simulateComponent(nextEvent.entity);
                    // If state changed, schedule simulation events for all connected gates.
                    if (changed) {
                        auto &comp = registry.get<GateComponent>(nextEvent.entity);
                        for (const auto &pin : comp.outputPins) {
                            for (const auto &conn : pin) {
                                if (registry.valid(conn.first)) {
                                    auto &dstGate = registry.get<GateComponent>(conn.first);
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
