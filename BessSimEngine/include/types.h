#pragma once

#include <chrono>
#include <entt/entt.hpp>
#include <functional>

namespace Bess::SimEngine {
    typedef std::chrono::milliseconds SimDelayMilliSeconds;
    typedef std::chrono::seconds SimDelaySeconds;
    typedef std::function<bool(entt::registry &, entt::entity)> SimulationFunction;
} // namespace Bess::SimEngine
