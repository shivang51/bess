#pragma once

#include "component_catalog.h"
#include "component_definition.h"
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

    inline void initComponentCatalog() {
        initIO();
    }
} // namespace Bess::SimEngine
