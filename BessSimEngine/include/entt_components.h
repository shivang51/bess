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

    inline void to_json(nlohmann::json &j, const IdComponent &idComp) {
        j["uuid"] = static_cast<uint64_t>(idComp.uuid);
    }

    inline void from_json(const nlohmann::json &j, IdComponent &idComp) {
        idComp.uuid = UUID(j.at("uuid").get<uint64_t>());
    }

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

    inline void to_json(nlohmann::json &j, const DigitalComponent &comp) {
        j["type"] = static_cast<int>(comp.type);
        j["delay"] = comp.delay.count();
        j["inputPins"] = nlohmann::json::array();
        for(const auto &input : comp.inputPins) {
            nlohmann::json inputJson = nlohmann::json::array();
            for (const auto &[id, idx] : input) {
                inputJson.push_back({{"id", static_cast<uint64_t>(id)}, {"index", idx}});
            }
            j["inputPins"].push_back(inputJson);
        }
        j["outputPins"] = nlohmann::json::array();
        for(const auto &output : comp.outputPins) {
            nlohmann::json outputJson = nlohmann::json::array();
            for (const auto &[id, idx] : output) {
                outputJson.push_back({{"id", static_cast<uint64_t>(id)}, {"index", idx}});
            }
            j["outputPins"].push_back(outputJson);
        }
        j["outputStates"] = comp.outputStates;
        j["inputStates"] = comp.inputStates;
    }

    inline void from_json(const nlohmann::json &j, DigitalComponent &comp) {
        comp.type = static_cast<ComponentType>(j.at("type").get<int>());
        comp.delay = SimDelayMilliSeconds(j.at("delay").get<long long>());

        if (j.contains("inputPins")) {
            for (const auto &inputJson : j["inputPins"]) {
                std::vector<std::pair<UUID, int>> inputVec;
                for (const auto &input : inputJson) {
                    inputVec.emplace_back(static_cast<UUID>(input["id"].get<uint64_t>()), input["index"].get<int>());
                }
                comp.inputPins.push_back(inputVec);
            }
        }

        if (j.contains("outputPins")) {
            for (const auto &outputJson : j["outputPins"]) {
                std::vector<std::pair<UUID, int>> outputVec;
                for (const auto &output : outputJson) {
                    outputVec.emplace_back(static_cast<UUID>(output["id"].get<uint64_t>()), output["index"].get<int>());
                }
                comp.outputPins.push_back(outputVec);
            }
        }

        comp.outputStates = j.value("outputStates", std::vector<bool>());
        comp.inputStates = j.value("inputStates", std::vector<bool>());
    }
} // namespace Bess::SimEngine
