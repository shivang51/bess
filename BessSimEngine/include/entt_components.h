#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include "component_types.h"
#include "types.h"
#include <entt/entt.hpp>
#include <vector>

namespace Bess::SimEngine {

    struct BESS_API FlipFlopComponent {
        FlipFlopComponent() = default;
        FlipFlopComponent(ComponentType type, int clockPinIndex) {
            this->type = type;
            this->clockPinIdx = clockPinIndex;
        }
        FlipFlopComponent(const FlipFlopComponent &) = default;
        ComponentType type;
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

            double periodNs = 1e9 / f;

            double phaseNs = high ? periodNs * dutyCycle : periodNs * (1.0 - dutyCycle);

            return std::chrono::nanoseconds(static_cast<long long>(phaseNs));
        }

        float dutyCycle = 0.5f;
        FrequencyUnit frequencyUnit = FrequencyUnit::hz;
        float frequency = 1.0f;

        bool high = false; // current output phase
    };

    struct BESS_API DigitalComponent {
        DigitalComponent() = default;
        DigitalComponent(const DigitalComponent &) = default;
        DigitalComponent(ComponentType type, int inputPinsCount, int outputPinsCount, SimDelayNanoSeconds delay,
                         const std::vector<std::string> &expr) {
            this->type = type;
            this->delay = delay;
            this->inputPins = Connections(inputPinsCount, decltype(Connections())::value_type());
            this->outputPins = Connections(outputPinsCount, decltype(Connections())::value_type());
            this->outputStates = std::vector<PinState>(outputPinsCount, {LogicState::low, SimTime(0)});
            this->inputStates = std::vector<PinState>(inputPinsCount, {LogicState::low, SimTime(0)});
            this->expressions = expr;
        }

        void updateInputCount(int n) {
            this->inputPins.resize(n);
            this->inputStates.resize(n);
        }

        ComponentType type;
        SimDelayNanoSeconds delay;
        Connections inputPins;
        Connections outputPins;
        std::vector<PinState> outputStates;
        std::vector<PinState> inputStates;
        std::vector<std::string> expressions;
        void *auxData = nullptr;
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
            timesteps.clear();
        }

        void appendState(SimTime time, const LogicState &state) {
            values.emplace_back(std::pair(time.count(), state));
            bool isHigh = state == LogicState::high;
            if (timesteps.size() == 0) {
                timesteps = {std::pair(0.f, isHigh)};
                return;
            }
            timesteps.emplace_back(std::pair(std::chrono::duration_cast<std::chrono::seconds>(time).count(), isHigh));
        }

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
        std::vector<std::pair<float, bool>> timesteps;
    };
} // namespace Bess::SimEngine
