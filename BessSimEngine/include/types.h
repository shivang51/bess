#pragma once

#include "bess_uuid.h"
#include <chrono>
#include <entt/entt.hpp>
#include <functional>

namespace Bess::SimEngine {
    typedef std::chrono::milliseconds SimDelayMilliSeconds;
    typedef std::chrono::seconds SimDelaySeconds;
    typedef std::function<bool(entt::registry &, entt::entity, std::function<entt::entity(const UUID &)>)> SimulationFunction;
} // namespace Bess::SimEngine
