#pragma once
#include "bess_core/style/bess_theme.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Bess {
    class Window;
}

namespace Bess::Events {
    struct WindowResizeEvent {
        uint32_t width;
        uint32_t height;
    };

    struct WindowDropPayload {
        std::string requestedMimeType;
        std::string mimeType;
        std::string data;
        std::vector<std::string> paths;
        int formatBits = 0;
    };

    struct WindowDropEvent {
        Window *window = nullptr;
        std::shared_ptr<const WindowDropPayload> payload;
        int x = 0;
        int y = 0;
    };

    struct ThemeChangeEvent {
        bool isDarkMode = false;
        std::shared_ptr<Core::Style::BessTheme> theme = nullptr;
    };
} // namespace Bess::Events
