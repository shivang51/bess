#pragma once
#include "bess_core/style/bess_theme.h"
#include <cstdint>
namespace Bess::Events {
    struct WindowResizeEvent {
        uint32_t width;
        uint32_t height;
    };

    struct ThemeChangeEvent {
        bool isDarkMode = false;
        std::shared_ptr<Core::Style::BessTheme> theme = nullptr;
    };
} // namespace Bess::Events
