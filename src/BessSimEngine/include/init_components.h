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
            m_outputSlotsInfo = {SlotsGroupType::output, false, 1, {"1.00Hz"}, {}};
            setSimulationFunction([](auto &, auto ts, const auto &oldState) -> ComponentState {
                auto newState = oldState;
                newState.outputStates[0].state = oldState.outputStates[0].state == LogicState::low
                                                     ? LogicState::high
                                                     : LogicState::low;
                newState.outputStates[0].lastChangeTime = ts;
                newState.isChanged = true;
                return newState;
            });

            m_shouldAutoReschedule = true;
            m_simDelay = SimDelayNanoSeconds(0);
            addTrait<ClockTrait>();
        }

        SimTime getRescheduleTime(SimTime currentTime) const override {
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
            double phase = clockTrait->high
                               ? period * clockTrait->dutyCycle
                               : period * (1.0 - clockTrait->dutyCycle);
            const auto nanoSeconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::duration<double>(phase));

            return currentTime + nanoSeconds;
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
    }

    inline void initComponentCatalog() {
        initIO();
    }
} // namespace Bess::SimEngine
