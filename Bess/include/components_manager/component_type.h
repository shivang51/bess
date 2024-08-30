#pragma once

namespace Bess::Simulator {
    enum class ComponentType {
        inputSlot = 0,
        outputSlot,
        connection,
        button,
        //draggable compoenents start below
        jcomponent = 101,
        inputProbe,
        outputProbe,
        text,
        connectionPoint,
        clock,
    };

    enum class FrequencyUnit {
        hertz,
        milliHertz
    };
}
