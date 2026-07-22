#pragma once

#include "bess_core/style/bess_theme.h"
#include "common/bess_api.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Bess {
    class Window;
}

namespace Bess::Events {
    struct BESS_API WindowResizeEvent {
        uint32_t width;
        uint32_t height;
    };

    struct BESS_API WindowDropPayload {
        std::string requestedMimeType;
        std::string mimeType;
        std::string data;
        std::vector<std::string> paths;
        int formatBits = 0;
    };

    struct BESS_API WindowDropEvent {
        Window *window = nullptr;
        std::shared_ptr<const WindowDropPayload> payload;
        int x = 0;
        int y = 0;
    };

    // Full native drag lifecycle. WindowDropEvent remains the compatibility
    // event for consumers interested only in committed drops; retained UI and
    // other interactive clients use this event to provide enter/move/leave
    // feedback before the payload is released.
    enum class WindowDragDropEventType : uint8_t {
        enter,
        move,
        leave,
        drop,
    };

    struct BESS_API WindowDragDropEvent {
        Window *window = nullptr;
        WindowDragDropEventType type = WindowDragDropEventType::enter;
        std::shared_ptr<const WindowDropPayload> payload;
        int x = 0;
        int y = 0;
        bool acceptedByPlatform = false;
    };

    struct BESS_API ThemeChangeEvent {
        bool isDarkMode = false;
        std::shared_ptr<Core::Style::BessTheme> theme = nullptr;
    };
} // namespace Bess::Events
