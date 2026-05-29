#pragma once
#include <cstdint>
namespace Bess::Events {
    struct WindowResizeEvent {
        uint32_t width;
        uint32_t height;
    };
} // namespace Bess::Events
