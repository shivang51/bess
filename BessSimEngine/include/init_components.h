#pragma once

#include "component_catalog.h"
#include "entt_components.h"
#include "expression_evalutator/expr_evaluator.h"
#include "types.h"
#include <iostream>

namespace Bess::SimEngine {
    inline void initFlipFlops() {
        auto simFunc = [&](entt::registry &reg, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
            assert(reg.all_of<FlipFlopComponent>(e));
            auto &flipFlopComp = reg.get<FlipFlopComponent>(e);
            auto &comp = reg.get<DigitalComponent>(e);

            // getting values of input pins
            comp.inputStates = inputs;
            auto clockValue = inputs[flipFlopComp.clockPinIdx];
            if (!clockValue || flipFlopComp.prevClock) {
                flipFlopComp.prevClock = clockValue;
                return false;
            }

            flipFlopComp.prevClock = clockValue;

            bool q = comp.outputStates[0], q0 = comp.outputStates[1];
            bool changed = false;
            if (flipFlopComp.type == FlipFlopType::FLIP_FLOP_JK) {
                assert(comp.inputPins.size() == 3);
                if (inputs[0] && inputs[2]) {
                    q = !q;
                } else if (inputs[0]) {
                    q = true;
                } else if (inputs[2]) {
                    q = false;
                }
                q0 = !q;
                changed = comp.outputStates[0] != q;
                comp.outputStates = {q, q0};
            } else if (flipFlopComp.type == FlipFlopType::FLIP_FLOP_SR) {
                assert(comp.inputPins.size() == 3);
                if (inputs[0]) {
                    q = true;
                } else if (inputs[2]) {
                    q = false;
                }
                q0 = !q;
                changed = comp.outputStates[0] != q;
                comp.outputStates = {q, q0};
            } else if (flipFlopComp.type == FlipFlopType::FLIP_FLOP_D) {
                assert(comp.inputPins.size() == 2);
                q = inputs[0];
                q0 = !q;
                changed = comp.outputStates[0] != q;
                comp.outputStates = {q, q0};
            } else if (flipFlopComp.type == FlipFlopType::FLIP_FLOP_T) {
                assert(comp.inputPins.size() == 2);
                if (inputs[0])
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

        ComponentCatalog::instance().registerComponent({ComponentType::SEVEN_SEG_DISPLAY, "Seven Segment Display", "IO", 7, 0,
                                                        [&](entt::registry &registry, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
                                                            auto &gate = registry.get<DigitalComponent>(e);
                                                            gate.inputStates = inputs;
                                                            int val = 0;
                                                            for (int i = 0; i < (int)inputs.size(); i++) {
                                                                if (inputs[i])
                                                                    val |= 1 << i;
                                                            }

                                                            int idx = 0;
                                                            switch (val) {
                                                            case 0b00000110:
                                                                idx = 1;
                                                                break;
                                                            case 0b01101011:
                                                                idx = 2;
                                                                break;
                                                            case 0b01001111:
                                                                idx = 3;
                                                                break;
                                                            case 0b01010110:
                                                                idx = 4;
                                                                break;
                                                            case 0b01011111:
                                                                idx = 5;
                                                                break;
                                                            case 0b01111101:
                                                                idx = 6;
                                                                break;
                                                            case 0b00000111:
                                                                idx = 7;
                                                                break;
                                                            case 0b01111111:
                                                                idx = 8;
                                                                break;
                                                            case 0b01101111:
                                                                idx = 9;
                                                                break;
                                                            default:
                                                                idx = 0;
                                                                break;
                                                            }

                                                            *((int *)gate.auxData) = idx;

                                                            return false;
                                                        },
                                                        SimDelayMilliSeconds(0)});
    }

    /// expression evaluator simulation function
    bool exprEvalSimFunc(entt::registry &registry, entt::entity e, const std::vector<bool> &inputs, std::function<entt::entity(const UUID &)> fn) {
        auto &comp = registry.get<DigitalComponent>(e);
        comp.inputStates = inputs;
        bool changed = false;
        for (int i = 0; i < (int)comp.expressions.size(); i++) {
            bool newState = ExprEval::evaluateExpression(comp.expressions[i], comp.inputStates);
            changed = changed || comp.outputStates[i] != newState;
            comp.outputStates[i] = newState;
        }
        return changed;
    }

    inline void initDigitalGates() {
        ComponentDefinition digitalGate = {ComponentType::AND, "AND Gate", "Digital Gates", 2, 1, &exprEvalSimFunc,
                                           SimDelayMilliSeconds(100), '*'};
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
                                       SimDelayMilliSeconds(100), '!'};
        notGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {2, 3, 4, 5, 6});
        ComponentCatalog::instance().registerComponent(notGate);
    }

    inline void initCombCircuits() {
        const std::string groupName = "Combinational Circuits";
        ComponentDefinition comp = {ComponentType::FULL_ADDER, "Full Adder", "Combinational Circuits", 3, 2, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"0^1^2", "(0*1) + 2*(0^1)"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::HALF_ADDER, "Half Adder", groupName, 2, 2, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"0^1", "0*1"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::MULTIPLEXER_4_1, "4-to-1 Mux", groupName, 6, 1, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"(0*!5*!4) + (1*!5*4) + (2*5*!4) + (3*5*4)"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::DECODER_2_4, "2-to-4 Decoder", groupName, 2, 4, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"!1*!0", "!1*0", "1*!0", "1*0"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::DEMUX_1_4, "1-to-4 Demux", groupName, 3, 4, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"0*!2*!1", "0*!2*1", "0*2*!1", "0*2*1"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::COMPARATOR_1_BIT, "1-Bit Comparator", groupName, 2, 3, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"0*!1", "!0*1", "!(0^1)"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::ENCODER_4_2, "4-to-2 Encoder", groupName, 4, 2, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"1+3", "2+3"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::HALF_SUBTRACTOR, "Half Subtractor", groupName, 2, 2, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"0^1", "!0*1"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::MULTIPLEXER_2_1, "2-to-1 Mux", groupName, 3, 1, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"(0*!2) + (1*2)"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::PRIORITY_ENCODER_4_2, "4-to-2 Priority Encoder", groupName, 4, 3, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"3 + (!2*1)", "3 + (2*!3)", "0+1+2+3"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::FULL_SUBTRACTOR, "Full Subtractor", groupName, 3, 2, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"0^1^2", "(!0*1) + (!(0^1)*2)"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::COMPARATOR_2_BIT, "2-Bit Comparator", groupName, 4, 3, &exprEvalSimFunc, SimDelayMilliSeconds(100), {"(1*!3)+(!(1^3)*(0*!2))", "(!1*3)+(!(1^3)*(!0*2))", "(!(1^3))*(!(0^2))"}};
        ComponentCatalog::instance().registerComponent(comp);
    }

    inline void initComponentCatalog() {
        initFlipFlops();
        initDigitalGates();
        initIO();
        initCombCircuits();
    }
} // namespace Bess::SimEngine
