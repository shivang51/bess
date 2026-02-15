#pragma once
#include "bess_json/bess_json.h"
#include <cstdint>

namespace Bess::Canvas {
    enum class SceneComponentTypeFlag : uint8_t {
        showInProjectExplorer = 1 << 7,
    };

    enum class SceneComponentType : int8_t {
        _base = -1,
        simulation = 0 | (int8_t)SceneComponentTypeFlag::showInProjectExplorer,
        slot = 1,
        nonSimulation = 2 | (int8_t)SceneComponentTypeFlag::showInProjectExplorer,
        connection = 3,
        connJoint = 4,
    };
} // namespace Bess::Canvas

REFLECT_ENUM(Bess::Canvas::SceneComponentType);
