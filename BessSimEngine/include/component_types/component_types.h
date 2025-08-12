#pragma once

namespace Bess::SimEngine {
    enum class ComponentType : int {
        EMPTY = -1,
#define COMPONENT(name, value) name value,
#include "component_types.def"
#undef COMPONENT
    };

    enum FlipFlopType : int {
        FLIP_FLOP_JK = 20,
        FLIP_FLOP_SR,
        FLIP_FLOP_D,
        FLIP_FLOP_T
    };
} // namespace Bess::SimEngine
