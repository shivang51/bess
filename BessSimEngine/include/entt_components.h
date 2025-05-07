#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include "component_types.h"
#include "json.hpp"
#include "types.h"
#include <entt/entt.hpp>
#include <utility>
#include <vector>

namespace Bess::SimEngine {

    struct BESS_API FlipFlopComponent {
        FlipFlopComponent() = default;
        FlipFlopComponent(FlipFlopType type, int clockPinIndex) {
            this->type = type;
            this->clockPinIdx = clockPinIndex;
        }
        FlipFlopComponent(const FlipFlopComponent &) = default;
        FlipFlopType type = FlipFlopType::FLIP_FLOP_JK;
        int clockPinIdx = 1;
        bool prevClock = false;
    };

    inline void to_json(nlohmann::json &j, const FlipFlopComponent &comp) {
        j["type"] = (int)comp.type;
    }

    inline void from_json(const nlohmann::json &j, FlipFlopComponent &comp) {
        comp.type = (FlipFlopType)j.at("type").get<FlipFlopType>();
    }

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

        std::chrono::milliseconds getNextDelay() const {
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
            double periodMs = 1000.0 / f;
            double phaseMs = high ? periodMs * dutyCycle : periodMs * (1.0 - dutyCycle);
            return std::chrono::milliseconds(static_cast<int>(phaseMs));
        }

        float dutyCycle = 0.5f;
        FrequencyUnit frequencyUnit = FrequencyUnit::hz;
        float frequency = 1.0f;

        bool high = false; // current output phase
    };

    inline void to_json(nlohmann::json &j, const ClockComponent &c) {
        j["frequency"] = c.frequency;
        j["frequencyUnit"] = (int)c.frequencyUnit;
        j["dutyCycle"] = c.dutyCycle;
    }

    inline void from_json(const nlohmann::json &j, ClockComponent &c) {
        c.frequencyUnit = j.at("frequencyUnit").get<FrequencyUnit>();
        c.frequency = j.at("frequency").get<float>();
        c.dutyCycle = j.value("dutyCycle", c.dutyCycle);
    }

    struct BESS_API DigitalComponent {
        DigitalComponent() = default;
        DigitalComponent(const DigitalComponent &) = default;
        DigitalComponent(ComponentType type, int inputPinsCount, int outputPinsCount, SimDelayMilliSeconds delay) {
            this->type = type;
            this->delay = delay;
            this->inputPins = Connections(inputPinsCount, decltype(Connections())::value_type());
            this->outputPins = Connections(outputPinsCount, decltype(Connections())::value_type());
            this->outputStates = std::vector<bool>(outputPinsCount, false);
            this->inputStates = std::vector<bool>(inputPinsCount, false);
        }

        ComponentType type;
        SimDelayMilliSeconds delay;
        Connections inputPins;
        Connections outputPins;
        std::vector<bool> outputStates;
        std::vector<bool> inputStates;
    };
} // namespace Bess::SimEngine
