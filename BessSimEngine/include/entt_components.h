#pragma once

#include "bess_uuid.h"
#include "component_types.h"
#include "types.h"
#include <entt/entt.hpp>
#include <utility>
#include <vector>

namespace Bess::SimEngine {
    struct IdComponent {
        IdComponent() = default;
        IdComponent(UUID uuid) { this->uuid = uuid; }
        IdComponent(const IdComponent &) = default;
        UUID uuid;
    };

    struct GateComponent {
        GateComponent() = default;
        GateComponent(const GateComponent &) = default;
        GateComponent(ComponentType type, int inputPinsCount, int outputPinsCount, SimDelayMilliSeconds delay) {
            this->type = type;
            this->delay = delay;
            this->inputPins = std::vector<std::vector<std::pair<UUID, int>>>(inputPinsCount, std::vector<std::pair<UUID, int>>());
            this->outputPins = std::vector<std::vector<std::pair<UUID, int>>>(outputPinsCount, std::vector<std::pair<UUID, int>>());
            this->outputStates = std::vector<bool>(outputPinsCount, false);
        }

        ComponentType type;
        SimDelayMilliSeconds delay;
        std::vector<std::vector<std::pair<UUID, int>>> inputPins;
        std::vector<std::vector<std::pair<UUID, int>>> outputPins;
        std::vector<bool> outputStates;
    };
} // namespace Bess::SimEngine
