#pragma once

namespace Bess::Simulator {
    enum class ComponentType {
        inputSlot = 0,
        outputSlot,
        connection,
        //draggable compoenents start below
        jcomponent = 101,
        inputProbe,
        outputProbe,
    };
}
