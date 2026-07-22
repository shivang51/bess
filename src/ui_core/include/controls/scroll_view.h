#pragma once

#include "ui_style.h"
#include "widget.h"

#include <cstdint>
#include <optional>

namespace Bess::UI {

    struct ScrollViewOptions {
        bool horizontal = true;
        bool vertical = true;
        bool clipContent = true;
        std::optional<UIScrollStyle> style;
    };

    // A single-content viewport with automatic, space-reserving scrollbars.
    // Put a FlexContainer (or another composite widget) beneath it when the
    // scrollable content consists of multiple controls.
    class BESS_API ScrollView : public Widget {
      public:
        explicit ScrollView(ScrollViewOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void arrange(WidgetArrangeContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        [[nodiscard]] WidgetBounds
        childClipBounds(WidgetBounds bounds) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] glm::vec2 scrollOffset() const noexcept;
        [[nodiscard]] glm::vec2 maximumScrollOffset() const noexcept;
        [[nodiscard]] glm::vec2 contentExtent() const noexcept;
        [[nodiscard]] WidgetBounds viewportBounds() const noexcept;
        [[nodiscard]] bool hasHorizontalScrollbar() const noexcept;
        [[nodiscard]] bool hasVerticalScrollbar() const noexcept;

        // Callers mutating a mounted ScrollView directly should invalidate
        // layout and paint through WidgetTree::mutateWidget/WidgetRef.
        bool setScrollOffset(glm::vec2 offset) noexcept;

      private:
        enum class Scrollbar : uint8_t { none, horizontal, vertical };

        struct BarGeometry {
            bool visible = false;
            WidgetBounds track;
            WidgetBounds thumb;
        };

        [[nodiscard]] const UIScrollStyle &
        resolvedStyle(const WidgetTree &state) const noexcept;
        [[nodiscard]] glm::vec2
        requiredContentExtent(const WidgetTree &state,
                              WidgetId content,
                              WidgetBounds viewport,
                              bool includeRootOverflow) const noexcept;
        void resolveGeometry(WidgetBounds bounds,
                             glm::vec2 requiredExtent,
                             const UIScrollStyle &style,
                             bool horizontal,
                             bool vertical) noexcept;
        [[nodiscard]] Scrollbar scrollbarAt(glm::vec2 position) const noexcept;
        bool updateHoveredScrollbar(glm::vec2 position) noexcept;
        void beginScrollbarDrag(Scrollbar bar, glm::vec2 position) noexcept;
        bool updateScrollbarDrag(glm::vec2 position) noexcept;
        [[nodiscard]] float coordinate(Scrollbar bar,
                                       glm::vec2 position) const noexcept;
        [[nodiscard]] const BarGeometry &geometry(Scrollbar bar) const noexcept;

        ScrollViewOptions m_options;
        WidgetId m_content;
        WidgetBounds m_viewport;
        glm::vec2 m_scrollOffset{0.f, 0.f};
        glm::vec2 m_maximumScrollOffset{0.f, 0.f};
        glm::vec2 m_contentExtent{0.f, 0.f};
        BarGeometry m_horizontalBar;
        BarGeometry m_verticalBar;
        Scrollbar m_hoveredScrollbar = Scrollbar::none;
        Scrollbar m_draggedScrollbar = Scrollbar::none;
        float m_thumbGrabOffset = 0.f;
    };

} // namespace Bess::UI
