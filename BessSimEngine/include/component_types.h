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
        FLIP_FLOP_T,
        FULL_ADDER,
        HALF_ADDER,
        MULTIPLEXER_4_1,
        DECODER_2_4,
        DEMUX_1_4,
        ENCODER_4_2,
        HALF_SUBTRACTOR,
        MULTIPLEXER_2_1,
        COMPARATOR_1_BIT,
        PRIORITY_ENCODER_4_2,
        FULL_SUBTRACTOR,
        COMPARATOR_2_BIT,
        SEVEN_SEG_DISPLAY,
        SEVEN_SEG_DISPLAY_DRIVER,
        STATE_MONITOR
    };

    enum FlipFlopBound : int {
        start = (int)ComponentType::FLIP_FLOP_JK,
        end = (int)ComponentType::FLIP_FLOP_T
    };
} // namespace Bess::SimEngine
