#pragma once

#include "component_catalog.h"
#include "entt_components.h"
#include "expression_evalutator/expr_evaluator.h"
#include "types.h"
#include <memory>

namespace Bess::SimEngine {

    struct FlipFlopAuxData {
        int clockPinIdx = 1;
        int clearPinIdx = 3;
        enum class FlipFlopType : uint8_t {
            JK,
            SR,
            D,
            T
        } type;
    };

    inline void initFlipFlops() {
        auto simFunc = [&](const std::vector<PinState> &inputs, SimTime currentTime, const ComponentState &prevState) -> ComponentState {
            ComponentState newState = prevState;
            newState.inputStates = inputs;
            const auto &clrPinState = inputs.back();
            auto flipFlopData = std::any_cast<FlipFlopAuxData>(prevState.auxData);

            int clockPinIdx = flipFlopData->clockPinIdx;
            int clearPinIdx = flipFlopData->clearPinIdx;

            auto prevClockState = prevState.inputStates[clockPinIdx].state;
            if (clrPinState.state == LogicState::high) {
                newState.isChanged = (prevState.outputStates[0].state != LogicState::low);
                newState.inputStates[clockPinIdx].state = LogicState::high;
                newState.outputStates[0] = {LogicState::low, currentTime};
                newState.outputStates[1] = {LogicState::high, currentTime}; // Q' is HIGH
                return newState;
            }

            const auto &clockPinState = inputs[flipFlopData->clockPinIdx];
            bool isRisingEdge = (clockPinState.state == LogicState::high &&
                                 prevState.inputStates[clockPinIdx].state == LogicState::low);

            if (!isRisingEdge) {
                return newState;
            }

            LogicState currentQ = prevState.outputStates[0].state;
            LogicState newQ = currentQ;

            const auto &j_input = inputs[0];
            const auto &k_input = inputs[2];

            auto type = flipFlopData->type;

            switch (type) {
            case FlipFlopAuxData::FlipFlopType::JK: {
                if (j_input.state == LogicState::high && k_input.state == LogicState::high) {
                    newQ = (currentQ == LogicState::low) ? LogicState::high : LogicState::low; // Toggle
                } else if (j_input.state == LogicState::high) {
                    newQ = LogicState::high;
                } else if (k_input.state == LogicState::high) {
                    newQ = LogicState::low;
                }
            } break;
            case FlipFlopAuxData::FlipFlopType::SR: {
                if (j_input.state == LogicState::high) {
                    newQ = LogicState::high;
                } else if (k_input.state == LogicState::high) {
                    newQ = LogicState::low;
                }
            } break;
            case FlipFlopAuxData::FlipFlopType::D: {
                newQ = j_input.state;
            } break;
            case FlipFlopAuxData::FlipFlopType::T: {
                if (j_input.state == LogicState::high) {
                    newQ = (currentQ == LogicState::low)
                               ? LogicState::high
                               : LogicState::low;
                }
            } break;
            }

            bool changed = (prevState.outputStates[0].state != newQ);

            if (!changed)
                return newState;

            newState.outputStates[0] = {newQ, currentTime};

            LogicState newQ_bar = (newQ == LogicState::low)
                                      ? LogicState::high
                                  : (newQ == LogicState::high) ? LogicState::low
                                                               : LogicState::unknown;
            newState.outputStates[1] = {newQ_bar, currentTime};

            newState.isChanged = changed;

            return newState;
        };

        auto &catalog = ComponentCatalog::instance();
        auto flipFlop = ComponentDefinition(ComponentType::FLIP_FLOP_JK, "JK Flip Flop", "Flip Flop", 4, 2, simFunc, SimDelayNanoSeconds(5));
        flipFlop.inputPinDetails = {};
        flipFlop.inputPinDetails.emplace_back(PinType::input, "J", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLK", ExtendedPinType::inputClock);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "K", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLR", ExtendedPinType::inputClear);
        flipFlop.auxData = FlipFlopAuxData{1, 3, FlipFlopAuxData::FlipFlopType::JK};
        catalog.registerComponent(flipFlop);

        flipFlop.name = "SR Flip Flop";
        flipFlop.type = ComponentType::FLIP_FLOP_SR;
        flipFlop.inputPinDetails = {};
        flipFlop.inputPinDetails.emplace_back(PinType::input, "S", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLK", ExtendedPinType::inputClock);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "R", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLR", ExtendedPinType::inputClear);
        flipFlop.auxData = FlipFlopAuxData{1, 3, FlipFlopAuxData::FlipFlopType::SR};
        catalog.registerComponent(flipFlop);

        flipFlop.name = "T Flip Flop";
        flipFlop.inputCount = 3;
        flipFlop.type = ComponentType::FLIP_FLOP_T;
        flipFlop.inputPinDetails = {};
        flipFlop.inputPinDetails.emplace_back(PinType::input, "T", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLK", ExtendedPinType::inputClock);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLR", ExtendedPinType::inputClear);
        flipFlop.auxData = FlipFlopAuxData{1, 2, FlipFlopAuxData::FlipFlopType::T};
        catalog.registerComponent(flipFlop);

        flipFlop.name = "D Flip Flop";
        flipFlop.type = ComponentType::FLIP_FLOP_D;
        flipFlop.inputPinDetails = {};
        flipFlop.inputPinDetails.emplace_back(PinType::input, "D", ExtendedPinType::none);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLK", ExtendedPinType::inputClock);
        flipFlop.inputPinDetails.emplace_back(PinType::input, "CLR", ExtendedPinType::inputClear);
        flipFlop.auxData = FlipFlopAuxData{1, 2, FlipFlopAuxData::FlipFlopType::D};
        catalog.registerComponent(flipFlop);
    }

    inline void initIO() {
        auto inpDef = ComponentDefinition(ComponentType::INPUT, "Input", "IO", 0, 1, [](auto &, auto, auto &) -> ComponentState { return {}; }, SimDelayNanoSeconds(0));
        inpDef.outputPinDetails = {{PinType::output, ""}};
        ComponentCatalog::instance().registerComponent(inpDef);

        ComponentDefinition outDef = {ComponentType::OUTPUT, "Output", "IO", 1, 0,
                                      [&](const std::vector<PinState> &inputs, SimTime currentTime, const ComponentState &prevState) -> ComponentState {
                                          auto newState = prevState;
                                          newState.inputStates = inputs;
                                          return newState;
                                      },
                                      SimDelayNanoSeconds(0)};
        outDef.inputPinDetails = {{PinType::input, ""}};
        ComponentCatalog::instance().registerComponent(outDef);

        ComponentDefinition stateMonDef = {ComponentType::STATE_MONITOR, "State Monitor", "IO", 1, 0,
                                           [&](const std::vector<PinState> &inputs, SimTime currentTime, const ComponentState &prevState) -> ComponentState {
                                               auto newState = prevState;
                                               newState.isChanged = false;
                                               if (inputs.size() == 0) {
                                                   return newState;
                                               }
                                               // comp.appendState(inputs[0].lastChangeTime, inputs[0].state);

                                               // auto &digiComp = registry.get<DigitalComponent>(e);
                                               newState.inputStates = inputs;
                                               return newState;
                                           },
                                           SimDelayNanoSeconds(0)};
        outDef.inputPinDetails = {{PinType::input, ""}};
        ComponentCatalog::instance().registerComponent(stateMonDef);

        ComponentCatalog::instance().registerComponent({ComponentType::SEVEN_SEG_DISPLAY_DRIVER, "7-Seg Display Driver", "IO", 4, 7,
                                                        [&](const std::vector<PinState> &inputs, SimTime currentTime, const ComponentState &prevState) -> ComponentState {
                                                            auto newState = prevState;
                                                            newState.inputStates = inputs;
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
                                                            for (int i = 0; i < (int)prevState.outputStates.size(); i++) {
                                                                bool out = val & (1 << i);
                                                                changed = changed || out != (bool)prevState.outputStates[i];
                                                                newState.outputStates[i] = out;
                                                            }
                                                            newState.isChanged = changed;

                                                            return newState;
                                                        },
                                                        SimDelayNanoSeconds(0)});

        ComponentCatalog::instance().registerComponent({ComponentType::SEVEN_SEG_DISPLAY, "Seven Segment Display", "IO", 7, 0,
                                                        [&](const std::vector<PinState> &inputs, SimTime currentTime, const ComponentState &prevState) -> ComponentState {
                                                            auto newState = prevState;
                                                            newState.inputStates = inputs;
                                                            newState.isChanged = false;
                                                            return newState;
                                                        },
                                                        SimDelayNanoSeconds(0)});
    }

    /// expression evaluator simulation function
    inline ComponentState exprEvalSimFunc(const std::vector<PinState> &inputs, SimTime currentTime, const ComponentState &prevState) {
        auto newState = prevState;
        newState.inputStates = inputs;
        bool changed = false;
        auto expressions = std::any_cast<std::vector<std::string>>(prevState.auxData);
        for (int i = 0; i < (int)expressions->size(); i++) {
            std::vector<bool> states;
            states.reserve(inputs.size());
            for (auto &state : inputs)
                states.emplace_back((bool)state);
            bool newStateBool = ExprEval::evaluateExpression(expressions->at(i), states);
            changed = changed || (bool)prevState.outputStates[i] != newStateBool;
            newState.outputStates[i] = {newStateBool ? LogicState::high : LogicState::low, currentTime};
        }
        newState.isChanged = changed;
        return newState;
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

        auto registerTriBufferN = [&](int N, ComponentType type, const std::string &humanName, const std::string &shortName) {
            auto simFuncN = [N](const std::vector<PinState> &inputs, SimTime currentTime, const ComponentState &prevState) -> ComponentState {
                auto newState = prevState;
                newState.inputStates = inputs;

                const PinState &oe = inputs[N];
                bool enabled = (oe.state == LogicState::high);

                if (prevState.outputStates.size() < (size_t)N)
                    newState.outputStates.resize(N, {LogicState::high_z, SimTime(0)});

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

                    if (prevState.outputStates[i].state != newOut.state) {
                        newState.outputStates[i] = newOut;
                        anyChanged = true;
                    } else {
                        newState.outputStates[i].lastChangeTime = newOut.lastChangeTime;
                    }
                }

                newState.isChanged = anyChanged;
                return newState;
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

        registerTriBufferN(8, ComponentType::TRISTATE_BUFFER_8BIT, "8-Bit Tri-State Buffer", "Digital Gates");
        registerTriBufferN(4, ComponentType::TRISTATE_BUFFER_4BIT, "4-Bit Tri-State Buffer", "Digital Gates");
        registerTriBufferN(2, ComponentType::TRISTATE_BUFFER_2BIT, "2-Bit Tri-State Buffer", "Digital Gates");
        registerTriBufferN(1, ComponentType::TRISTATE_BUFFER, "Tri-State Buffer", "Digital Gates");
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
