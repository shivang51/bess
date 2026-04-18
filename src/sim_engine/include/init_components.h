#pragma once

#include "component_catalog.h"
#include "component_definition.h"
#include "analog_simulation.h"
#include "types.h"
#include <memory>

namespace Bess::SimEngine {
    enum class FrequencyUnit : uint8_t {
        hz,
        kHz,
        MHz
    };

    class ClockTrait : public Trait {
      public:
        ClockTrait() = default;
        ~ClockTrait() override = default;

        std::shared_ptr<Trait> clone() const override {
            return std::make_shared<ClockTrait>(*this);
        }

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
            auto cloned = std::make_shared<ClockDefinition>(*this);
            cloned->m_traits.clear();
            auto clockTrait = m_traits.find<ClockTrait>()->second;
            if (clockTrait) {
                cloned->m_traits.put<ClockTrait>(clockTrait->clone());
            } else {
                assert(false);
            }
            return cloned;
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
        outDef->setSimulationFunction([](const std::vector<SlotState> &inputs, SimTime,
                                         const ComponentState &prevState) -> ComponentState {
						auto newState = prevState;
						newState.inputStates = inputs;
						newState.isChanged = true;
						return newState; });
        outDef->setSimDelay(SimDelayNanoSeconds(0));
        catalog.registerComponent(outDef);
    }

    inline void initAnalog() {
        auto &catalog = ComponentCatalog::instance();

        const auto resistorDef = std::make_shared<ComponentDefinition>();
        resistorDef->setName("Resistor");
        resistorDef->setGroupName("Analog");
        resistorDef->setInputSlotsInfo({SlotsGroupType::input, false, 1, {"+"}, {}});
        resistorDef->setOutputSlotsInfo({SlotsGroupType::output, false, 1, {"-"}, {}});
        resistorDef->addTrait<AnalogComponentTrait>(
            AnalogComponentTrait{2,
                                 {"+", "-"},
                                 [] {
                                     return std::make_shared<Resistor>(1000.0, "R");
                                 }});
        catalog.registerComponent(resistorDef);

        const auto voltageSourceDef = std::make_shared<ComponentDefinition>();
        voltageSourceDef->setName("DC Voltage Source");
        voltageSourceDef->setGroupName("Analog");
        voltageSourceDef->setInputSlotsInfo({SlotsGroupType::input, false, 1, {"+"}, {}});
        voltageSourceDef->setOutputSlotsInfo({SlotsGroupType::output, false, 1, {"-"}, {}});
        voltageSourceDef->addTrait<AnalogComponentTrait>(
            AnalogComponentTrait{2,
                                 {"+", "-"},
                                 [] {
                                     return std::make_shared<DCVoltageSource>(5.0, "V1");
                                 }});
        catalog.registerComponent(voltageSourceDef);

        const auto testPointDef = std::make_shared<ComponentDefinition>();
        testPointDef->setName("Analog Test Point");
        testPointDef->setGroupName("Analog");
        testPointDef->setInputSlotsInfo({SlotsGroupType::input, false, 1, {"TP"}, {}});
        testPointDef->setOutputSlotsInfo({SlotsGroupType::output, false, 0, {}, {}});
        testPointDef->addTrait<AnalogComponentTrait>(
            AnalogComponentTrait{1,
                                 {"TP"},
                                 [] {
                                     return std::make_shared<AnalogTestPoint>("TP");
                                 }});
        catalog.registerComponent(testPointDef);

        const auto voltageProbeDef = std::make_shared<ComponentDefinition>();
        voltageProbeDef->setName("Differential Voltage Probe");
        voltageProbeDef->setGroupName("Analog");
        voltageProbeDef->setInputSlotsInfo({SlotsGroupType::input, false, 1, {"+"}, {}});
        voltageProbeDef->setOutputSlotsInfo({SlotsGroupType::output, false, 1, {"-"}, {}});
        voltageProbeDef->addTrait<AnalogComponentTrait>(
            AnalogComponentTrait{2,
                                 {"+", "-"},
                                 [] {
                                     return std::make_shared<VoltageProbe>("VProbe");
                                 }});
        catalog.registerComponent(voltageProbeDef);

        const auto currentProbeDef = std::make_shared<ComponentDefinition>();
        currentProbeDef->setName("Inline Current Probe");
        currentProbeDef->setGroupName("Analog");
        currentProbeDef->setInputSlotsInfo({SlotsGroupType::input, false, 1, {"IN"}, {}});
        currentProbeDef->setOutputSlotsInfo({SlotsGroupType::output, false, 1, {"OUT"}, {}});
        currentProbeDef->addTrait<AnalogComponentTrait>(
            AnalogComponentTrait{2,
                                 {"IN", "OUT"},
                                 [] {
                                     return std::make_shared<CurrentProbe>("IProbe");
                                 }});
        catalog.registerComponent(currentProbeDef);

        const auto groundDef = std::make_shared<ComponentDefinition>();
        groundDef->setName("Ground");
        groundDef->setGroupName("Analog");
        groundDef->setInputSlotsInfo({SlotsGroupType::input, false, 1, {"GND"}, {}});
        groundDef->setOutputSlotsInfo({SlotsGroupType::output, false, 0, {}, {}});
        groundDef->addTrait<AnalogComponentTrait>(
            AnalogComponentTrait{1,
                                 {"GND"},
                                 [] {
                                     return std::make_shared<GroundReference>("GND");
                                 }});
        catalog.registerComponent(groundDef);
    }

    inline void initComponentCatalog() {
        initIO();
        initAnalog();
    }
} // namespace Bess::SimEngine
