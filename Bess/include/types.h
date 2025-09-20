#pragma once

#include <chrono>

namespace Bess {
    using TFrameTime = std::chrono::duration<double, std::milli>;
    using TAnimationTime = std::chrono::duration<double, std::milli>;
}; // namespace Bess
