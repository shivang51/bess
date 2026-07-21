#include "ui_style.h"

namespace Bess::UI {
    UITheme UITheme::dark() {
        using Color = Core::Renderer::Color;

        const auto transparent = Color::fromRGBA8(0, 0, 0, 0);
        const auto surface = Color::fromRGBA8(40, 43, 48);
        const auto raised = Color::fromRGBA8(52, 56, 63);
        const auto hover = Color::fromRGBA8(65, 70, 78);
        const auto pressed = Color::fromRGBA8(43, 47, 53);
        const auto border = Color::fromRGBA8(72, 77, 86);
        const auto accent = Color::fromRGBA8(106, 153, 244);
        const auto text = Color::fromRGBA8(238, 240, 245);
        const glm::vec4 radius{6.f};

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
        theme.button.minimumSize = {0.f, 26.f};
        theme.button.contentPadding = {8.f, 4.f};

        theme.tabs.strip = {
            .background = Color::fromRGBA8(28, 32, 34),
            .border = transparent,
        };
        theme.tabs.normal = {
            .background = transparent,
            .border = transparent,
        };
        theme.tabs.hovered = theme.tabs.normal;
        theme.tabs.hovered.background = Color::fromRGBA8(48, 54, 57);
        theme.tabs.hovered.cornerRadius = glm::vec4{9.f};
        theme.tabs.active = {
            .background = Color::fromRGBA8(64, 78, 80),
            .border = Color::fromRGBA8(82, 96, 99),
            .cornerRadius = glm::vec4{9.f},
            .borderThickness = glm::vec4{1.f},
        };
        theme.tabs.pressed = theme.tabs.hovered;
        theme.tabs.pressed.background = pressed;
        theme.tabs.pressed.cornerRadius = glm::vec4{9.f};
        theme.tabs.text = theme.label;
        theme.tabs.height = 36.f;
        theme.tabs.minimumWidth = 78.f;
        theme.tabs.horizontalPadding = 12.f;
        theme.tabs.stripPadding = {4.f, 3.f};
        theme.tabs.gap = 2.f;

        theme.dock.background = {
            .background = Color::fromRGBA8(24, 27, 29),
            .border = transparent,
        };
        theme.dock.stack = theme.panel;
        theme.dock.stack.cornerRadius = glm::vec4{7.f};
        theme.dock.floatingWindow = {
            .background = Color::fromRGBA8(34, 38, 42),
            .border = Color::fromRGBA8(83, 90, 101),
            .cornerRadius = glm::vec4{9.f},
            .borderThickness = glm::vec4{1.f},
            .shadow =
                Core::Renderer::ShadowProps{
                    .enabled = true,
                    .offset = {0.f, 7.f},
                    .blur = 16.f,
                    .spread = -1.f,
                    .color = Color::fromRGBA8(0, 0, 0, 120),
                },
        };
        theme.dock.floatingHeader = theme.tabs.active;
        theme.dock.floatingHeader.cornerRadius = glm::vec4{8.f};
        theme.dock.dropGuide = {
            .background = Color::fromRGBA8(43, 49, 56, 235),
            .border = Color::fromRGBA8(112, 158, 244, 220),
            .cornerRadius = glm::vec4{8.f},
            .borderThickness = glm::vec4{1.f},
        };
        theme.dock.dropGuideHovered = theme.dock.dropGuide;
        theme.dock.dropGuideHovered.background =
            Color::fromRGBA8(82, 128, 218, 245);
        theme.dock.dropGuideHovered.border =
            Color::fromRGBA8(170, 202, 255, 255);
        theme.dock.dropPreview = {
            .background = Color::fromRGBA8(75, 126, 220, 78),
            .border = Color::fromRGBA8(126, 174, 255, 220),
            .cornerRadius = glm::vec4{7.f},
            .borderThickness = glm::vec4{2.f},
        };
        return theme;
    }
} // namespace Bess::UI
