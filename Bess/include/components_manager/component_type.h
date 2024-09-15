#pragma once

namespace Bess::Simulator {
    enum class ComponentType {
        none = -1,
        inputSlot = 0,
        outputSlot,
        connection,
        button,
        // draggable components start below
        jcomponent = 101,
        inputProbe,
        outputProbe,
        text,
        connectionPoint,
        clock,
        flipFlop
    };

    enum class FrequencyUnit {
        hertz,
        kiloHertz,
        megaHertz
    };
} // namespace Bess::Simulator
