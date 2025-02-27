#pragma once

#include "component_catalog.h"
#include <chrono>
#include <entt/entt.hpp>
#include <utility>
#include <vector>

namespace Bess::SimEngine {
    struct GateComponent {
        ComponentType type;
        std::chrono::milliseconds delay;
        std::vector<std::vector<std::pair<entt::entity, int>>> inputPins;
        std::vector<std::vector<std::pair<entt::entity, int>>> outputPins;
        std::vector<bool> outputStates;
    };
} // namespace Bess::SimEngine
