#pragma once

#include "bess_api.h"
#include "bess_uuid.h"
#include <chrono>
#include <entt/entt.hpp>
#include <functional>

namespace Bess::SimEngine {
    typedef std::chrono::milliseconds SimDelayMilliSeconds;
    typedef std::chrono::milliseconds SimTime;
    typedef std::chrono::seconds SimDelaySeconds;

    typedef std::vector<std::vector<std::pair<UUID, int>>> Connections;

    struct BESS_API ConnectionBundle {
        Connections inputs;
        Connections outputs;
    };

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

    enum class SimulationState {
        running,
        paused
    };

    struct BESS_API SimulationEvent {
        std::chrono::milliseconds simTime;
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
        const std::vector<bool>& inputStates;
        const std::vector<bool>& inputConnected;
        const std::vector<bool>& outputStates;
        const std::vector<bool>& outputConnected;
    };

} // namespace Bess::SimEngine
