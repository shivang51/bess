#pragma once
#include "bess_core/style/bess_theme.h"
#include <cstdint>

namespace Bess {
    class Window;
}

namespace Bess::Events {
    struct WindowResizeEvent {
        uint32_t width;
        uint32_t height;
    };

    struct FileDropEvent {
        Window *window;
        std::vector<std::string> files;
    };

    struct ThemeChangeEvent {
        bool isDarkMode = false;
        std::shared_ptr<Core::Style::BessTheme> theme = nullptr;
    };
} // namespace Bess::Events
