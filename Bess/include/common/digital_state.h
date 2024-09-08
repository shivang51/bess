#pragma once

namespace Bess::Simulator {
    enum DigitalState {
        low = 0,
        high = 1
    };

    inline DigitalState operator!(DigitalState state) {
        return state == DigitalState::low ? DigitalState::high : DigitalState::low;
    }
} // namespace Bess::Simulator
