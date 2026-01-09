#pragma once

#include "component_catalog.h"
#include "component_definition.h"
#include "expression_evalutator/expr_evaluator.h"
#include "types.h"

namespace Bess::SimEngine {
    class ClockTrait : public Trait {
      public:
        ClockTrait() = default;

        FrequencyUnit frequencyUnit = FrequencyUnit::hz;
        float frequency = 1.0;
        bool high = false; // current output phase
        float dutyCycle = 0.5f;
    };

    class ClockDefinition : public ComponentDefinition {
      public:
        ClockDefinition(const std::string &groupName) {
            m_name = "Clock";
            m_groupName = groupName;
            m_outputSlotsInfo = {SlotsGroupType::output, false, 1, {"", ""}, {}};
            setSimulationFunction([](auto &, auto ts, const auto &oldState) -> ComponentState {
            auto newState = oldState;
						newState.outputStates[0].state = oldState.outputStates[0].state == LogicState::low 
						? LogicState::high 
						: LogicState::low;
						newState.outputStates[0].lastChangeTime = ts;
						newState.isChanged = true;
						return newState; });

            m_shouldAutoReschedule = true;
            m_simDelay = SimDelayNanoSeconds(0);
            addTrait<ClockTrait>();
        }

        SimTime getRescheduleDelay() override {
            const auto &clockTrait = getTrait<ClockTrait>();
            double f = clockTrait->frequency;
            if (clockTrait->frequencyUnit == FrequencyUnit::kHz) {
                f *= 1e3;
            } else if (clockTrait->frequencyUnit == FrequencyUnit::MHz) {
                f *= 1e6;
            }

            if (f <= 0.0) {
                throw std::runtime_error("Invalid clock frequency");
            }

            const double period = 1 / f;
            const double phase = clockTrait->high
                                     ? period * clockTrait->dutyCycle
                                     : period * (1.0 - clockTrait->dutyCycle);

            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(phase));
        }

        void onStateChange(const ComponentState &oldState,
                           const ComponentState &newState) override {
            const auto &clockTrait = getTrait<ClockTrait>();
            clockTrait->high = newState.outputStates[0].state == LogicState::high;
        }

        std::shared_ptr<ComponentDefinition> clone() const override {
            return std::make_shared<ClockDefinition>(*this);
        }
    };

    inline void initIO() {
        auto &catalog = ComponentCatalog::instance();

        const auto inpDef = std::make_shared<ComponentDefinition>();
        inpDef->setName("Input");
        inpDef->setGroupName("IO");
        inpDef->setBehaviorType(ComponentBehaviorType::input);
        inpDef->setOutputSlotsInfo({SlotsGroupType::output, true, 1, {}, {}});
        inpDef->setSimulationFunction([](auto &, auto ts, const auto &oldState) -> ComponentState {
            auto newState = oldState;
						newState.isChanged = true;
						newState.outputStates[0].lastChangeTime = ts;
						return newState; });
        inpDef->setSimDelay(SimDelayNanoSeconds(0));
        catalog.registerComponent(inpDef);

        const auto clockDef = std::make_shared<ClockDefinition>("IO");
        catalog.registerComponent(clockDef);

        const auto outDef = std::make_shared<ComponentDefinition>();
        outDef->setName("Output");
        outDef->setGroupName("IO");
        outDef->setBehaviorType(ComponentBehaviorType::output);
        outDef->setInputSlotsInfo({SlotsGroupType::input, true, 1, {"LSB"}, {}});
        outDef->setSimulationFunction([](const std::vector<SlotState> &, SimTime,
                                         const ComponentState &prevState) -> ComponentState {
						auto newState = prevState;
						return newState; });
        outDef->setSimDelay(SimDelayNanoSeconds(0));
        catalog.registerComponent(outDef);

        // ComponentDefinition stateMonDef = {"State Monitor", "IO", 1, 0,
        //                                    [&](const std::vector<PinState> &inputs, SimTime currentTime, const ComponentState &prevState) -> ComponentState {
        //                                        auto newState = prevState;
        //                                        newState.isChanged = false;
        //                                        if (inputs.size() == 0) {
        //                                            return newState;
        //                                        }
        //                                        newState.inputStates = inputs;
        //                                        return newState;
        //                                    },
        //                                    SimDelayNanoSeconds(0)};
        // stateMonDef.inputPinDetails = {{PinType::input, ""}};
        // ComponentCatalog::instance().registerComponent(stateMonDef, ComponentCatalog::SpecialType::stateMonitor);
        //
        // ComponentCatalog::instance().registerComponent({"7-Seg Display Driver", "IO", 4, 7,
        //                                                 [&](const std::vector<PinState> &inputs, SimTime currentTime, const ComponentState &prevState) -> ComponentState {
        //                                                     auto newState = prevState;
        //                                                     newState.inputStates = inputs;
        //                                                     const bool connected = std::ranges::any_of(prevState.inputConnected, [](bool b) { return b; });
        //                                                     int dec = connected ? 0 : -1;
        //                                                     for (int i = 0; i < (int)inputs.size() && connected; i++) {
        //                                                         if (inputs[i])
        //                                                             dec |= 1 << i;
        //                                                     }
        //
        //                                                     constexpr std::array<int, 16> digitToSegment = {
        //                                                         0b0111111,
        //                                                         0b00000110,
        //                                                         0b01011011,
        //                                                         0b01001111,
        //                                                         0b01100110,
        //                                                         0b01101101,
        //                                                         0b01111101,
        //                                                         0b00000111,
        //                                                         0b01111111,
        //                                                         0b01101111,
        //                                                         0b01110111,
        //                                                         0b01111100,
        //                                                         0b00111001,
        //                                                         0b01011110,
        //                                                         0b01111001,
        //                                                         0b01110001};
        //
        //                                                     const auto val = dec == -1 ? 0 : digitToSegment[dec];
        //
        //                                                     bool changed = false;
        //                                                     for (int i = 0; i < (int)prevState.outputStates.size(); i++) {
        //                                                         bool out = val & (1 << i);
        //                                                         changed = changed || out != (bool)prevState.outputStates[i];
        //                                                         newState.outputStates[i] = out;
        //                                                     }
        //                                                     newState.isChanged = changed;
        //
        //                                                     return newState;
        //                                                 },
        //                                                 SimDelayNanoSeconds(0)});
        //
        // ComponentDefinition def = {"Seven Segment Display", "IO", 7, 0,
        //                            [&](const std::vector<PinState> &inputs, SimTime currentTime, const ComponentState &prevState) -> ComponentState {
        //                                auto newState = prevState;
        //                                newState.inputStates = inputs;
        //                                newState.isChanged = false;
        //                                return newState;
        //                            },
        //                            SimDelayNanoSeconds(0)};
        // ComponentCatalog::instance().registerComponent(def, ComponentCatalog::SpecialType::sevenSegmentDisplay);
    }

    inline void initDigitalGates() {
        // auto registerTriBufferN = [&](int N, const std::string &humanName, const std::string &shortName) {
        //     auto simFuncN = [N](const std::vector<PinState> &inputs, SimTime currentTime, const ComponentState &prevState) -> ComponentState {
        //         auto newState = prevState;
        //         newState.inputStates = inputs;
        //
        //         const PinState &oe = inputs[N];
        //         bool enabled = (oe.state == LogicState::high);
        //
        //         if (prevState.outputStates.size() < (size_t)N)
        //             newState.outputStates.resize(N, {LogicState::high_z, SimTime(0)});
        //
        //         bool anyChanged = false;
        //         for (int i = 0; i < N; ++i) {
        //             PinState newOut;
        //             if (enabled) {
        //                 newOut.state = inputs[i].state;
        //                 newOut.lastChangeTime = currentTime;
        //             } else {
        //                 newOut.state = LogicState::high_z;
        //                 newOut.lastChangeTime = currentTime;
        //             }
        //
        //             if (prevState.outputStates[i].state != newOut.state) {
        //                 newState.outputStates[i] = newOut;
        //                 anyChanged = true;
        //             } else {
        //                 newState.outputStates[i].lastChangeTime = newOut.lastChangeTime;
        //             }
        //         }
        //
        //         newState.isChanged = anyChanged;
        //         return newState;
        //     };
        //
        //     ComponentDefinition inst(
        //         humanName,
        //         shortName,
        //         N + 1,
        //         N,
        //         simFuncN,
        //         SimDelayNanoSeconds(1));
        //
        //     inst.inputPinDetails.clear();
        //     for (int i = 0; i < N; ++i)
        //         inst.inputPinDetails.emplace_back(PinType::input, std::format("D{}", i));
        //     inst.inputPinDetails.emplace_back(PinType::input, "OE");
        //     inst.outputPinDetails.clear();
        //     for (int i = 0; i < N; ++i)
        //         inst.outputPinDetails.emplace_back(PinType::output, std::format("Q{}", i));
        //
        //     ComponentCatalog::instance().registerComponent(inst);
        // };
        //
        // registerTriBufferN(8, "8-Bit Tri-State Buffer", "Digital Gates");
        // registerTriBufferN(4, "4-Bit Tri-State Buffer", "Digital Gates");
        // registerTriBufferN(2, "2-Bit Tri-State Buffer", "Digital Gates");
        // registerTriBufferN(1, "Tri-State Buffer", "Digital Gates");
    }

    inline void initCombCircuits() {
        const std::string groupName = "Combinational Circuits";
        auto &catalog = ComponentCatalog::instance();

        const auto definition = std::make_shared<ComponentDefinition>();
        definition->setGroupName(groupName);
        definition->setSimulationFunction(ExprEval::exprEvalSimFunc);

        definition->setName("Full Adder");
        definition->setInputSlotsInfo({SlotsGroupType::input, false, 3, {"A", "B", "C"}, {}});
        definition->setOutputSlotsInfo({SlotsGroupType::output, false, 2, {"S", "C"}, {}});
        definition->setSimDelay(SimDelayNanoSeconds(3));
        definition->setOutputExpressions({"0^1^2", "(0*1) + 2*(0^1)"});
        catalog.registerComponent(definition);

        // ComponentDefinition comp = {"Full Adder", "Combinational Circuits", 3, 2, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(3), {"0^1^2", "(0*1) + 2*(0^1)"}};
        // comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}, {PinType::input, "C"}};
        // comp.outputPinDetails = {{PinType::output, "S"}, {PinType::output, "C"}};
        // ComponentCatalog::instance().registerComponent(comp);
        //
        // comp = {"Half Adder", groupName, 2, 2, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(2), {"0^1", "0*1"}};
        // comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}};
        // comp.outputPinDetails = {{PinType::output, "S"}, {PinType::output, "C"}};
        // ComponentCatalog::instance().registerComponent(comp);
        //
        // comp = {"4-to-1 Mux", groupName, 6, 1, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(2), {"(0*!5*!4) + (1*!5*4) + (2*5*!4) + (3*5*4)"}};
        // comp.inputPinDetails = {{PinType::input, "D0"}, {PinType::input, "D1"}, {PinType::input, "D2"}, {PinType::input, "D3"}, {PinType::input, "S0"}, {PinType::input, "S1"}};
        // comp.outputPinDetails = {{PinType::output, "Y"}};
        // ComponentCatalog::instance().registerComponent(comp);
        //
        // comp = {"2-to-4 Decoder", groupName, 2, 4, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(2), {"!1*!0", "!1*0", "1*!0", "1*0"}};
        // comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}};
        // comp.outputPinDetails = {{PinType::output, "D0"}, {PinType::output, "D1"}, {PinType::output, "D2"}, {PinType::output, "D3"}};
        // ComponentCatalog::instance().registerComponent(comp);
        //
        // comp = {"1-to-4 Demux", groupName, 3, 4, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(2), {"0*!2*!1", "0*!2*1", "0*2*!1", "0*2*1"}};
        // comp.inputPinDetails = {{PinType::input, "D"}, {PinType::input, "S0"}, {PinType::input, "S1"}};
        // comp.outputPinDetails = {{PinType::output, "Y0"}, {PinType::output, "Y1"}, {PinType::output, "Y2"}, {PinType::output, "Y3"}};
        // ComponentCatalog::instance().registerComponent(comp);
        //
        // comp = {"1-Bit Comparator", groupName, 2, 3, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(3), {"0*!1", "!(0^1)", "!0*1"}};
        // comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}};
        // comp.outputPinDetails = {{PinType::output, "A>B"}, {PinType::output, "A=B"}, {PinType::output, "A<B"}};
        // ComponentCatalog::instance().registerComponent(comp);
        //
        // comp = {"4-to-2 Encoder", groupName, 4, 2, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(3), {"1+3", "2+3"}};
        // comp.inputPinDetails = {{PinType::input, "D0"}, {PinType::input, "D1"}, {PinType::input, "D2"}, {PinType::input, "D3"}};
        // comp.outputPinDetails = {{PinType::output, "Y0"}, {PinType::output, "Y1"}};
        // ComponentCatalog::instance().registerComponent(comp);
        //
        // comp = {"Half Subtractor", groupName, 2, 2, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(3), {"0^1", "!0*1"}};
        // comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}};
        // comp.outputPinDetails = {{PinType::output, "D"}, {PinType::output, "B"}};
        // ComponentCatalog::instance().registerComponent(comp);
        //
        // comp = {"2-to-1 Mux", groupName, 3, 1, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(3), {"(0*!2) + (1*2)"}};
        // comp.inputPinDetails = {{PinType::input, "D0"}, {PinType::input, "D1"}, {PinType::input, "S"}};
        // comp.outputPinDetails = {{PinType::output, "Y"}};
        // ComponentCatalog::instance().registerComponent(comp);
        //
        // comp = {"4-to-2 Priority Encoder", groupName, 4, 3, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(3), {"3 + (!2*1)", "3 + (2*!3)", "0+1+2+3"}};
        // comp.inputPinDetails = {{PinType::input, "D0"}, {PinType::input, "D1"}, {PinType::input, "D2"}, {PinType::input, "D3"}};
        // comp.outputPinDetails = {{PinType::output, "Y0"}, {PinType::output, "Y1"}, {PinType::output, "V"}};
        // ComponentCatalog::instance().registerComponent(comp);
        //
        // comp = {"Full Subtractor", groupName, 3, 2, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(3), {"0^1^2", "(!0*1) + (!(0^1)*2)"}};
        // comp.inputPinDetails = {{PinType::input, "A"}, {PinType::input, "B"}, {PinType::input, "Bin"}};
        // comp.outputPinDetails = {{PinType::output, "D"}, {PinType::output, "Bout"}};
        // ComponentCatalog::instance().registerComponent(comp);
        //
        // comp = {"2-Bit Comparator", groupName, 4, 3, ExprEval::exprEvalSimFunc, SimDelayNanoSeconds(3), {"(1*!3)+(!(1^3)*(0*!2))", "(!1*3)+(!(1^3)*(!0*2))", "(!(1^3))*(!(0^2))"}};
        // comp.inputPinDetails = {{PinType::input, "A0"}, {PinType::input, "A1"}, {PinType::input, "B0"}, {PinType::input, "B1"}};
        // comp.outputPinDetails = {{PinType::output, "A>B"}, {PinType::output, "A<B"}, {PinType::output, "A=B"}};
        // ComponentCatalog::instance().registerComponent(comp);
    }

    inline void initComponentCatalog() {
        initFlipFlops();
        initDigitalGates();
        initIO();
        initCombCircuits();
    }
} // namespace Bess::SimEngine
