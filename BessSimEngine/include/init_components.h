#pragma once

#include "component_catalog.h"
#include "entt_components.h"
#include "expression_evalutator/expr_evaluator.h"
#include "types.h"
#include <iostream>

namespace Bess::SimEngine {
    inline void initFlipFlops() {
        auto simFunc = [&](entt::registry &reg, entt::entity e, const std::vector<PinState> &inputs, SimTime currentTime, auto fn) -> bool {
            auto &flipFlopComp = reg.get<FlipFlopComponent>(e);
            auto &comp = reg.get<DigitalComponent>(e);
            comp.inputStates = inputs;

            const auto &clrPinState = inputs.back();
            if (clrPinState.state == LogicState::high) {
                bool changed = (comp.outputStates[0].state != LogicState::low);
                comp.outputStates[0] = {LogicState::low, currentTime};
                comp.outputStates[1] = {LogicState::high, currentTime}; // Q' is HIGH
                return changed;
            }

            const auto &clockPinState = inputs[flipFlopComp.clockPinIdx];
            bool isRisingEdge = (clockPinState.state == LogicState::high && flipFlopComp.prevClockState == LogicState::low);

            flipFlopComp.prevClockState = clockPinState.state;

            if (!isRisingEdge) {
                return false;
            }

            const auto &dataPin1 = inputs[0]; // Represents D, T, J, S

            if ((currentTime - dataPin1.lastChangeTime) < flipFlopComp.setupTime) {
                bool changed = (comp.outputStates[0].state != LogicState::unknown);
                comp.outputStates[0] = {LogicState::unknown, currentTime};
                comp.outputStates[1] = {LogicState::unknown, currentTime}; // Q' is also unknown
                return changed;
            }

            if (dataPin1.lastChangeTime == currentTime) {
                bool changed = (comp.outputStates[0].state != LogicState::unknown);
                comp.outputStates[0] = {LogicState::unknown, currentTime};
                comp.outputStates[1] = {LogicState::unknown, currentTime};
                return changed;
            }

            LogicState currentQ = comp.outputStates[0].state;
            LogicState newQ = currentQ;

            if (currentQ == LogicState::unknown) {
                return false; // No change from unknown state
            }

            const auto &j_input = inputs[0];
            const auto &k_input = inputs[2];

            if (flipFlopComp.type == ComponentType::FLIP_FLOP_JK) {
                if (j_input.state == LogicState::high && k_input.state == LogicState::high) {
                    newQ = (currentQ == LogicState::low) ? LogicState::high : LogicState::low; // Toggle
                } else if (j_input.state == LogicState::high) {
                    newQ = LogicState::high; // Set
                } else if (k_input.state == LogicState::high) {
                    newQ = LogicState::low; // Reset
                }
            } else if (flipFlopComp.type == ComponentType::FLIP_FLOP_D) {
                newQ = j_input.state; // D follows the input
            } else if (flipFlopComp.type == ComponentType::FLIP_FLOP_T) {
                if (j_input.state == LogicState::high) {
                    newQ = (currentQ == LogicState::low) ? LogicState::high : LogicState::low; // Toggle
                }
            } else if (flipFlopComp.type == ComponentType::FLIP_FLOP_SR) {
                if (j_input.state == LogicState::high) {
                    newQ = LogicState::high; // Set
                } else if (k_input.state == LogicState::high) {
                    newQ = LogicState::low; // Reset
                }
            }

            bool changed = (comp.outputStates[0].state != newQ);

            comp.outputStates[0] = {newQ, currentTime};

            LogicState newQ_bar = (newQ == LogicState::low) ? LogicState::high : (newQ == LogicState::high) ? LogicState::low
                                                                                                            : LogicState::unknown;
            comp.outputStates[1] = {newQ_bar, currentTime};

            return changed;
        };

        auto &catalog = ComponentCatalog::instance();
        auto flipFlop = ComponentDefinition(ComponentType::FLIP_FLOP_JK, "JK Flip Flop", "Flip Flop", 4, 2, simFunc, SimDelayNanoSeconds(15));
        catalog.registerComponent(flipFlop);

        flipFlop.name = "SR Flip Flop";
        flipFlop.type = ComponentType::FLIP_FLOP_SR;
        catalog.registerComponent(flipFlop);

        flipFlop.name = "T Flip Flop";
        flipFlop.inputCount = 3;
        flipFlop.type = ComponentType::FLIP_FLOP_T;
        catalog.registerComponent(flipFlop);

        flipFlop.name = "D Flip Flop";
        flipFlop.type = ComponentType::FLIP_FLOP_D;
        catalog.registerComponent(flipFlop);
    }

    inline void initIO() {
        ComponentCatalog::instance().registerComponent(ComponentDefinition(ComponentType::INPUT, "Input", "IO", 0, 1, [](auto &, auto, auto, auto, auto) -> bool { return false; }, SimDelayNanoSeconds(0)));

        ComponentCatalog::instance().registerComponent({ComponentType::OUTPUT, "Output", "IO", 1, 0,
                                                        [&](entt::registry &registry, entt::entity e, const std::vector<PinState> &inputs, SimTime _, auto fn) -> bool {
                                                            auto &gate = registry.get<DigitalComponent>(e);
                                                            gate.inputStates = inputs;
                                                            return false;
                                                        },
                                                        SimDelayNanoSeconds(0)});

        ComponentCatalog::instance().registerComponent({ComponentType::SEVEN_SEG_DISPLAY_DRIVER, "7-Seg Display Driver", "IO", 4, 7,
                                                        [&](entt::registry &registry, entt::entity e, const std::vector<PinState> &inputs, SimTime _, auto fn) -> bool {
                                                            auto &gate = registry.get<DigitalComponent>(e);
                                                            gate.inputStates = inputs;
                                                            int dec = 0;
                                                            for (int i = 0; i < (int)inputs.size(); i++) {
                                                                if (inputs[i])
                                                                    dec |= 1 << i;
                                                            }

                                                            int val = 0;
                                                            switch (dec) {
                                                            case 1:
                                                                val = 0b00000110;
                                                                break;
                                                            case 2:
                                                                val = 0b01011011;
                                                                break;
                                                            case 3:
                                                                val = 0b01001111;
                                                                break;
                                                            case 4:
                                                                val = 0b01100110;
                                                                break;
                                                            case 5:
                                                                val = 0b01101101;
                                                                break;
                                                            case 6:
                                                                val = 0b01111101;
                                                                break;
                                                            case 7:
                                                                val = 0b00000111;
                                                                break;
                                                            case 8:
                                                                val = 0b01111111;
                                                                break;
                                                            case 9:
                                                                val = 0b01101111;
                                                                break;
                                                            default:
                                                                val = 0;
                                                                break;
                                                            }

                                                            bool changed = false;
                                                            for (int i = 0; i < (int)gate.outputStates.size(); i++) {
                                                                bool out = val & (1 << i);
                                                                changed = changed || out != (bool)gate.outputStates[i];
                                                                gate.outputStates[i] = out;
                                                            }

                                                            return changed;
                                                        },
                                                        SimDelayNanoSeconds(0)});

        ComponentCatalog::instance().registerComponent({ComponentType::SEVEN_SEG_DISPLAY, "Seven Segment Display", "IO", 7, 0,
                                                        [&](entt::registry &registry, entt::entity e, const std::vector<PinState> &inputs, SimTime _, auto fn) -> bool {
                                                            auto &gate = registry.get<DigitalComponent>(e);
                                                            gate.inputStates = inputs;
                                                            return false;
                                                        },
                                                        SimDelayNanoSeconds(0)});
    }

    /// expression evaluator simulation function
    bool exprEvalSimFunc(entt::registry &registry, entt::entity e, const std::vector<PinState> &inputs, SimTime currentTime, std::function<entt::entity(const UUID &)> fn) {
        auto &comp = registry.get<DigitalComponent>(e);
        comp.inputStates = inputs;
        bool changed = false;
        for (int i = 0; i < (int)comp.expressions.size(); i++) {
            std::vector<bool> states;
            for (auto &state : comp.inputStates)
                states.emplace_back((bool)state);
            bool newState = ExprEval::evaluateExpression(comp.expressions[i], states);
            changed = changed || (bool)comp.outputStates[i] != newState;
            comp.outputStates[i] = {newState ? LogicState::high : LogicState::low, currentTime};
            ;
        }
        return changed;
    }

    inline void initDigitalGates() {
        ComponentDefinition digitalGate = {ComponentType::AND, "AND Gate", "Digital Gates", 2, 1, &exprEvalSimFunc,
                                           SimDelayNanoSeconds(100), '*'};
        digitalGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {3, 4, 5});
        ComponentCatalog::instance().registerComponent(digitalGate);

        digitalGate.type = ComponentType::OR;
        digitalGate.name = "OR Gate";
        digitalGate.op = '+';
        ComponentCatalog::instance().registerComponent(digitalGate);

        digitalGate.type = ComponentType::XOR;
        digitalGate.name = "XOR Gate";
        digitalGate.op = '^';
        ComponentCatalog::instance().registerComponent(digitalGate);

        digitalGate.type = ComponentType::XNOR;
        digitalGate.name = "XNOR Gate";
        digitalGate.op = '^';
        digitalGate.negate = true;
        ComponentCatalog::instance().registerComponent(digitalGate);

        digitalGate.type = ComponentType::NAND;
        digitalGate.name = "NAND Gate";
        digitalGate.op = '*';
        ComponentCatalog::instance().registerComponent(digitalGate);

        digitalGate.type = ComponentType::NOR;
        digitalGate.name = "NOR Gate";
        digitalGate.op = '+';
        ComponentCatalog::instance().registerComponent(digitalGate);

        ComponentDefinition notGate = {ComponentType::NOT, "NOT Gate", "Digital Gates", 1, 1, &exprEvalSimFunc,
                                       SimDelayNanoSeconds(100), '!'};
        notGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {2, 3, 4, 5, 6});
        ComponentCatalog::instance().registerComponent(notGate);
    }

    inline void initCombCircuits() {
        const std::string groupName = "Combinational Circuits";
        ComponentDefinition comp = {ComponentType::FULL_ADDER, "Full Adder", "Combinational Circuits", 3, 2, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"0^1^2", "(0*1) + 2*(0^1)"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::HALF_ADDER, "Half Adder", groupName, 2, 2, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"0^1", "0*1"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::MULTIPLEXER_4_1, "4-to-1 Mux", groupName, 6, 1, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"(0*!5*!4) + (1*!5*4) + (2*5*!4) + (3*5*4)"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::DECODER_2_4, "2-to-4 Decoder", groupName, 2, 4, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"!1*!0", "!1*0", "1*!0", "1*0"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::DEMUX_1_4, "1-to-4 Demux", groupName, 3, 4, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"0*!2*!1", "0*!2*1", "0*2*!1", "0*2*1"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::COMPARATOR_1_BIT, "1-Bit Comparator", groupName, 2, 3, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"0*!1", "!0*1", "!(0^1)"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::ENCODER_4_2, "4-to-2 Encoder", groupName, 4, 2, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"1+3", "2+3"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::HALF_SUBTRACTOR, "Half Subtractor", groupName, 2, 2, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"0^1", "!0*1"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::MULTIPLEXER_2_1, "2-to-1 Mux", groupName, 3, 1, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"(0*!2) + (1*2)"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::PRIORITY_ENCODER_4_2, "4-to-2 Priority Encoder", groupName, 4, 3, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"3 + (!2*1)", "3 + (2*!3)", "0+1+2+3"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::FULL_SUBTRACTOR, "Full Subtractor", groupName, 3, 2, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"0^1^2", "(!0*1) + (!(0^1)*2)"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::COMPARATOR_2_BIT, "2-Bit Comparator", groupName, 4, 3, &exprEvalSimFunc, SimDelayNanoSeconds(100), {"(1*!3)+(!(1^3)*(0*!2))", "(!1*3)+(!(1^3)*(!0*2))", "(!(1^3))*(!(0^2))"}};
        ComponentCatalog::instance().registerComponent(comp);
    }

    inline void initComponentCatalog() {
        initFlipFlops();
        initDigitalGates();
        initIO();
        initCombCircuits();
    }
} // namespace Bess::SimEngine
