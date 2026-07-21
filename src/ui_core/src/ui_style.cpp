#include "ui_style.h"

namespace Bess::UI {
    UITheme UITheme::dark() {
        using Color = Core::Renderer::Color;

        const auto transparent = Color::fromRGBA8(0, 0, 0, 0);
        const auto surface = Color::fromRGBA8(38, 40, 48);
        const auto raised = Color::fromRGBA8(48, 51, 61);
        const auto hover = Color::fromRGBA8(59, 64, 77);
        const auto pressed = Color::fromRGBA8(42, 45, 55);
        const auto border = Color::fromRGBA8(76, 80, 94);
        const auto accent = Color::fromRGBA8(91, 141, 239);
        const auto text = Color::fromRGBA8(238, 240, 245);
        const glm::vec4 radius{5.f};

        UITheme theme;
        theme.panel = {
            .background = surface,
            .border = border,
            .cornerRadius = radius,
            .borderThickness = glm::vec4{1.f},
        };
        theme.label = {.color = text, .fontSize = 14.f};

        theme.button.normal = {
            .background = raised,
            .border = border,
            .cornerRadius = radius,
            .borderThickness = glm::vec4{1.f},
        };
        theme.button.hovered = theme.button.normal;
        theme.button.hovered.background = hover;
        theme.button.pressed = theme.button.normal;
        theme.button.pressed.background = pressed;
        theme.button.focused = theme.button.hovered;
        theme.button.focused.border = accent;
        theme.button.focused.borderThickness = glm::vec4{2.f};
        theme.button.disabled = theme.button.normal;
        theme.button.disabled.background = Color::fromRGBA8(43, 45, 52);
        theme.button.disabled.border = Color::fromRGBA8(57, 59, 67);
        theme.button.text = theme.label;

        theme.tabs.strip = {
            .background = Color::fromRGBA8(31, 33, 40),
            .border = transparent,
        };
        theme.tabs.normal = {
            .background = transparent,
            .border = transparent,
        };
        theme.tabs.hovered = theme.tabs.normal;
        theme.tabs.hovered.background = hover;
        theme.tabs.active = {
            .background = surface,
            .border = accent,
            .borderThickness = glm::vec4{0.f, 0.f, 2.f, 0.f},
        };
        theme.tabs.pressed = theme.tabs.hovered;
        theme.tabs.pressed.background = pressed;
        theme.tabs.text = theme.label;

        theme.dock.background = {
            .background = Color::fromRGBA8(25, 27, 33),
            .border = transparent,
        };
        theme.dock.stack = theme.panel;
        return theme;
    }
} // namespace Bess::UI
