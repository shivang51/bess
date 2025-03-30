#pragma once

#include "bess_uuid.h"
#include "component_types.h"
#include "json.hpp"
#include "types.h"
#include <entt/entt.hpp>
#include <utility>
#include <vector>

namespace Bess::SimEngine {

    struct FlipFlopComponent {
        FlipFlopComponent() = default;
        FlipFlopComponent(FlipFlopType type, int clockPinIndex) {
            this->type = type;
            this->clockPinIdx = clockPinIndex;
        }
        FlipFlopComponent(const FlipFlopComponent &) = default;
        FlipFlopType type = FlipFlopType::FLIP_FLOP_JK;
        int clockPinIdx = 1;
    };

    inline void to_json(nlohmann::json &j, const FlipFlopComponent &comp) {
        j["type"] = (int)comp.type;
    }

    inline void from_json(const nlohmann::json &j, FlipFlopComponent &comp) {
        comp.type = (FlipFlopType)j.at("type").get<FlipFlopType>();
    }

    struct IdComponent {
        IdComponent() = default;
        IdComponent(UUID uuid) { this->uuid = uuid; }
        IdComponent(const IdComponent &) = default;
        UUID uuid;
    };

    struct ClockComponent {
        ClockComponent() = default;
        ClockComponent(const ClockComponent &) = default;

        SimDelayMilliSeconds getTimeInMS() {
            auto f = frequency;
            switch (frequencyUnit) {
            case FrequencyUnit::hz:
                break;
            case FrequencyUnit::MHz:
                f *= 10e6;
                break;
            case FrequencyUnit::kHz:
                f *= 10e3;
                break;
            default:
                throw std::runtime_error("Unhandled clock frequency");
                break;
            }

            return SimDelayMilliSeconds((int)((1.0 / f) * 1000));
        }

        float dutyCycle = 0.50;
        FrequencyUnit frequencyUnit = FrequencyUnit::hz;
        float frequency = 1.f;
    };

    inline void to_json(nlohmann::json &j, const ClockComponent &comp) {
        j["frequency"] = comp.frequency;
        j["frequencyUnit"] = (int)comp.frequencyUnit;
    }

    inline void from_json(const nlohmann::json &j, ClockComponent &comp) {
        comp.frequencyUnit = j.at("frequencyUnit").get<SimEngine::FrequencyUnit>();
        comp.frequency = j.at("frequency").get<float>();
    }

    struct DigitalComponent {
        DigitalComponent() = default;
        DigitalComponent(const DigitalComponent &) = default;
        DigitalComponent(ComponentType type, int inputPinsCount, int outputPinsCount, SimDelayMilliSeconds delay) {
            this->type = type;
            this->delay = delay;
            this->inputPins = std::vector<std::vector<std::pair<UUID, int>>>(inputPinsCount, std::vector<std::pair<UUID, int>>());
            this->outputPins = std::vector<std::vector<std::pair<UUID, int>>>(outputPinsCount, std::vector<std::pair<UUID, int>>());
            this->outputStates = std::vector<bool>(outputPinsCount, false);
            this->inputStates = std::vector<bool>(inputPinsCount, false);
        }

        ComponentType type;
        SimDelayMilliSeconds delay;
        std::vector<std::vector<std::pair<UUID, int>>> inputPins;
        std::vector<std::vector<std::pair<UUID, int>>> outputPins;
        std::vector<bool> outputStates;
        std::vector<bool> inputStates;
    };
} // namespace Bess::SimEngine
