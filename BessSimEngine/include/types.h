#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include <chrono>
#include <entt/entt.hpp>
#include <functional>

namespace Bess::SimEngine {
    typedef std::chrono::milliseconds SimDelayMilliSeconds;
    typedef std::chrono::seconds SimDelaySeconds;
    using TimePoint = std::chrono::steady_clock::time_point;

    // registry, entity, inputs, function to convert uuid to entity
    typedef std::function<bool(entt::registry &, entt::entity, const std::vector<bool> &, std::function<entt::entity(const UUID &)>)> SimulationFunction;

    enum class PinType {
        input,
        output
    };

    enum class FrequencyUnit {
        hz,
        kHz,
        MHz
    };

    // Represents a scheduled simulation event.
    struct BESS_API SimulationEvent {
        TimePoint time;
        entt::entity entity;
        uint64_t id;

        bool operator<(SimulationEvent const &other) const noexcept {
            if (time != other.time)
                return time < other.time;
            return id < other.id;
        }
    };

    struct BESS_API ComponentState {
        std::vector<bool> inputStates;
        std::vector<bool> outputStates;
    };

} // namespace Bess::SimEngine
