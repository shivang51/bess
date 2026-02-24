#pragma once

#include "imgui.h"
#include <cstdint>
#include <string>
#include <unordered_map>
namespace Bess::UI {

    enum class Dock : uint8_t {
        left,
        right,
        top,
        bottom,
        main
    };

} // namespace Bess::UI
