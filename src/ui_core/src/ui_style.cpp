#include "ui_style.h"

#include "bess_core/style/bess_theme.h"

namespace Bess::UI {
    UITheme UITheme::fromBessTheme(const Core::Style::BessTheme &bessTheme) {
        const auto &colors = bessTheme.getColorScheme().getColors();

        // UICore owns component geometry, while BessTheme is the single
        // source of truth for color. Alpha variants deliberately retain the
        // RGB channels of their semantic BessTheme role.
        const auto transparent = colors.surface.withAlpha(0.f);
        const Core::Renderer::ShadowProps noShadow{
            .enabled = false,
            .color = colors.shadow.withAlpha(0.f),
        };
        const glm::vec4 controlRadius{6.f};
        const glm::vec4 compactItemRadius{5.f};

        UITheme theme;
        theme.canvas = colors.surface;
        theme.surface = {
            .background = colors.surfaceContainerLow,
            .border = transparent,
            .cornerRadius = controlRadius,
            .borderThickness = glm::vec4{0.f},
            .shadow = noShadow,
        };
        theme.panel = theme.surface;
        theme.panel.border = colors.outlineVariant;
        theme.panel.borderThickness = glm::vec4{1.f};
        theme.label = {.color = colors.onSurface, .fontSize = 14.f};

        theme.button.normal = {
            .background = colors.surfaceContainerHigh,
            .border = colors.outlineVariant,
            .cornerRadius = controlRadius,
            .borderThickness = glm::vec4{1.f},
            .shadow = noShadow,
        };
        theme.button.hovered = theme.button.normal;
        theme.button.hovered.background = colors.surfaceContainerHighest;
        theme.button.pressed = theme.button.normal;
        theme.button.pressed.background = colors.surfaceContainer;
        theme.button.focused = theme.button.hovered;
        theme.button.focused.border = colors.primary;
        theme.button.focused.borderThickness = glm::vec4{2.f};
        theme.button.disabled = theme.button.normal;
        theme.button.disabled.background = colors.surfaceContainerLow;
        theme.button.disabled.border = colors.outlineVariant.withAlpha(0.45f);
        theme.button.text = theme.label;
        theme.button.disabledText = colors.onSurface.withAlpha(0.38f);
        theme.button.minimumSize = {0.f, 26.f};
        theme.button.contentPadding = {8.f, 4.f};

        theme.tabs.strip = {
            .background = colors.surfaceContainerLowest,
            .border = transparent,
            .shadow = noShadow,
        };
        theme.tabs.normal = {
            .background = transparent,
            .border = transparent,
            .shadow = noShadow,
        };
        theme.tabs.hovered = theme.tabs.normal;
        theme.tabs.hovered.background = colors.surfaceContainerHigh;
        theme.tabs.hovered.cornerRadius = compactItemRadius;
        theme.tabs.active = {
            .background = colors.surfaceContainerHighest,
            .border = colors.outlineVariant,
            .cornerRadius = compactItemRadius,
            .borderThickness = glm::vec4{1.f},
            .shadow = noShadow,
        };
        theme.tabs.pressed = theme.tabs.hovered;
        theme.tabs.pressed.background = colors.surfaceContainer;
        theme.tabs.closeHovered = {
            .background = colors.onSurface.withAlpha(0.10f),
            .border = transparent,
            .cornerRadius = glm::vec4{9.f},
            .shadow = noShadow,
        };
        theme.tabs.closePressed = theme.tabs.closeHovered;
        theme.tabs.closePressed.background = colors.onSurface.withAlpha(0.18f);
        theme.tabs.text = theme.label;
        theme.tabs.text.fontSize = 13.f;
        theme.tabs.inactiveText = colors.onSurfaceVariant;
        theme.tabs.closeIcon = colors.onSurfaceVariant;
        theme.tabs.closeIconHovered = colors.onSurface;
        theme.tabs.height = 28.f;
        theme.tabs.minimumWidth = 78.f;
        theme.tabs.horizontalPadding = 10.f;
        theme.tabs.stripPadding = {3.f, 2.f};
        theme.tabs.gap = 2.f;
        theme.tabs.closeButtonSize = 18.f;
        theme.tabs.closeIconSize = 10.f;
        theme.tabs.closeButtonGap = 4.f;
        theme.tabs.closeButtonTrailingPadding = 5.f;

        theme.menus.bar = {
            .background = colors.surfaceContainerLowest,
            .border = colors.outlineVariant,
            .borderThickness = {0.f, 0.f, 1.f, 0.f},
            .shadow = noShadow,
        };
        theme.menus.barItem = {
            .background = transparent,
            .border = transparent,
            .cornerRadius = compactItemRadius,
            .shadow = noShadow,
        };
        theme.menus.barItemHovered = theme.menus.barItem;
        theme.menus.barItemHovered.background = colors.surfaceContainerHigh;
        theme.menus.barItemActive = theme.menus.barItem;
        theme.menus.barItemActive.background = colors.surfaceContainerHighest;
        theme.menus.popup = {
            .background = colors.surfaceContainerLow.withAlpha(0.98f),
            .border = transparent,
            .cornerRadius = glm::vec4{7.f},
            .borderThickness = glm::vec4{0.f},
            .shadow =
                Core::Renderer::ShadowProps{
                    .enabled = true,
                    .offset = {0.f, 5.f},
                    .blur = 14.f,
                    .spread = -1.f,
                    .color = colors.shadow.withAlpha(0.45f),
                },
        };
        theme.menus.itemHovered = {
            .background = colors.surfaceContainerHighest,
            .border = transparent,
            .cornerRadius = compactItemRadius,
            .shadow = noShadow,
        };
        theme.menus.itemPressed = theme.menus.itemHovered;
        theme.menus.itemPressed.background = colors.surfaceContainer;
        // Tabs, menu-bar entries, and popup menu items share one compact
        // typography scale. Deriving the menu styles from the tab style keeps
        // them synchronized when that scale changes.
        theme.menus.barText = theme.tabs.text;
        theme.menus.text = theme.tabs.text;
        theme.menus.iconColor = colors.onSurfaceVariant;
        theme.menus.shortcutColor = colors.onSurfaceVariant.withAlpha(0.78f);
        theme.menus.disabledText = colors.onSurface.withAlpha(0.38f);
        theme.menus.separator = colors.outlineVariant;
        theme.menus.barHeight = 22.f;
        theme.menus.barVerticalMargin = 2.f;
        theme.menus.barHorizontalPadding = 6.f;
        theme.menus.submenuChevronSize = 11.f;

        theme.dock.background = {
            .background = colors.surface,
            .border = transparent,
            .shadow = noShadow,
        };
        theme.dock.stack = theme.panel;
        theme.dock.stack.cornerRadius = glm::vec4{7.f};
        theme.dock.floatingWindow = {
            .background = colors.surfaceContainerLow,
            .border = colors.outline,
            .cornerRadius = glm::vec4{9.f},
            .borderThickness = glm::vec4{1.f},
            .shadow =
                Core::Renderer::ShadowProps{
                    .enabled = true,
                    .offset = {0.f, 7.f},
                    .blur = 16.f,
                    .spread = -1.f,
                    .color = colors.shadow.withAlpha(0.47f),
                },
        };
        theme.dock.floatingStack = theme.dock.stack;
        theme.dock.floatingStack.background =
            theme.dock.floatingWindow.background;
        theme.dock.floatingStack.border = transparent;
        theme.dock.floatingStack.borderThickness = glm::vec4{0.f};
        // The content joins the square lower edge of the title bar. Bottom
        // radii are retained only for stacks that touch the corresponding
        // outer window corners (see DockSpace::paint).
        theme.dock.floatingStack.cornerRadius = {0.f, 0.f, 8.f, 8.f};
        theme.dock.floatingStack.shadow.enabled = false;
        theme.dock.floatingHeader = theme.tabs.active;
        // top-left, top-right, bottom-right, bottom-left
        theme.dock.floatingHeader.cornerRadius = {8.f, 8.f, 0.f, 0.f};
        theme.dock.floatingTitleBarHeight = 24.f;
        theme.dock.floatingTitleHorizontalPadding = 8.f;
        theme.dock.splitter = colors.outlineVariant;
        theme.dock.splitterHovered = colors.primary;
        theme.dock.dropGuide = {
            .background = colors.primaryContainer.withAlpha(0.92f),
            .border = colors.primary.withAlpha(0.86f),
            .cornerRadius = glm::vec4{8.f},
            .borderThickness = glm::vec4{1.f},
            .shadow = noShadow,
        };
        theme.dock.dropGuideHovered = theme.dock.dropGuide;
        theme.dock.dropGuideHovered.background =
            colors.primary.withAlpha(0.96f);
        theme.dock.dropGuideHovered.border = colors.onPrimary;
        theme.dock.dropPreview = {
            .background = colors.primary.withAlpha(0.30f),
            .border = colors.primary.withAlpha(0.86f),
            .cornerRadius = glm::vec4{7.f},
            .borderThickness = glm::vec4{2.f},
            .shadow = noShadow,
        };
        return theme;
    }

    UITheme UITheme::dark() {
        return fromBessTheme(*Core::Style::BessTheme::defaultDarkTheme());
    }
} // namespace Bess::UI
