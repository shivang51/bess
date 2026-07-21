#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "common/bess_api.h"
#include "ext/vector_float2.hpp"
#include "ext/vector_float4.hpp"

namespace Bess::UI {

    struct UIBoxStyle {
        Core::Renderer::Color background{0.f, 0.f, 0.f, 0.f};
        Core::Renderer::Color border{0.f, 0.f, 0.f, 0.f};
        glm::vec4 cornerRadius{0.f};
        glm::vec4 borderThickness{0.f};
        Core::Renderer::ShadowProps shadow{};
    };

    struct UITextStyle {
        Core::Renderer::Color color{1.f, 1.f, 1.f, 1.f};
        float fontSize = 14.f;
        float letterSpacing = 0.f;
    };

    struct UIInteractiveStyle {
        UIBoxStyle normal;
        UIBoxStyle hovered;
        UIBoxStyle pressed;
        UIBoxStyle focused;
        UIBoxStyle disabled;
        UITextStyle text;
        Core::Renderer::Color disabledText{0.55f, 0.57f, 0.62f, 1.f};
        glm::vec2 minimumSize{72.f, 32.f};
        glm::vec2 contentPadding{12.f, 7.f};
    };

    struct UITabStyle {
        UIBoxStyle strip;
        UIBoxStyle normal;
        UIBoxStyle hovered;
        UIBoxStyle active;
        UIBoxStyle pressed;
        UITextStyle text;
        Core::Renderer::Color inactiveText{0.72f, 0.74f, 0.78f, 1.f};
        float height = 28.f;
        float minimumWidth = 72.f;
        float maximumWidth = 220.f;
        float horizontalPadding = 12.f;
        // Space around the strip and between neighboring tabs. Keeping these
        // in the theme lets standalone and docked tab bars share identical
        // geometry as well as paint.
        glm::vec2 stripPadding{0.f};
        float gap = 0.f;
    };

    struct UIMenuStyle {
        UIBoxStyle bar;
        UIBoxStyle barItem;
        UIBoxStyle barItemHovered;
        UIBoxStyle barItemActive;
        UIBoxStyle popup;
        UIBoxStyle itemHovered;
        UIBoxStyle itemPressed;
        UITextStyle barText;
        UITextStyle text;
        Core::Renderer::Color iconColor{0.82f, 0.84f, 0.88f, 1.f};
        Core::Renderer::Color shortcutColor{0.62f, 0.65f, 0.70f, 1.f};
        Core::Renderer::Color disabledText{0.42f, 0.44f, 0.48f, 1.f};
        Core::Renderer::Color separator{0.25f, 0.27f, 0.31f, 1.f};
        float barHeight = 22.f;
        float barVerticalMargin = 2.f;
        float barHorizontalPadding = 6.f;
        float popupMinimumWidth = 190.f;
        float popupMaximumWidth = 420.f;
        float popupPadding = 4.f;
        float popupOverlap = 2.f;
        float itemHeight = 26.f;
        float separatorHeight = 9.f;
        float itemHorizontalPadding = 8.f;
        float iconColumnWidth = 20.f;
        float shortcutGap = 24.f;
        float submenuIndicatorWidth = 16.f;
        float submenuChevronSize = 11.f;
    };

    struct UIDockStyle {
        UIBoxStyle background;
        UIBoxStyle stack;
        // Stack chrome used inside an already-framed floating host. It should
        // not draw a second rounded panel beneath the host title bar.
        UIBoxStyle floatingStack;
        UIBoxStyle floatingWindow;
        UIBoxStyle floatingHeader;
        UIBoxStyle dropGuide;
        UIBoxStyle dropGuideHovered;
        UIBoxStyle dropPreview;
        Core::Renderer::Color splitter{0.18f, 0.19f, 0.23f, 1.f};
        Core::Renderer::Color splitterHovered{0.32f, 0.55f, 0.95f, 1.f};
        float splitterThickness = 4.f;
        glm::vec2 floatingMinimumSize{260.f, 180.f};
        glm::vec2 floatingMaximumSize{640.f, 480.f};
        float floatingMargin = 8.f;
        float floatingTitleBarHeight = 24.f;
        float floatingTitleHorizontalPadding = 8.f;
        float floatingVisibleTitleWidth = 48.f;
        float floatingVisibleTitleHeight = 8.f;
        float dragThreshold = 6.f;
        float dropGuideSize = 38.f;
        float dropGuideGap = 7.f;
        float dropPreviewInset = 5.f;
    };

    struct BESS_API UITheme {
        UIBoxStyle panel;
        UITextStyle label;
        UIInteractiveStyle button;
        UITabStyle tabs;
        UIMenuStyle menus;
        UIDockStyle dock;

        [[nodiscard]] static UITheme dark();
    };

} // namespace Bess::UI
