#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include <any>
#include <chrono>
#include <functional>
#include <string>

namespace Bess::SimEngine {
    typedef std::chrono::nanoseconds SimTime;
    typedef std::chrono::seconds SimDelaySeconds;
    typedef std::chrono::nanoseconds SimDelayNanoSeconds;

    typedef std::pair<UUID, int> ComponentPin;

    typedef std::vector<std::vector<ComponentPin>> Connections;

    struct BESS_API ConnectionBundle {
        Connections inputs;
        Connections outputs;
    };

    enum class FrequencyUnit {
        hz,
        kHz,
        MHz
    };

    enum class SimulationState {
        running,
        paused
    };

    enum class LogicState { low,
                            high,
                            unknown,
                            high_z
    };

    enum class SlotsGroupType : uint8_t {
        none,
        input,
        output
    };

    enum class SlotCatergory : uint8_t {
        none,
        clock,
        clear,
        enable,
    };

    enum class ComponentBehaviorType : uint8_t {
        none,
        input,
        output
    };

    enum class SlotType : uint8_t {
        digitalInput,
        digitalOutput
    };

    struct BESS_API SlotState {
        LogicState state = LogicState::low;
        SimTime lastChangeTime{0};

        SlotState() = default;

        SlotState(LogicState state, SimTime time) {
            this->state = state;
            lastChangeTime = time;
        }

        SlotState(bool value) {
            state = value ? LogicState::high : LogicState::low;
        }

        explicit operator bool() const {
            return state == LogicState::high;
        }

        SlotState &operator=(const bool &val) {
            this->state = val ? LogicState::high : LogicState::low;
            return *this;
        }
    };

    struct BESS_API SimulationEvent {
        SimTime simTime;
        UUID compId;
        UUID schedulerId; // enitity that triggered the change
        uint64_t id;
        bool operator<(const SimulationEvent &other) const noexcept {
            if (simTime != other.simTime)
                return simTime < other.simTime;
            return id < other.id;
        }
    };

    struct BESS_API ComponentState {
        std::vector<SlotState> inputStates;
        std::vector<bool> inputConnected;
        std::vector<SlotState> outputStates;
        std::vector<bool> outputConnected;
        bool isChanged = false;
        std::any *auxData = nullptr;
        bool simError = false;
        std::string errorMessage;
    };

    typedef std::function<ComponentState(const std::vector<SlotState> &, SimTime, const ComponentState &)> SimulationFunction;

    struct BESS_API TruthTable {
        std::vector<std::vector<LogicState>> table;
        std::vector<UUID> inputUuids;
        std::vector<UUID> outputUuids;
    };
} // namespace Bess::SimEngine

namespace Bess::JsonConvert {
    BESS_API void toJsonValue(const Bess::SimEngine::SlotState &state, Json::Value &j);
    BESS_API void fromJsonValue(const Json::Value &j, Bess::SimEngine::SlotState &state);

    BESS_API void toJsonValue(const Bess::SimEngine::ComponentState &state, Json::Value &j);
    BESS_API void fromJsonValue(const Json::Value &j, Bess::SimEngine::ComponentState &state);

    BESS_API void toJsonValue(const Bess::SimEngine::Connections &connections, Json::Value &j);
    BESS_API void fromJsonValue(const Json::Value &j, Bess::SimEngine::Connections &connections);

    BESS_API void toJsonValue(const Bess::SimEngine::ComponentPin &pin, Json::Value &j);
    BESS_API void fromJsonValue(const Json::Value &j, Bess::SimEngine::ComponentPin &pin);
} // namespace Bess::JsonConvert
