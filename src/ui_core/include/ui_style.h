#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "common/bess_api.h"
#include "ext/vector_float2.hpp"
#include "ext/vector_float4.hpp"

namespace Bess::Core::Style {
    class BessTheme;
}

namespace Bess::UI {

    struct UIBoxStyle {
        Core::Renderer::Color background{0.f, 0.f, 0.f, 0.f};
        Core::Renderer::Color border{0.f, 0.f, 0.f, 0.f};
        glm::vec4 cornerRadius{0.f};
        glm::vec4 borderThickness{0.f};
        Core::Renderer::ShadowProps shadow{};
    };

    struct UITextStyle {
        Core::Renderer::Color color{};
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
        Core::Renderer::Color disabledText{};
        glm::vec2 minimumSize{72.f, 32.f};
        glm::vec2 contentPadding{12.f, 7.f};
    };

    struct UITabStyle {
        UIBoxStyle strip;
        UIBoxStyle normal;
        UIBoxStyle hovered;
        UIBoxStyle active;
        UIBoxStyle pressed;
        UIBoxStyle closeHovered;
        UIBoxStyle closePressed;
        UITextStyle text;
        Core::Renderer::Color inactiveText{};
        Core::Renderer::Color closeIcon{};
        Core::Renderer::Color closeIconHovered{};
        float height = 28.f;
        float minimumWidth = 72.f;
        float maximumWidth = 220.f;
        float horizontalPadding = 12.f;
        // Space around the strip and between neighboring tabs. Keeping these
        // in the theme lets standalone and docked tab bars share identical
        // geometry as well as paint.
        glm::vec2 stripPadding{0.f};
        float gap = 0.f;
        float closeButtonSize = 18.f;
        float closeIconSize = 10.f;
        float closeButtonGap = 4.f;
        float closeButtonTrailingPadding = 5.f;
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
        Core::Renderer::Color iconColor{};
        Core::Renderer::Color shortcutColor{};
        Core::Renderer::Color disabledText{};
        Core::Renderer::Color separator{};
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

    struct UIScrollStyle {
        UIBoxStyle track;
        UIBoxStyle thumb;
        UIBoxStyle thumbHovered;
        UIBoxStyle thumbPressed;
        float thickness = 10.f;
        float margin = 2.f;
        float minimumThumbLength = 24.f;
        float wheelStep = 36.f;
    };

    struct UIPopupStyle {
        UIBoxStyle panel;
        float viewportMargin = 8.f;
    };

    struct UISelectionControlStyle {
        UIBoxStyle indicator;
        UIBoxStyle indicatorHovered;
        UIBoxStyle indicatorPressed;
        UIBoxStyle indicatorFocused;
        UIBoxStyle indicatorSelected;
        UIBoxStyle indicatorDisabled;
        UITextStyle text;
        Core::Renderer::Color disabledText{};
        Core::Renderer::Color mark{};
        Core::Renderer::Color disabledMark{};
        float indicatorSize = 16.f;
        float markSize = 10.f;
        float gap = 8.f;
        float minimumHeight = 24.f;
    };

    struct UIToggleStyle {
        UIBoxStyle track;
        UIBoxStyle trackHovered;
        UIBoxStyle trackPressed;
        UIBoxStyle trackSelected;
        UIBoxStyle trackDisabled;
        UIBoxStyle thumb;
        UIBoxStyle thumbSelected;
        glm::vec2 size{34.f, 18.f};
        float inset = 2.f;
    };

    struct UISliderStyle {
        UIBoxStyle track;
        UIBoxStyle fill;
        UIBoxStyle thumb;
        UIBoxStyle thumbHovered;
        UIBoxStyle thumbPressed;
        UIBoxStyle thumbFocused;
        UIBoxStyle disabledTrack;
        UIBoxStyle disabledThumb;
        float trackThickness = 4.f;
        float thumbSize = 14.f;
        float minimumLength = 96.f;
        float crossAxisSize = 24.f;
        double pageStepFactor = 10.0;
    };

    struct UIDropdownStyle {
        UIInteractiveStyle field;
        UITextStyle placeholder;
        Core::Renderer::Color chevron{};
        float minimumWidth = 140.f;
        float popupMaximumHeight = 280.f;
        float itemHeight = 26.f;
        float itemHorizontalPadding = 8.f;
        float iconColumnWidth = 20.f;
        float chevronSize = 11.f;
    };

    struct UITooltipStyle {
        UIBoxStyle panel;
        UITextStyle text;
        glm::vec2 padding{8.f, 5.f};
        float maximumWidth = 360.f;
        float delayMs = 450.f;
    };

    struct UITextInputStyle {
        UIBoxStyle normal;
        UIBoxStyle hovered;
        UIBoxStyle focused;
        UIBoxStyle disabled;
        UITextStyle text;
        UITextStyle placeholder;
        Core::Renderer::Color selection{};
        Core::Renderer::Color caret{};
        Core::Renderer::Color compositionUnderline{};
        glm::vec2 minimumSize{160.f, 28.f};
        glm::vec2 padding{8.f, 4.f};
        float caretWidth = 1.f;
        float compositionUnderlineThickness = 1.f;
        float blinkIntervalMs = 530.f;
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
        Core::Renderer::Color splitter{};
        Core::Renderer::Color splitterHovered{};
        // The full divider remains available for hit testing and becomes the
        // active visual while hovered or dragged. At rest, only a centered
        // line of splitterIdleThickness is painted.
        float splitterThickness = 4.f;
        float splitterIdleThickness = 1.f;
        glm::vec2 floatingMinimumSize{260.f, 180.f};
        // Non-positive components mean no theme-imposed maximum. The
        // DockSpace extent still provides a safe effective upper bound.
        glm::vec2 floatingMaximumSize{0.f, 0.f};
        float floatingMargin = 8.f;
        float floatingTitleBarHeight = 24.f;
        float floatingTitleHorizontalPadding = 8.f;
        float floatingVisibleTitleWidth = 48.f;
        float floatingVisibleTitleHeight = 8.f;
        // Resize targets are deliberately wider than the painted border.
        // Corners extend farther along both adjoining edges so diagonal
        // resizing remains easy without adding visible handles.
        float floatingResizeHandleThickness = 6.f;
        float floatingResizeCornerSize = 18.f;
        float dragThreshold = 6.f;
        float dropGuideSize = 38.f;
        float dropGuideGap = 7.f;
        float dropPreviewInset = 5.f;
    };

    struct BESS_API UITheme {
        Core::Renderer::Color canvas;
        UIBoxStyle surface;
        UIBoxStyle panel;
        UITextStyle label;
        UIInteractiveStyle button;
        UITabStyle tabs;
        UIMenuStyle menus;
        UIPopupStyle popup;
        UISelectionControlStyle checkbox;
        UISelectionControlStyle radio;
        UIToggleStyle toggle;
        UISliderStyle slider;
        UIDropdownStyle dropdown;
        UITooltipStyle tooltip;
        UITextInputStyle textBox;
        UIScrollStyle scroll;
        UIDockStyle dock;

        [[nodiscard]] static UITheme
        fromBessTheme(const Core::Style::BessTheme &theme);
        [[nodiscard]] static UITheme dark();
    };

} // namespace Bess::UI
