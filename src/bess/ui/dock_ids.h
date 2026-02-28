#pragma once

#include <cstdint>
namespace Bess::UI {

    enum class Dock : uint8_t {
        none,
        left,
        right,
        top,
        bottom,
        main
    };

} // namespace Bess::UI
