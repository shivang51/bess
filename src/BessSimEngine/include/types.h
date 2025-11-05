#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include <any>
#include <chrono>
#include <entt/entt.hpp>
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

    enum class PinType {
        input,
        output
    };

    enum class ExtendedPinType {
        none = -1,
        inputClock,
        inputClear
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

    struct BESS_API PinDetail {
        PinType type;
        std::string name = "";
        ExtendedPinType extendedType = ExtendedPinType::none;
    };

    inline bool operator==(const Bess::SimEngine::PinDetail &a, const Bess::SimEngine::PinDetail &b) noexcept {
        return a.type == b.type && a.name == b.name && a.extendedType == b.extendedType;
    }

    struct BESS_API PinState {
        LogicState state = LogicState::low;
        SimTime lastChangeTime{0};

        PinState() = default;

        PinState(LogicState state, SimTime time) {
            this->state = state;
            lastChangeTime = time;
        }

        PinState(bool value) {
            state = value ? LogicState::high : LogicState::low;
        }

        explicit operator bool() const {
            return state == LogicState::high;
        }

        PinState &operator=(const bool &val) {
            this->state = val ? LogicState::high : LogicState::low;
            return *this;
        }
    };

    struct BESS_API SimulationEvent {
        SimTime simTime;
        entt::entity entity;
        entt::entity schedulerEntity; // enitity that triggered the change
        uint64_t id;
        bool operator<(const SimulationEvent &other) const noexcept {
            if (simTime != other.simTime)
                return simTime < other.simTime;
            return id < other.id;
        }
    };

    struct BESS_API ComponentState {
        std::vector<PinState> inputStates;
        std::vector<bool> inputConnected;
        std::vector<PinState> outputStates;
        std::vector<bool> outputConnected;
        bool isChanged = false;
        std::any *auxData = nullptr;
        bool simError = false;
        std::string errorMessage;
    };

    typedef std::function<ComponentState(const std::vector<PinState> &, SimTime, const ComponentState &)> SimulationFunction;
} // namespace Bess::SimEngine
