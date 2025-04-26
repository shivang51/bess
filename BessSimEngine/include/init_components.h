#pragma once

#include "component_catalog.h"
#include "entt_components.h"
#include "types.h"
#include <iostream>

namespace Bess::SimEngine {

    static std::vector<bool> getInputPinStates(entt::registry &reg, const DigitalComponent &gateComp, entt::entity callerEnt, auto fn) {
        std::vector<bool> inPinValues = gateComp.inputStates;
        for (int i = 0; i < gateComp.inputPins.size(); i++) {
            auto pin = gateComp.inputPins[i];
            bool pinValue = false;
            bool update = false;
            for (const auto &conn : pin) {
                auto e = fn(conn.first);
                if (reg.valid(e)) {
                    update = update || !reg.valid(callerEnt) || e == callerEnt;
                    auto &srcGate = reg.get<DigitalComponent>(e);
                    if (!srcGate.outputStates.empty()) {
                        pinValue = pinValue || srcGate.outputStates[conn.second];
                        if (pinValue)
                            break;
                    }
                }
            }
            if (update) {
                inPinValues[i] = pinValue;
            }
        }
        return inPinValues;
    }

    inline void initFlipFlops() {
        auto simFunc = [&](entt::registry &reg, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
            assert(reg.all_of<FlipFlopComponent>(e));
            auto &flipFlopComp = reg.get<FlipFlopComponent>(e);
            auto &comp = reg.get<DigitalComponent>(e);

            // getting values of input pins
            comp.inputStates = inputs;
            auto inPinValues = comp.inputStates;
            auto clockValue = inPinValues[flipFlopComp.clockPinIdx];
            if (!clockValue || flipFlopComp.prevClock) {
                flipFlopComp.prevClock = clockValue;
                return false;
            }

            flipFlopComp.prevClock = clockValue;

            bool q = comp.outputStates[0], q0 = comp.outputStates[1];
            bool changed = false;
            if (flipFlopComp.type == FlipFlopType::FLIP_FLOP_JK) {
                assert(comp.inputPins.size() == 3);
                if (inPinValues[0] && inPinValues[2]) {
                    q = !q;
                } else if (inPinValues[0]) {
                    q = true;
                } else if (inPinValues[2]) {
                    q = false;
                }
                q0 = !q;
                changed = comp.outputStates[0] != q;
                comp.outputStates = {q, q0};
            } else if (flipFlopComp.type == FlipFlopType::FLIP_FLOP_SR) {
                assert(comp.inputPins.size() == 3);
                if (inPinValues[0]) {
                    q = true;
                } else if (inPinValues[2]) {
                    q = false;
                }
                q0 = !q;
                changed = comp.outputStates[0] != q;
                comp.outputStates = {q, q0};
            } else if (flipFlopComp.type == FlipFlopType::FLIP_FLOP_D) {
                assert(comp.inputPins.size() == 2);
                q = inPinValues[0];
                q0 = !q;
                changed = comp.outputStates[0] != q;
                comp.outputStates = {q, q0};
            } else if (flipFlopComp.type == FlipFlopType::FLIP_FLOP_T) {
                assert(comp.inputPins.size() == 2);
                if (inPinValues[0])
                    q = !q;
                q0 = !q;
                changed = comp.outputStates[0] != q;
                comp.outputStates = {q, q0};
            }

            return changed;
        };

        auto &catalog = ComponentCatalog::instance();
        auto flipFlop = ComponentDefinition(ComponentType::FLIP_FLOP_JK, "JK Flip Flop", "Flip Flop", 3, 2, simFunc, SimDelayMilliSeconds(100));
        catalog.registerComponent(flipFlop);

        flipFlop.name = "SR Flip Flop";
        flipFlop.type = ComponentType::FLIP_FLOP_SR;
        catalog.registerComponent(flipFlop);

        flipFlop.name = "T Flip Flop";
        flipFlop.inputCount = 2;
        flipFlop.type = ComponentType::FLIP_FLOP_T;
        catalog.registerComponent(flipFlop);

        flipFlop.name = "D Flip Flop";
        flipFlop.inputCount = 2;
        flipFlop.type = ComponentType::FLIP_FLOP_D;
        catalog.registerComponent(flipFlop);
    }

    inline void initIO() {
        ComponentCatalog::instance().registerComponent(ComponentDefinition(ComponentType::INPUT, "Input", "IO", 0, 1, [](auto &, auto, auto, auto fn) -> bool { return false; }, SimDelayMilliSeconds(0)));

        ComponentCatalog::instance().registerComponent({ComponentType::OUTPUT, "Output", "IO", 1, 0,
                                                        [&](entt::registry &registry, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
                                                            auto &gate = registry.get<DigitalComponent>(e);
                                                            gate.inputStates = inputs;
                                                            return false;
                                                        },
                                                        SimDelayMilliSeconds(0)});
    }

    inline void initDigitalGates() {
        ComponentDefinition andGate = {ComponentType::AND, "AND Gate", "Digital Gates", 2, 1,
                                       [&](entt::registry &registry, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
                                           auto &gate = registry.get<DigitalComponent>(e);
                                           gate.inputStates = inputs;
                                           bool newState = true;
                                           for (auto state : gate.inputStates) {
                                               newState = newState && state;
                                               if (!newState)
                                                   break;
                                           }

                                           bool changed = gate.outputStates[0] != newState;
                                           gate.outputStates[0] = newState;
                                           return changed;
                                       },
                                       SimDelayMilliSeconds(100)};

        andGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {3, 4, 5});

        ComponentCatalog::instance().registerComponent(andGate);

        ComponentCatalog::instance().registerComponent({ComponentType::OR, "OR Gate", "Digital Gates", 2, 1,
                                                        [&](entt::registry &registry, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
                                                            auto &gate = registry.get<DigitalComponent>(e);
                                                            gate.inputStates = inputs;
                                                            bool newState = false;
                                                            for (auto state : gate.inputStates) {
                                                                newState = newState || state;
                                                                if (newState)
                                                                    break;
                                                            }
                                                            bool changed = gate.outputStates[0] != newState;
                                                            gate.outputStates[0] = newState;
                                                            return changed;
                                                        },
                                                        SimDelayMilliSeconds(100)});

        ComponentDefinition notGate = {ComponentType::NOT, "NOT Gate", "Digital Gates", 1, 1,
                                       [&](entt::registry &registry, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
                                           auto &gate = registry.get<DigitalComponent>(e);
                                           gate.inputStates = inputs;
                                           std::vector<bool> newStates = {};
                                           for (auto state : gate.inputStates)
                                               newStates.emplace_back(!state);

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
                                                        [&](entt::registry &registry, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
                                                            auto &gate = registry.get<DigitalComponent>(e);
                                                            gate.inputStates = inputs;
                                                            std::vector<bool> pinValues = gate.inputStates;
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

        ComponentCatalog::instance().registerComponent(ComponentDefinition(ComponentType::XNOR, "XNOR Gate", "Digital Gates", 2, 1, [&](entt::registry &registry, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
                                                            auto &gate = registry.get<DigitalComponent>(e);
                                                            gate.inputStates = inputs;
                                                            std::vector<bool> pinValues = gate.inputStates;
                                                            bool newState = (pinValues.size() >= 2) ? !(pinValues[0] ^ pinValues[1]) : false;
                                                            bool changed = false;
                                                            for (auto state : gate.outputStates) {
                                                                if (state != newState) {
                                                                    state = newState;
                                                                    changed = true;
                                                                }
                                                            }
                                                            return changed; }, SimDelayMilliSeconds(100)));

        auto nandGate = ComponentDefinition(
            ComponentType::NAND, "NAND Gate",
            "Digital Gates", 2, 1,
            [&](entt::registry &registry, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
                auto &gate = registry.get<DigitalComponent>(e);
                gate.inputStates = inputs;
                std::vector<bool> pinValues = gate.inputStates;
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
                return changed;
            },
            SimDelayMilliSeconds(100));
        nandGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {3, 4, 5});
        ComponentCatalog::instance().registerComponent(nandGate);

        ComponentCatalog::instance().registerComponent(ComponentDefinition(ComponentType::NOR, "NOR Gate", "Digital Gates", 2, 1, [&](entt::registry &registry, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
                                                            auto &gate = registry.get<DigitalComponent>(e);
                                                            gate.inputStates = inputs;
                                                            std::vector<bool> pinValues = gate.inputStates;
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

    inline void initComponentCatalog() {
        initFlipFlops();
        initDigitalGates();
        initIO();
    }
} // namespace Bess::SimEngine
