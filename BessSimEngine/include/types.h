#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include <chrono>
#include <entt/entt.hpp>
#include <functional>

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

    struct BESS_API PinDetails {
        PinType type;
        std::string name = "";
        ExtendedPinType extendedType = ExtendedPinType::none;
    };

    struct BESS_API PinState {
        LogicState state = LogicState::low;
        SimTime lastChangeTime{0};

        PinState() = default;

        PinState(const LogicState state, const SimTime time) {
            this->state = state;
            lastChangeTime = time;
        }

        PinState(const bool value) {
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

    typedef std::function<bool(entt::registry &, entt::entity, const std::vector<PinState> &, SimTime, std::function<entt::entity(const UUID &)>)> SimulationFunction;

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
        const std::vector<PinState> &inputStates;
        const std::vector<bool> &inputConnected;
        const std::vector<PinState> &outputStates;
        const std::vector<bool> &outputConnected;
        const void *auxData;
    };

} // namespace Bess::SimEngine
