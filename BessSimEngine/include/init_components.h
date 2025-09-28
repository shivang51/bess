#pragma once

#include "component_catalog.h"
#include "entt_components.h"
#include "expression_evalutator/expr_evaluator.h"
#include "types.h"

namespace Bess::SimEngine {
    inline void initFlipFlops() {
        auto simFunc = [&](entt::registry &reg, const entt::entity e, const std::vector<PinState> &inputs, SimTime currentTime, auto fn) -> bool {
            auto &flipFlopComp = reg.get<FlipFlopComponent>(e);
            auto &comp = reg.get<DigitalComponent>(e);

            comp.inputStates = inputs;

            const auto &clrPinState = inputs.back();
            if (clrPinState.state == LogicState::high) {
                const bool changed = (comp.outputStates[0].state != LogicState::low);
                comp.inputStates[flipFlopComp.clockPinIdx].state = LogicState::high;
                flipFlopComp.prevClockState = LogicState::high;
                comp.outputStates[0] = {LogicState::low, currentTime};
                comp.outputStates[1] = {LogicState::high, currentTime}; // Q' is HIGH
                return changed;
            }

            const auto &clockPinState = inputs[flipFlopComp.clockPinIdx];
            const bool isRisingEdge = (clockPinState.state == LogicState::high && flipFlopComp.prevClockState == LogicState::low);
            flipFlopComp.prevClockState = clockPinState.state;

            if (!isRisingEdge) {
                return false;
            }

            const LogicState currentQ = comp.outputStates[0].state;
            LogicState newQ = currentQ;

            const auto &j_input = inputs[0];
            const auto &k_input = inputs[2];

            if (flipFlopComp.type == ComponentType::FLIP_FLOP_JK) {
                if (j_input.state == LogicState::high && k_input.state == LogicState::high) {
                    newQ = (currentQ == LogicState::low) ? LogicState::high : LogicState::low; // Toggle
                } else if (j_input.state == LogicState::high) {
                    newQ = LogicState::high;
                } else if (k_input.state == LogicState::high) {
                    newQ = LogicState::low;
                }
            } else if (flipFlopComp.type == ComponentType::FLIP_FLOP_D) {
                newQ = j_input.state;
            } else if (flipFlopComp.type == ComponentType::FLIP_FLOP_T) {
                if (j_input.state == LogicState::high) {
                    newQ = (currentQ == LogicState::low) ? LogicState::high : LogicState::low; // Toggle
                }
            } else if (flipFlopComp.type == ComponentType::FLIP_FLOP_SR) {
                if (j_input.state == LogicState::high) {
                    newQ = LogicState::high;
                } else if (k_input.state == LogicState::high) {
                    newQ = LogicState::low;
                }
            }

            const bool changed = (comp.outputStates[0].state != newQ);

            comp.outputStates[0] = {newQ, currentTime};

            LogicState newQ_bar = (newQ == LogicState::low) ? LogicState::high : (newQ == LogicState::high) ? LogicState::low
                                                                                                            : LogicState::unknown;
            comp.outputStates[1] = {newQ_bar, currentTime};

            return changed;
        };

        auto &catalog = ComponentCatalog::instance();
        auto flipFlop = ComponentDefinition(ComponentType::FLIP_FLOP_JK, "JK Flip Flop", "Flip Flop", 4, 2, simFunc, SimDelayNanoSeconds(5));
        flipFlop.inputPinDetails = {};
        flipFlop.inputPinDetails.emplace_back(PinType::input, "J", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLK", ExtendedPinType::inputClock);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "K", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLR", ExtendedPinType::inputClear);
        catalog.registerComponent(flipFlop);

        flipFlop.name = "SR Flip Flop";
        flipFlop.type = ComponentType::FLIP_FLOP_SR;
        flipFlop.inputPinDetails = {};
        flipFlop.inputPinDetails.emplace_back(PinType::input, "S", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLK", ExtendedPinType::inputClock);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "R", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLR", ExtendedPinType::inputClear);
        catalog.registerComponent(flipFlop);

        flipFlop.name = "T Flip Flop";
        flipFlop.inputCount = 3;
        flipFlop.type = ComponentType::FLIP_FLOP_T;
        flipFlop.inputPinDetails = {};
        flipFlop.inputPinDetails.emplace_back(PinType::input, "T", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLK", ExtendedPinType::inputClock);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLR", ExtendedPinType::inputClear);
        catalog.registerComponent(flipFlop);

        flipFlop.name = "D Flip Flop";
        flipFlop.type = ComponentType::FLIP_FLOP_D;
        flipFlop.inputPinDetails = {};
        flipFlop.inputPinDetails.emplace_back(PinType::input, "D", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLK", ExtendedPinType::inputClock);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLR", ExtendedPinType::inputClear);
        catalog.registerComponent(flipFlop);
    }

    inline void initIO() {
        auto inpDef = ComponentDefinition(ComponentType::INPUT, "Input", "IO", 0, 1, [](auto &, auto, auto, auto, auto) -> bool { return false; }, SimDelayNanoSeconds(0));
        inpDef.outputPinDetails = {{PinType::output, ""}};
        ComponentCatalog::instance().registerComponent(inpDef);

        ComponentDefinition outDef = {ComponentType::OUTPUT, "Output", "IO", 1, 0,
                                      [&](entt::registry &registry, const entt::entity e, const std::vector<PinState> &inputs, SimTime _, auto fn) -> bool {
                                          auto &gate = registry.get<DigitalComponent>(e);
                                          gate.inputStates = inputs;
                                          return false;
                                      },
                                      SimDelayNanoSeconds(0)};
        outDef.inputPinDetails = {{PinType::input, ""}};
        ComponentCatalog::instance().registerComponent(outDef);

        ComponentDefinition stateMonDef = {ComponentType::STATE_MONITOR, "State Monitor", "IO", 1, 0,
                                           [&](entt::registry &registry, const entt::entity e, const std::vector<PinState> &inputs, SimTime simTime, auto fn) -> bool {
                                               if (inputs.size() == 0)
                                                   return false;
                                               auto &comp = registry.get<StateMonitorComponent>(e);
                                               comp.appendState(inputs[0].lastChangeTime, inputs[0].state);

                                               auto &digiComp = registry.get<DigitalComponent>(e);
                                               digiComp.inputStates = inputs;
                                               return false;
                                           },
                                           SimDelayNanoSeconds(0)};
        outDef.inputPinDetails = {{PinType::input, ""}};
        ComponentCatalog::instance().registerComponent(stateMonDef);

        ComponentCatalog::instance().registerComponent({ComponentType::SEVEN_SEG_DISPLAY_DRIVER, "7-Seg Display Driver", "IO", 4, 7,
                                                        [&](entt::registry &registry, const entt::entity e, const std::vector<PinState> &inputs, SimTime _, auto fn) -> bool {
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
                                                                const bool out = val & (1 << i);
                                                                changed = changed || out != (bool)gate.outputStates[i];
                                                                gate.outputStates[i] = out;
                                                            }

                                                            return changed;
                                                        },
                                                        SimDelayNanoSeconds(0)});

        ComponentCatalog::instance().registerComponent({ComponentType::SEVEN_SEG_DISPLAY, "Seven Segment Display", "IO", 7, 0,
                                                        [&](entt::registry &registry, const entt::entity e, const std::vector<PinState> &inputs, SimTime _, auto fn) -> bool {
                                                            auto &gate = registry.get<DigitalComponent>(e);
                                                            gate.inputStates = inputs;
                                                            return false;
                                                        },
                                                        SimDelayNanoSeconds(0)});
    }

    /// expression evaluator simulation function
    inline bool exprEvalSimFunc(entt::registry &registry, const entt::entity e, const std::vector<PinState> &inputs, SimTime currentTime, std::function<entt::entity(const UUID &)> fn) {
        auto &comp = registry.get<DigitalComponent>(e);
        comp.inputStates = inputs;
        bool changed = false;
        for (int i = 0; i < (int)comp.expressions.size(); i++) {
            std::vector<bool> states;
            states.reserve(comp.inputStates.size());
            for (auto &state : comp.inputStates)
                states.emplace_back((bool)state);
            const bool newState = ExprEval::evaluateExpression(comp.expressions[i], states);
            changed = changed || (bool)comp.outputStates[i] != newState;
            comp.outputStates[i] = {newState ? LogicState::high : LogicState::low, currentTime};
        }
        return changed;
    }

    inline void initDigitalGates() {
        ComponentDefinition digitalGate = {ComponentType::AND, "AND Gate", "Digital Gates", 2, 1, &exprEvalSimFunc,
                                           SimDelayNanoSeconds(1), '*'};
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
                                       SimDelayNanoSeconds(2), '!'};
        notGate.addModifiableProperty(Properties::ComponentProperty::inputCount, {2, 3, 4, 5, 6});
        ComponentCatalog::instance().registerComponent(notGate);

        auto registerTriBufferN = [&](int N, const ComponentType type, const std::string &humanName, const std::string &shortName) {
            auto simFuncN = [N](entt::registry &reg, const entt::entity e, const std::vector<PinState> &inputs, const SimTime currentTime, auto fn) -> bool {
                // inputs expected: D0..D(N-1), OE  => inputs.size() == N+1
                if ((int)inputs.size() < N + 1)
                    return false;
                auto &comp = reg.get<DigitalComponent>(e);

                const PinState &oe = inputs[N];
                const bool enabled = (oe.state == LogicState::high);

                if (comp.outputStates.size() < (size_t)N)
                    comp.outputStates.resize(N, {LogicState::high_z, SimTime(0)});

                bool anyChanged = false;
                for (int i = 0; i < N; ++i) {
                    PinState newOut;
                    if (enabled) {
                        newOut.state = inputs[i].state;
                        newOut.lastChangeTime = currentTime;
                    } else {
                        newOut.state = LogicState::high_z;
                        newOut.lastChangeTime = currentTime;
                    }

                    if (comp.outputStates[i].state != newOut.state) {
                        comp.outputStates[i] = newOut;
                        anyChanged = true;
                    } else {
                        comp.outputStates[i].lastChangeTime = newOut.lastChangeTime;
                    }
                }
                return anyChanged;
            };

            ComponentDefinition inst(
                type,
                humanName,
                shortName,
                N + 1,
                N,
                simFuncN,
                SimDelayNanoSeconds(1));

            inst.inputPinDetails.clear();
            for (int i = 0; i < N; ++i)
                inst.inputPinDetails.emplace_back(PinType::input, std::format("D{}", i));
            inst.inputPinDetails.emplace_back(PinType::input, "OE");
            inst.outputPinDetails.clear();
            for (int i = 0; i < N; ++i)
                inst.outputPinDetails.emplace_back(PinType::output, std::format("Q{}", i));

            ComponentCatalog::instance().registerComponent(inst);
        };

        const auto *groupName = "Digital Gates";
        registerTriBufferN(8, ComponentType::TRISTATE_BUFFER_8BIT, "8-Bit Tri-State Buffer", groupName);
        registerTriBufferN(4, ComponentType::TRISTATE_BUFFER_4BIT, "4-Bit Tri-State Buffer", groupName);
        registerTriBufferN(1, ComponentType::TRISTATE_BUFFER, "Tri-State Buffer", groupName);
    }

    inline void initCombCircuits() {
        const std::string groupName = "Combinational Circuits";
        ComponentDefinition comp = {ComponentType::FULL_ADDER, "Full Adder", "Combinational Circuits", 3, 2, &exprEvalSimFunc, SimDelayNanoSeconds(3), {"0^1^2", "(0*1) + 2*(0^1)"}};
        comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}, {PinType::input, "C"}};
        comp.outputPinDetails = {{PinType::output, "S"}, {PinType::output, "C"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::HALF_ADDER, "Half Adder", groupName, 2, 2, &exprEvalSimFunc, SimDelayNanoSeconds(2), {"0^1", "0*1"}};
        comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}};
        comp.outputPinDetails = {{PinType::output, "S"}, {PinType::output, "C"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::MULTIPLEXER_4_1, "4-to-1 Mux", groupName, 6, 1, &exprEvalSimFunc, SimDelayNanoSeconds(2), {"(0*!5*!4) + (1*!5*4) + (2*5*!4) + (3*5*4)"}};
        comp.inputPinDetails = {{PinType::input, "D0"}, {PinType::input, "D1"}, {PinType::input, "D2"}, {PinType::input, "D3"}, {PinType::input, "S0"}, {PinType::input, "S1"}};
        comp.outputPinDetails = {{PinType::output, "Y"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::DECODER_2_4, "2-to-4 Decoder", groupName, 2, 4, &exprEvalSimFunc, SimDelayNanoSeconds(2), {"!1*!0", "!1*0", "1*!0", "1*0"}};
        comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}};
        comp.outputPinDetails = {{PinType::output, "D0"}, {PinType::output, "D1"}, {PinType::output, "D2"}, {PinType::output, "D3"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::DEMUX_1_4, "1-to-4 Demux", groupName, 3, 4, &exprEvalSimFunc, SimDelayNanoSeconds(2), {"0*!2*!1", "0*!2*1", "0*2*!1", "0*2*1"}};
        comp.inputPinDetails = {{PinType::input, "D"}, {PinType::input, "S0"}, {PinType::input, "S1"}};
        comp.outputPinDetails = {{PinType::output, "Y0"}, {PinType::output, "Y1"}, {PinType::output, "Y2"}, {PinType::output, "Y3"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::COMPARATOR_1_BIT, "1-Bit Comparator", groupName, 2, 3, &exprEvalSimFunc, SimDelayNanoSeconds(3), {"0*!1", "!(0^1)", "!0*1"}};
        comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}};
        comp.outputPinDetails = {{PinType::output, "A>B"}, {PinType::output, "A=B"}, {PinType::output, "A<B"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::ENCODER_4_2, "4-to-2 Encoder", groupName, 4, 2, &exprEvalSimFunc, SimDelayNanoSeconds(3), {"1+3", "2+3"}};
        comp.inputPinDetails = {{PinType::input, "D0"}, {PinType::input, "D1"}, {PinType::input, "D2"}, {PinType::input, "D3"}};
        comp.outputPinDetails = {{PinType::output, "Y0"}, {PinType::output, "Y1"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::HALF_SUBTRACTOR, "Half Subtractor", groupName, 2, 2, &exprEvalSimFunc, SimDelayNanoSeconds(3), {"0^1", "!0*1"}};
        comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}};
        comp.outputPinDetails = {{PinType::output, "D"}, {PinType::output, "B"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::MULTIPLEXER_2_1, "2-to-1 Mux", groupName, 3, 1, &exprEvalSimFunc, SimDelayNanoSeconds(3), {"(0*!2) + (1*2)"}};
        comp.inputPinDetails = {{PinType::input, "D0"}, {PinType::input, "D1"}, {PinType::input, "S"}};
        comp.outputPinDetails = {{PinType::output, "Y"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::PRIORITY_ENCODER_4_2, "4-to-2 Priority Encoder", groupName, 4, 3, &exprEvalSimFunc, SimDelayNanoSeconds(3), {"3 + (!2*1)", "3 + (2*!3)", "0+1+2+3"}};
        comp.inputPinDetails = {{PinType::input, "D0"}, {PinType::input, "D1"}, {PinType::input, "D2"}, {PinType::input, "D3"}};
        comp.outputPinDetails = {{PinType::output, "Y0"}, {PinType::output, "Y1"}, {PinType::output, "V"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::FULL_SUBTRACTOR, "Full Subtractor", groupName, 3, 2, &exprEvalSimFunc, SimDelayNanoSeconds(3), {"0^1^2", "(!0*1) + (!(0^1)*2)"}};
        comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}, {PinType::input, "Bin"}};
        comp.outputPinDetails = {{PinType::output, "D"}, {PinType::output, "Bout"}};
        ComponentCatalog::instance().registerComponent(comp);

        comp = {ComponentType::COMPARATOR_2_BIT, "2-Bit Comparator", groupName, 4, 3, &exprEvalSimFunc, SimDelayNanoSeconds(3), {"(1*!3)+(!(1^3)*(0*!2))", "(!1*3)+(!(1^3)*(!0*2))", "(!(1^3))*(!(0^2))"}};
        comp.inputPinDetails = {{PinType::input, "A0"}, {PinType::input, "A1"}, {PinType::input, "B0"}, {PinType::input, "B1"}};
        comp.outputPinDetails = {{PinType::output, "A>B"}, {PinType::output, "A<B"}, {PinType::output, "A=B"}};
        ComponentCatalog::instance().registerComponent(comp);
    }

    inline void initComponentCatalog() {
        initFlipFlops();
        initDigitalGates();
        initIO();
        initCombCircuits();
    }
} // namespace Bess::SimEngine
