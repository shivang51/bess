#pragma once

#include <chrono>
#include <ratio>

namespace Bess {
    using TimeMs = std::chrono::duration<double, std::milli>;
    using TimeNs = std::chrono::duration<double, std::nano>;
}; // namespace Bess
