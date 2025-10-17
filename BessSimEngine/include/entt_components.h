#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include "component_definition.h"
#include "component_types/component_types.h"
#include "types.h"
#include <entt/entt.hpp>
#include <iostream>
#include <utility>
#include <vector>

namespace Bess::SimEngine {

    struct BESS_API FlipFlopComponent {
        FlipFlopComponent() = default;
        FlipFlopComponent(int clockPinIndex) : clockPinIdx(clockPinIndex) {
        }
        FlipFlopComponent(const FlipFlopComponent &) = default;
        int clockPinIdx = 1;
        LogicState prevClockState = LogicState::low;
    };

    struct BESS_API IdComponent {
        IdComponent() = default;
        IdComponent(UUID uuid) { this->uuid = uuid; }
        IdComponent(const IdComponent &) = default;
        UUID uuid;
    };

    struct BESS_API ClockComponent {
        ClockComponent() = default;
        ClockComponent(const ClockComponent &) = default;

        void setup(float freq, FrequencyUnit unit) {
            frequency = freq;
            frequencyUnit = unit;
            high = false;
        }

        void toggle() {
            high = !high;
        }

        std::chrono::nanoseconds getNextDelay() const {
            double f = frequency;
            switch (frequencyUnit) {
            case FrequencyUnit::hz:
                break;
            case FrequencyUnit::kHz:
                f *= 1e3;
                break;
            case FrequencyUnit::MHz:
                f *= 1e6;
                break;
            default:
                throw std::runtime_error("Unhandled clock frequency unit");
            }

            if (f <= 0.0) {
                throw std::runtime_error("Invalid clock frequency");
            }

            double period = 1 / f;
            double phase = high ? period * dutyCycle : period * (1.0 - dutyCycle);
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(phase));
        }

        float dutyCycle = 0.5f;
        FrequencyUnit frequencyUnit = FrequencyUnit::hz;
        float frequency = 1.0f;

        bool high = false; // current output phase
    };

    struct BESS_API DigitalComponent {
        DigitalComponent() = delete;
        DigitalComponent(const DigitalComponent &) = default;
        DigitalComponent(ComponentDefinition def) : definition(std::move(def)) {
            state.inputStates.resize(definition.inputCount, {LogicState::low, SimTime(0)});
            state.outputStates.resize(definition.outputCount, {LogicState::low, SimTime(0)});
            state.inputConnected.resize(definition.inputCount, false);
            state.outputConnected.resize(definition.outputCount, false);
            state.auxData = &definition.auxData;
            inputConnections.resize(definition.inputCount);
            outputConnections.resize(definition.outputCount);
        }

        DigitalComponent(ComponentDefinition def, ComponentState state) : definition(std::move(def)), state(std::move(state)) {}

        ComponentState state;
        ComponentDefinition definition;
        Connections inputConnections;
        Connections outputConnections;
    };

    struct BESS_API StateMonitorComponent {
        StateMonitorComponent() = default;
        StateMonitorComponent(const StateMonitorComponent &) = default;
        StateMonitorComponent(ComponentPin pin, PinType type) {
            attachedTo = pin;
            attachedToType = type;
        }

        void clear() {
            values.clear();
            timestepedBoolData.clear();
        }

        void appendState(SimTime time, const LogicState &state) {
            values.emplace_back(time.count(), state);

            bool isHigh = state == LogicState::high;
            auto clockTime = clock.now();
            if (timestepedBoolData.empty()) {
                timestepedBoolData.emplace_back(0.f, isHigh);
                lastUpdateTime = clockTime;
                return;
            }

            float diff = std::chrono::duration<float>(clockTime - lastUpdateTime).count();
            float value = timestepedBoolData.back().first + diff;
            timestepedBoolData.emplace_back(value, isHigh);
            lastUpdateTime = clockTime;
        }

        static constexpr std::chrono::steady_clock clock{};

        void attacthTo(ComponentPin pin, PinType type) {
            clear();
            attachedTo = pin;
            attachedToType = type;
        }

        ComponentPin attachedTo;
        PinType attachedToType;

        /// Values of states of attached pin
        /// vector of  pair<<time in nanoseconds, LogicState>>
        std::vector<std::pair<float, LogicState>> values = {};

        /// Vector of <timestep, bool>, timesteps are in seconds here
        /// Note(Shivang): I have no idea if this is right thing to do, to track data along a consistent time
        std::vector<std::pair<float, bool>> timestepedBoolData;

        std::chrono::steady_clock::time_point lastUpdateTime{};
    };
} // namespace Bess::SimEngine
