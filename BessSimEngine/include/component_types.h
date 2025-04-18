#pragma once

namespace Bess::SimEngine {
    enum class ComponentType : int {
        EMPTY = -1,
        INPUT,
        OUTPUT,
        AND,
        OR,
        NOT,
        NOR,
        NAND,
        XOR,
        XNOR,
        FLIP_FLOP_JK = 20,
        FLIP_FLOP_SR,
        FLIP_FLOP_D,
        FLIP_FLOP_T
    };

    enum FlipFlopType : int {
        FLIP_FLOP_JK = 20,
        FLIP_FLOP_SR,
        FLIP_FLOP_D,
        FLIP_FLOP_T
    };
} // namespace Bess::SimEngine
