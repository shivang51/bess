#pragma once

#include "component_catalog.h"
#include "entt_components.h"
#include "types.h"
#include "expression_evalutator/expr_evaluator.h"
#include <iostream>

namespace Bess::SimEngine {
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
        ComponentCatalog::instance().registerComponent(ComponentDefinition(ComponentType::INPUT, "Input", "IO", 0, 1, 
            [](auto &, auto, auto, auto fn) -> bool { return false; }, SimDelayMilliSeconds(0)));

        ComponentCatalog::instance().registerComponent({ComponentType::OUTPUT, "Output", "IO", 1, 0,
                                                        [&](entt::registry &registry, entt::entity e, const std::vector<bool> &inputs, auto fn) -> bool {
                                                            auto &gate = registry.get<DigitalComponent>(e);
                                                            gate.inputStates = inputs;
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

    inline void initComponentCatalog() {
        initFlipFlops();
        initDigitalGates();
        initIO();
    }
} // namespace Bess::SimEngine
