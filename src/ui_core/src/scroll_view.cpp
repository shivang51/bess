#include "controls/scroll_view.h"

#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Bess::UI {
    namespace {
        constexpr float kOverflowTolerance = 0.5f;
        constexpr float kScrollbarTrackZ = 0.010f;
        constexpr float kScrollbarThumbZ = 0.011f;

        float finiteMetric(float value,
                           float fallback,
                           float minimum,
                           float maximum) noexcept {
            return std::clamp(
                std::isfinite(value) ? value : fallback, minimum, maximum);
        }

        float nonNegative(float value) noexcept {
            return std::isfinite(value) ? std::max(0.f, value) : 0.f;
        }

        glm::vec2 nonNegative(glm::vec2 value) noexcept {
            return {nonNegative(value.x), nonNegative(value.y)};
        }

        BoxPaint boxPaint(WidgetBounds bounds,
                          const UIBoxStyle &style,
                          PickingId id,
                          float zIndex) {
            return {
                .bounds = bounds,
                .color = style.background,
                .borderColor = style.border,
                .cornerRadius = style.cornerRadius,
                .borderThickness = style.borderThickness,
                .shadow = style.shadow,
                .zIndex = zIndex,
                .pickingId = id,
            };
        }

        float axisLength(WidgetBounds bounds, bool horizontal) noexcept {
            return horizontal ? bounds.size.x : bounds.size.y;
        }

        float axisStart(WidgetBounds bounds, bool horizontal) noexcept {
            return horizontal ? bounds.topLeft().x : bounds.topLeft().y;
        }
    } // namespace

    ScrollView::ScrollView(ScrollViewOptions options)
        : m_options(std::move(options)) {
    }

    std::string_view ScrollView::typeName() const noexcept {
        return "ScrollView";
    }

    WidgetTraits ScrollView::traits() const noexcept {
        return {
            .focusable = false,
            .hitTestVisible = true,
            .clipChildren = m_options.clipContent,
        };
    }

    void ScrollView::onMount(WidgetMountContext &context) {
        context.layout.setWidthPercent(1.f);
        context.layout.setHeightPercent(1.f);
    }

    void ScrollView::arrange(WidgetArrangeContext &context) {
        const auto children = context.children();
        m_content = children.empty() ? WidgetId{} : children.front();
        for (size_t index = 0; index < children.size(); ++index) {
            // ScrollView intentionally has one content root. Keeping an
            // accidental second root out of paint and hit testing is safer
            // than letting it bypass the viewport's scroll transform.
            context.setChildVisible(children[index], index == 0);
        }

        if (!m_content) {
            m_viewport = context.bounds;
            m_scrollOffset = {};
            m_maximumScrollOffset = {};
            m_contentExtent = {};
            m_horizontalBar = {};
            m_verticalBar = {};
            m_hoveredScrollbar = Scrollbar::none;
            return;
        }

        const auto &style = resolvedStyle(context.state);
        // Measure intrinsic content only (mins, explicit point sizes, and
        // descendant extents). Never use the content root's previous arranged
        // size: ScrollView itself writes that size each frame, so reading it
        // back as "overflow" creates sticky scrollbars on empty/fit content.
        glm::vec2 required =
            requiredContentExtent(context.state, m_content, context.bounds);
        const glm::vec2 available = nonNegative(context.bounds.size);
        bool horizontal = m_options.horizontal &&
                          required.x > available.x + kOverflowTolerance;
        bool vertical =
            m_options.vertical && required.y > available.y + kOverflowTolerance;

        // Scrollbar gutters reduce the opposite viewport axis. Reflow before
        // deciding that the second bar is necessary: percentage dimensions,
        // flex spacers, and stretched children should adapt to that smaller
        // viewport instead of being mistaken for intrinsic overflow. Bars are
        // added monotonically during this solve, which prevents oscillation.
        for (int pass = 0; pass < 4; ++pass) {
            resolveGeometry(
                context.bounds, required, style, horizontal, vertical);
            const WidgetBounds contentBounds{
                .center = m_viewport.topLeft() - m_scrollOffset +
                          m_contentExtent * 0.5f,
                .size = m_contentExtent,
            };
            context.setChildBounds(m_content, contentBounds);

            const glm::vec2 reflowed =
                requiredContentExtent(context.state, m_content, m_viewport);
            // Never let a previous overflow latch keep an overstated extent
            // after content shrank (e.g. empty list). Re-measure each pass.
            required = reflowed;
            const bool nextHorizontal =
                m_options.horizontal &&
                required.x > m_viewport.size.x + kOverflowTolerance;
            const bool nextVertical =
                m_options.vertical &&
                required.y > m_viewport.size.y + kOverflowTolerance;
            if (nextHorizontal == horizontal && nextVertical == vertical) {
                break;
            }
            horizontal = nextHorizontal;
            vertical = nextVertical;
        }

        resolveGeometry(context.bounds, required, style, horizontal, vertical);

        // Drop gutters when the solved range is empty so a zero-travel bar
        // cannot reserve space or paint a stuck thumb.
        const bool hadHorizontal = horizontal;
        const bool hadVertical = vertical;
        if (horizontal && m_maximumScrollOffset.x <= kOverflowTolerance) {
            horizontal = false;
        }
        if (vertical && m_maximumScrollOffset.y <= kOverflowTolerance) {
            vertical = false;
        }
        if (hadHorizontal != horizontal || hadVertical != vertical) {
            resolveGeometry(
                context.bounds, required, style, horizontal, vertical);
        }

        context.setChildBounds(m_content,
                               {
                                   .center = m_viewport.topLeft() -
                                             m_scrollOffset +
                                             m_contentExtent * 0.5f,
                                   .size = m_contentExtent,
                               });
    }

    void ScrollView::paint(WidgetPaintContext &context) const {
        const auto &style = resolvedStyle(context.state);
        const auto paintBar = [&](const BarGeometry &bar, Scrollbar scrollbar) {
            if (!bar.visible || bar.track.empty() || bar.thumb.empty()) {
                return;
            }
            context.painter.drawBox(boxPaint(
                bar.track, style.track, context.pickingId, kScrollbarTrackZ));
            const UIBoxStyle *thumbStyle = &style.thumb;
            if (m_draggedScrollbar == scrollbar) {
                thumbStyle = &style.thumbPressed;
            } else if (m_hoveredScrollbar == scrollbar) {
                thumbStyle = &style.thumbHovered;
            }
            context.painter.drawBox(boxPaint(
                bar.thumb, *thumbStyle, context.pickingId, kScrollbarThumbZ));
        };
        paintBar(m_horizontalBar, Scrollbar::horizontal);
        paintBar(m_verticalBar, Scrollbar::vertical);
    }

    WidgetBounds
    ScrollView::childClipBounds(WidgetBounds bounds) const noexcept {
        return m_content ? m_viewport : bounds;
    }

    UIEventReply ScrollView::onEvent(WidgetEventContext &context,
                                     const UIEvent &event) {
        const auto pointer =
            context.hasPointerPosition ? context.pointerPosition : glm::vec2{};

        if (context.phase == UIEventPhase::capture) {
            if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                button != nullptr && button->button == MouseButton::left &&
                button->action == MouseButtonAction::press &&
                context.hasPointerPosition) {
                const Scrollbar bar = scrollbarAt(pointer);
                if (bar != Scrollbar::none) {
                    beginScrollbarDrag(bar, pointer);
                    return {.handled = true,
                            .stopPropagation = true,
                            .capturePointer = true,
                            .invalidate = WidgetInvalidation::layout |
                                          WidgetInvalidation::paint};
                }
            }
            if (event.is<Input::MouseMoveEvent>() &&
                m_draggedScrollbar == Scrollbar::none &&
                context.hasPointerPosition && updateHoveredScrollbar(pointer)) {
                return {.invalidate = WidgetInvalidation::paint};
            }
            return {};
        }

        if (context.phase != UIEventPhase::target &&
            context.phase != UIEventPhase::bubble) {
            return {};
        }

        if (const auto *crossing = event.getIf<UIPointerCrossingEvent>();
            crossing != nullptr && !crossing->entered &&
            m_draggedScrollbar == Scrollbar::none) {
            const bool changed = m_hoveredScrollbar != Scrollbar::none;
            m_hoveredScrollbar = Scrollbar::none;
            return {.invalidate = changed ? WidgetInvalidation::paint
                                          : WidgetInvalidation::none};
        }

        if (event.is<Input::MouseMoveEvent>()) {
            if (m_draggedScrollbar != Scrollbar::none) {
                const bool changed = updateScrollbarDrag(pointer);
                return {.handled = true,
                        .stopPropagation = true,
                        .capturePointer = true,
                        .invalidate = changed ? WidgetInvalidation::layout |
                                                    WidgetInvalidation::paint
                                              : WidgetInvalidation::paint};
            }
            const bool changed =
                context.hasPointerPosition && updateHoveredScrollbar(pointer);
            return {.invalidate = changed ? WidgetInvalidation::paint
                                          : WidgetInvalidation::none};
        }

        if (const auto *button = event.getIf<Input::MouseButtonEvent>();
            button != nullptr && button->button == MouseButton::left) {
            if (button->action == MouseButtonAction::release &&
                m_draggedScrollbar != Scrollbar::none) {
                if (context.hasPointerPosition) {
                    static_cast<void>(updateScrollbarDrag(pointer));
                }
                m_draggedScrollbar = Scrollbar::none;
                m_thumbGrabOffset = 0.f;
                m_hoveredScrollbar = context.hasPointerPosition
                                         ? scrollbarAt(pointer)
                                         : Scrollbar::none;
                return {.handled = true,
                        .stopPropagation = true,
                        .releasePointer = true,
                        .invalidate = WidgetInvalidation::layout |
                                      WidgetInvalidation::paint};
            }
            if (button->action == MouseButtonAction::press &&
                context.hasPointerPosition) {
                const Scrollbar bar = scrollbarAt(pointer);
                if (bar != Scrollbar::none) {
                    beginScrollbarDrag(bar, pointer);
                    return {.handled = true,
                            .stopPropagation = true,
                            .capturePointer = true,
                            .invalidate = WidgetInvalidation::layout |
                                          WidgetInvalidation::paint};
                }
            }
        }

        if (const auto *wheel = event.getIf<Input::MouseWheelEvent>();
            wheel != nullptr) {
            const float step = finiteMetric(
                resolvedStyle(context.state).wheelStep, 36.f, 1.f, 512.f);
            glm::vec2 requested = m_scrollOffset;
            if (event.modifiers.shift) {
                const float delta = std::abs(wheel->offset.x) > 0.f
                                        ? wheel->offset.x
                                        : wheel->offset.y;
                requested.x -= delta * step;
            } else {
                requested.x -= wheel->offset.x * step;
                requested.y -= wheel->offset.y * step;
                if (m_maximumScrollOffset.y <= 0.f &&
                    m_maximumScrollOffset.x > 0.f && wheel->offset.x == 0.f) {
                    requested.x -= wheel->offset.y * step;
                }
            }
            if (setScrollOffset(requested)) {
                return {.handled = true,
                        .stopPropagation = true,
                        .invalidate = WidgetInvalidation::layout |
                                      WidgetInvalidation::paint};
            }
            // At an extent boundary the event remains unhandled, allowing an
            // enclosing ScrollView to continue the gesture (scroll chaining).
        }

        return {};
    }

    glm::vec2 ScrollView::scrollOffset() const noexcept {
        return m_scrollOffset;
    }

    glm::vec2 ScrollView::maximumScrollOffset() const noexcept {
        return m_maximumScrollOffset;
    }

    glm::vec2 ScrollView::contentExtent() const noexcept {
        return m_contentExtent;
    }

    WidgetBounds ScrollView::viewportBounds() const noexcept {
        return m_viewport;
    }

    bool ScrollView::hasHorizontalScrollbar() const noexcept {
        return m_horizontalBar.visible;
    }

    bool ScrollView::hasVerticalScrollbar() const noexcept {
        return m_verticalBar.visible;
    }

    bool ScrollView::setScrollOffset(glm::vec2 offset) noexcept {
        offset = nonNegative(offset);
        offset = glm::min(offset, nonNegative(m_maximumScrollOffset));
        if (offset == m_scrollOffset) {
            return false;
        }
        m_scrollOffset = offset;
        return true;
    }

    const UIScrollStyle &
    ScrollView::resolvedStyle(const WidgetTree &state) const noexcept {
        return m_options.style ? *m_options.style : state.theme().scroll;
    }

    glm::vec2
    ScrollView::requiredContentExtent(const WidgetTree &state,
                                      WidgetId content,
                                      WidgetBounds viewport) const noexcept {
        (void)viewport;
        const auto *rootLayout = state.getLayout(content);
        const WidgetBounds rootBounds = state.getBounds(content);
        glm::vec2 required{0.f, 0.f};
        if (rootLayout != nullptr) {
            required =
                glm::max(required, nonNegative(rootLayout->getMinSize()));
            if (rootLayout->getWidthMode() == LayoutSizeMode::point) {
                required.x = std::max(required.x,
                                      nonNegative(rootLayout->getWidthValue()));
            }
            if (rootLayout->getHeightMode() == LayoutSizeMode::point) {
                required.y = std::max(
                    required.y, nonNegative(rootLayout->getHeightValue()));
            }
            const auto &padding = rootLayout->getPadding();
            required = glm::max(
                required,
                glm::vec2{
                    nonNegative(padding.left) + nonNegative(padding.right),
                    nonNegative(padding.top) + nonNegative(padding.bottom)});
        }

        // Intrinsic scroll size comes from descendants, not the root's
        // arranged box. Stretch-to-fill content roots (and the size ScrollView
        // assigned last frame) must not be mistaken for overflow.
        //
        // Nested ScrollViews (including DockPanel, which derives from
        // ScrollView) are opaque: their own scroll content must not inflate
        // this parent's required extent, or the outer panel scrolls the whole
        // chrome while the inner bar is irrelevant.
        glm::vec2 extentMin{0.f, 0.f};
        glm::vec2 extentMax{0.f, 0.f};
        bool hasLeaf = false;
        const glm::vec2 origin = rootBounds.topLeft();
        const auto accumulateLeaf = [&](glm::vec2 topLeft, glm::vec2 size) {
            extentMin = hasLeaf ? glm::min(extentMin, topLeft) : topLeft;
            extentMax =
                hasLeaf ? glm::max(extentMax, topLeft + size) : topLeft + size;
            hasLeaf = true;
        };
        const auto leafSize = [](const LayoutNode *layout,
                                 WidgetBounds bounds,
                                 glm::vec2 minimum) {
            glm::vec2 size = glm::max(bounds.size, minimum);
            if (layout != nullptr) {
                if (layout->getWidthMode() == LayoutSizeMode::point) {
                    size.x =
                        std::max(size.x, nonNegative(layout->getWidthValue()));
                }
                if (layout->getHeightMode() == LayoutSizeMode::point) {
                    size.y =
                        std::max(size.y, nonNegative(layout->getHeightValue()));
                }
            }
            return size;
        };
        const auto accumulate = [&](auto &&self, WidgetId id) -> void {
            if (state.getVisibility(id) == WidgetVisibility::collapsed) {
                return;
            }
            const auto children = state.getChildren(id);
            const auto *layout = state.getLayout(id);
            const WidgetBounds bounds = state.getBounds(id);
            const glm::vec2 minimum = layout != nullptr
                                          ? nonNegative(layout->getMinSize())
                                          : glm::vec2{};
            const bool constrained = minimum.x > 0.f || minimum.y > 0.f;
            const bool explicitPoint =
                layout != nullptr &&
                (layout->getWidthMode() == LayoutSizeMode::point ||
                 layout->getHeightMode() == LayoutSizeMode::point);
            // Nested scroll hosts own their descendants' overflow.
            if (state.getWidget<ScrollView>(id) != nullptr) {
                accumulateLeaf(bounds.topLeft() - origin,
                               leafSize(layout, bounds, minimum));
                return;
            }
            if (children.empty() || constrained || explicitPoint) {
                accumulateLeaf(bounds.topLeft() - origin,
                               leafSize(layout, bounds, minimum));
            }
            for (const WidgetId child : children) {
                self(self, child);
            }
        };
        for (const WidgetId child : state.getChildren(content)) {
            accumulate(accumulate, child);
        }
        if (hasLeaf) {
            required = glm::max(required, extentMax - extentMin);
        }
        return nonNegative(required);
    }

    void ScrollView::resolveGeometry(WidgetBounds bounds,
                                     glm::vec2 requiredExtent,
                                     const UIScrollStyle &style,
                                     bool horizontal,
                                     bool vertical) noexcept {
        const float thickness = finiteMetric(style.thickness, 10.f, 4.f, 24.f);
        const float margin = finiteMetric(style.margin, 2.f, 0.f, 8.f);
        const float gutter = thickness + margin * 2.f;
        const glm::vec2 available = nonNegative(bounds.size);
        horizontal = horizontal && m_options.horizontal;
        vertical = vertical && m_options.vertical;

        const glm::vec2 viewportSize =
            glm::max(available - glm::vec2{vertical ? gutter : 0.f,
                                           horizontal ? gutter : 0.f},
                     glm::vec2{0.f});
        m_viewport = {
            .center = bounds.topLeft() + viewportSize * 0.5f,
            .size = viewportSize,
        };
        const glm::vec2 scrollableExtent{
            horizontal ? nonNegative(requiredExtent.x) : viewportSize.x,
            vertical ? nonNegative(requiredExtent.y) : viewportSize.y,
        };
        m_contentExtent = glm::max(scrollableExtent, viewportSize);
        m_maximumScrollOffset =
            glm::max(m_contentExtent - viewportSize, glm::vec2{0.f});
        m_scrollOffset =
            glm::min(nonNegative(m_scrollOffset), m_maximumScrollOffset);

        m_horizontalBar = {};
        m_verticalBar = {};
        const float minimumThumb =
            finiteMetric(style.minimumThumbLength, 24.f, thickness, 256.f);
        // Only paint a bar when the axis actually scrolls. Visible chrome with
        // zero travel leaves a thumb glued to the track start.
        const bool showHorizontal =
            horizontal && m_maximumScrollOffset.x > kOverflowTolerance;
        const bool showVertical =
            vertical && m_maximumScrollOffset.y > kOverflowTolerance;
        if (showHorizontal) {
            const float trackLength = std::max(
                0.f,
                available.x - (showVertical ? gutter : 0.f) - margin * 2.f);
            m_horizontalBar.visible = trackLength > 0.f;
            m_horizontalBar.track = {
                .center = {bounds.topLeft().x + margin + trackLength * 0.5f,
                           bounds.bottomRight().y - margin - thickness * 0.5f},
                .size = {trackLength, thickness},
            };
            const float ratio = m_contentExtent.x > 0.f
                                    ? viewportSize.x / m_contentExtent.x
                                    : 1.f;
            const float thumbLength =
                std::clamp(trackLength * ratio,
                           std::min(minimumThumb, trackLength),
                           trackLength);
            const float travel = std::max(0.f, trackLength - thumbLength);
            const float progress =
                m_maximumScrollOffset.x > 0.f
                    ? m_scrollOffset.x / m_maximumScrollOffset.x
                    : 0.f;
            m_horizontalBar.thumb = {
                .center = {m_horizontalBar.track.topLeft().x +
                               thumbLength * 0.5f + travel * progress,
                           m_horizontalBar.track.center.y},
                .size = {thumbLength, thickness},
            };
        }
        if (showVertical) {
            const float trackLength = std::max(
                0.f,
                available.y - (showHorizontal ? gutter : 0.f) - margin * 2.f);
            m_verticalBar.visible = trackLength > 0.f;
            m_verticalBar.track = {
                .center = {bounds.bottomRight().x - margin - thickness * 0.5f,
                           bounds.topLeft().y + margin + trackLength * 0.5f},
                .size = {thickness, trackLength},
            };
            const float ratio = m_contentExtent.y > 0.f
                                    ? viewportSize.y / m_contentExtent.y
                                    : 1.f;
            const float thumbLength =
                std::clamp(trackLength * ratio,
                           std::min(minimumThumb, trackLength),
                           trackLength);
            const float travel = std::max(0.f, trackLength - thumbLength);
            const float progress =
                m_maximumScrollOffset.y > 0.f
                    ? m_scrollOffset.y / m_maximumScrollOffset.y
                    : 0.f;
            m_verticalBar.thumb = {
                .center = {m_verticalBar.track.center.x,
                           m_verticalBar.track.topLeft().y +
                               thumbLength * 0.5f + travel * progress},
                .size = {thickness, thumbLength},
            };
        }

        if (!m_horizontalBar.visible &&
            m_hoveredScrollbar == Scrollbar::horizontal) {
            m_hoveredScrollbar = Scrollbar::none;
        }
        if (!m_verticalBar.visible &&
            m_hoveredScrollbar == Scrollbar::vertical) {
            m_hoveredScrollbar = Scrollbar::none;
        }
    }

    ScrollView::Scrollbar
    ScrollView::scrollbarAt(glm::vec2 position) const noexcept {
        if (m_verticalBar.visible && m_verticalBar.track.contains(position)) {
            return Scrollbar::vertical;
        }
        if (m_horizontalBar.visible &&
            m_horizontalBar.track.contains(position)) {
            return Scrollbar::horizontal;
        }
        return Scrollbar::none;
    }

    bool ScrollView::updateHoveredScrollbar(glm::vec2 position) noexcept {
        const Scrollbar hovered = scrollbarAt(position);
        if (hovered == m_hoveredScrollbar) {
            return false;
        }
        m_hoveredScrollbar = hovered;
        return true;
    }

    void ScrollView::beginScrollbarDrag(Scrollbar bar,
                                        glm::vec2 position) noexcept {
        const auto &barGeometry = geometry(bar);
        if (bar == Scrollbar::none || !barGeometry.visible) {
            return;
        }
        m_draggedScrollbar = bar;
        m_hoveredScrollbar = bar;
        const bool horizontal = bar == Scrollbar::horizontal;
        const float pointer = coordinate(bar, position);
        const float thumbStart = axisStart(barGeometry.thumb, horizontal);
        if (barGeometry.thumb.contains(position)) {
            m_thumbGrabOffset = pointer - thumbStart;
        } else {
            m_thumbGrabOffset =
                axisLength(barGeometry.thumb, horizontal) * 0.5f;
            static_cast<void>(updateScrollbarDrag(position));
        }
    }

    bool ScrollView::updateScrollbarDrag(glm::vec2 position) noexcept {
        const Scrollbar bar = m_draggedScrollbar;
        if (bar == Scrollbar::none) {
            return false;
        }
        const auto &barGeometry = geometry(bar);
        const bool horizontal = bar == Scrollbar::horizontal;
        const float trackLength = axisLength(barGeometry.track, horizontal);
        const float thumbLength = axisLength(barGeometry.thumb, horizontal);
        const float travel = std::max(0.f, trackLength - thumbLength);
        const float maximum =
            horizontal ? m_maximumScrollOffset.x : m_maximumScrollOffset.y;
        if (travel <= 0.f || maximum <= 0.f) {
            return false;
        }
        const float thumbStart =
            std::clamp(coordinate(bar, position) - m_thumbGrabOffset -
                           axisStart(barGeometry.track, horizontal),
                       0.f,
                       travel);
        glm::vec2 requested = m_scrollOffset;
        const float value = thumbStart / travel * maximum;
        if (horizontal) {
            requested.x = value;
        } else {
            requested.y = value;
        }
        return setScrollOffset(requested);
    }

    float ScrollView::coordinate(Scrollbar bar,
                                 glm::vec2 position) const noexcept {
        return bar == Scrollbar::horizontal ? position.x : position.y;
    }

    const ScrollView::BarGeometry &
    ScrollView::geometry(Scrollbar bar) const noexcept {
        return bar == Scrollbar::vertical ? m_verticalBar : m_horizontalBar;
    }

} // namespace Bess::UI
