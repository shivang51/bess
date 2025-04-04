#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include <chrono>
#include <entt/entt.hpp>
#include <functional>

namespace Bess::SimEngine {
    typedef std::chrono::milliseconds SimDelayMilliSeconds;
    typedef std::chrono::seconds SimDelaySeconds;

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
        std::chrono::steady_clock::time_point time;
        entt::entity entity;
        // For the priority queue: earlier times have higher priority.
        bool operator<(const SimulationEvent &other) const;
    };

    struct BESS_API ComponentState {
        std::vector<bool> inputStates;
        std::vector<bool> outputStates;
    };

} // namespace Bess::SimEngine
