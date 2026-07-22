#include "controls/dock_space.h"

#include "bess_core/ui/icons/font_awesome_icons.h"
#include "controls/tab_bar.h"
#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace Bess::UI {
    namespace {
        BoxPaint makeBox(WidgetBounds bounds,
                         const UIBoxStyle &style,
                         PickingId id = PickingId::invalid(),
                         float z = 0.f) {
            return {
                .bounds = bounds,
                .color = style.background,
                .borderColor = style.border,
                .cornerRadius = style.cornerRadius,
                .borderThickness = style.borderThickness,
                .shadow = style.shadow,
                .zIndex = z,
                .pickingId = id,
            };
        }

        TabStripMetrics metrics(const UITabStyle &style) {
            return {
                .height = style.height,
                .minimumWidth = style.minimumWidth,
                .maximumWidth = style.maximumWidth,
                .horizontalPadding = style.horizontalPadding,
                .stripPadding = style.stripPadding,
                .gap = style.gap,
            };
        }

        TabStripRegion withCloseButton(TabStripRegion region,
                                       const UITabStyle &style,
                                       float trailingPadding) noexcept {
            return TabStripLayout::withTrailingAction(std::move(region),
                                                      style.closeButtonSize,
                                                      style.closeButtonGap,
                                                      trailingPadding);
        }

        UIBoxStyle circularStyle(const UIBoxStyle &source,
                                 WidgetBounds bounds) {
            auto result = source;
            result.cornerRadius =
                glm::vec4{std::min(bounds.size.x, bounds.size.y) * 0.5f};
            return result;
        }

        bool finiteVec(glm::vec2 value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        struct FloatingSizeLimits {
            glm::vec2 minimum{1.f, 1.f};
            glm::vec2 maximum{1.f, 1.f};
        };

        FloatingSizeLimits floatingSizeLimits(const UIDockStyle &style,
                                              glm::vec2 available) noexcept {
            available = {
                std::isfinite(available.x) ? std::max(1.f, available.x) : 1.f,
                std::isfinite(available.y) ? std::max(1.f, available.y) : 1.f,
            };
            const auto configuredMinimum = glm::vec2{
                std::isfinite(style.floatingMinimumSize.x)
                    ? std::max(1.f, style.floatingMinimumSize.x)
                    : 1.f,
                std::isfinite(style.floatingMinimumSize.y)
                    ? std::max(1.f, style.floatingMinimumSize.y)
                    : 1.f,
            };
            const glm::vec2 minimum = glm::min(configuredMinimum, available);
            const auto configuredMaximum = glm::vec2{
                std::isfinite(style.floatingMaximumSize.x) &&
                        style.floatingMaximumSize.x > 0.f
                    ? style.floatingMaximumSize.x
                    : available.x,
                std::isfinite(style.floatingMaximumSize.y) &&
                        style.floatingMaximumSize.y > 0.f
                    ? style.floatingMaximumSize.y
                    : available.y,
            };
            return {
                .minimum = minimum,
                .maximum =
                    glm::max(minimum, glm::min(configuredMaximum, available)),
            };
        }

        constexpr float floatingLayer(size_t index) noexcept {
            return 0.10f + static_cast<float>(index) * 0.05f;
        }

        // Floating chrome is composited in this order. In particular, the
        // outline must be the final chrome layer: the title-bar quad shares
        // the host's top and side edges and would otherwise cover its border.
        constexpr float kFloatingHeaderLayerOffset = 0.011f;
        constexpr float kFloatingCloseLayerOffset = 0.012f;
        constexpr float kFloatingForegroundLayerOffset = 0.013f;
        constexpr float kFloatingOutlineLayerOffset = 0.014f;

        WidgetBounds zoneGlyph(WidgetBounds bounds, DockZone zone) noexcept {
            const float inset = std::max(4.f, bounds.size.x * 0.24f);
            return DockDropGuideLayoutSolver::regionBounds(bounds, zone, inset);
        }

    } // namespace

    DockPanel::DockPanel(std::string title, DockPanelOptions options)
        : ScrollView({.horizontal = true,
                      .vertical = true,
                      .clipContent = options.clipContent}),
          m_title(std::move(title)),
          m_options(std::move(options)) {
    }

    std::string_view DockPanel::typeName() const noexcept {
        return "DockPanel";
    }

    void DockPanel::onMount(WidgetMountContext &context) {
        ScrollView::onMount(context);
        context.layout.setPosMode(PosMode::absolute);
        context.layout.setWidth(0.f);
        context.layout.setHeight(0.f);
    }

    void DockPanel::paint(WidgetPaintContext &context) const {
        if (m_options.background) {
            context.painter.drawBox(makeBox(
                context.bounds, *m_options.background, context.pickingId));
        }
        ScrollView::paint(context);
    }

    DockItemId DockPanel::itemId() const noexcept {
        return m_itemId;
    }

    const std::string &DockPanel::title() const noexcept {
        return m_title;
    }

    void DockPanel::setTitle(std::string title) {
        m_title = std::move(title);
    }

    DockSpace::DockSpace(DockSpaceOptions options)
        : m_options(std::move(options)) {
    }

    std::string_view DockSpace::typeName() const noexcept {
        return "DockSpace";
    }

    WidgetTraits DockSpace::traits() const noexcept {
        return {
            .focusable = true, .hitTestVisible = true, .clipChildren = true};
    }

    void DockSpace::onMount(WidgetMountContext &context) {
        context.layout.setWidthPercent(1.f);
        context.layout.setHeightStretch();
        m_mountedState = &context.state;
        m_mountedId = context.id;
        m_modelConnection = m_model.changed().connect(
            [state = &context.state, id = context.id](const DockModelChange &) {
                state->invalidate(
                    id, WidgetInvalidation::layout | WidgetInvalidation::paint);
            });
    }

    void DockSpace::onUnmount(WidgetTree &, WidgetId) {
        m_modelConnection.disconnect();
        m_mountedState = nullptr;
        m_mountedId = {};
        clearTabInteraction();
        clearCloseInteraction();
        clearResizeInteraction();
    }

    void DockSpace::arrange(WidgetArrangeContext &context) {
        for (const auto child : context.children()) {
            if (context.state.getWidget<DockPanel>(child) == nullptr) {
                continue;
            }
            context.setChildVisible(child, false);
            if (auto *layout = context.state.getLayout(child)) {
                layout->setZVal(0.f);
            }
        }

        const auto arrangeModel = [&](const DockSpaceModel &model,
                                      const DockLayoutResult &layout,
                                      float z) {
            for (const auto &stackLayout : layout.stacks) {
                const auto *item = model.getItem(stackLayout.activeItem);
                if (item == nullptr || !context.isDirectChild(item->content)) {
                    continue;
                }
                context.setChildBounds(item->content,
                                       stackLayout.contentBounds);
                context.setChildVisible(item->content, true);
                if (auto *childLayout =
                        context.state.getLayout(item->content)) {
                    childLayout->setZVal(z);
                }
            }
        };

        arrangeModel(
            m_model, calculateLayout(context.bounds, context.state), 0.f);
        for (size_t i = 0; i < m_floatingHosts.size(); ++i) {
            auto &host = *m_floatingHosts[i];
            host.bounds = normalizedFloatingBounds(
                host.bounds, context.bounds, context.state);
            arrangeModel(host.model,
                         calculateFloatingLayout(host, context.state),
                         floatingLayer(i));
        }
    }

    void DockSpace::paint(WidgetPaintContext &context) const {
        const auto &dock = dockStyle(context.state);
        const auto &tabs = tabStyle(context.state);
        context.painter.drawBox(
            makeBox(context.bounds, dock.background, context.pickingId));

        const auto paintTopology = [&](const DockSpaceModel &model,
                                       const DockLayoutResult &layout,
                                       float layer,
                                       DockHostId host,
                                       const WidgetBounds *floatingClient =
                                           nullptr) {
            uint32_t pickingInfo = 1;
            for (const auto &stackLayout : layout.stacks) {
                auto stackStyle = host ? dock.floatingStack : dock.stack;
                if (floatingClient != nullptr) {
                    constexpr float edgeTolerance = 0.5f;
                    const auto stackTopLeft = stackLayout.bounds.topLeft();
                    const auto stackBottomRight =
                        stackLayout.bounds.bottomRight();
                    const auto clientTopLeft = floatingClient->topLeft();
                    const auto clientBottomRight =
                        floatingClient->bottomRight();
                    const bool touchesBottom =
                        std::abs(stackBottomRight.y - clientBottomRight.y) <=
                        edgeTolerance;
                    const bool touchesLeft =
                        std::abs(stackTopLeft.x - clientTopLeft.x) <=
                        edgeTolerance;
                    const bool touchesRight =
                        std::abs(stackBottomRight.x - clientBottomRight.x) <=
                        edgeTolerance;
                    // Floating content is one continuous window surface, not
                    // a rounded panel nested below the title bar. Preserve
                    // only the corners that coincide with the outer bottom
                    // silhouette when the host contains multiple stacks.
                    stackStyle.cornerRadius.x = 0.f;
                    stackStyle.cornerRadius.y = 0.f;
                    if (!touchesBottom || !touchesRight) {
                        stackStyle.cornerRadius.z = 0.f;
                    }
                    if (!touchesBottom || !touchesLeft) {
                        stackStyle.cornerRadius.w = 0.f;
                    }
                }
                context.painter.drawBox(makeBox(
                    stackLayout.bounds, stackStyle, context.pickingId, layer));
                if (stackLayout.tabBarBounds.size.y <= 0.f) {
                    continue;
                }
                context.painter.drawBox(makeBox(stackLayout.tabBarBounds,
                                                tabs.strip,
                                                context.pickingId,
                                                layer + 0.001f));
                const auto *stack = model.getStack(stackLayout.node);
                if (stack == nullptr) {
                    continue;
                }
                const auto regions =
                    TabStripLayout::calculate(stackLayout.tabBarBounds,
                                              stack->tabs.size(),
                                              metrics(tabs));
                const auto items = stack->tabs.items();
                for (size_t i = 0; i < regions.size() && i < items.size();
                     ++i) {
                    const auto &item = items[i];
                    auto region = regions[i];
                    if (item.closable) {
                        region =
                            withCloseButton(std::move(region),
                                            tabs,
                                            tabs.closeButtonTrailingPadding);
                    }
                    const HitTab tab{
                        .host = host,
                        .stack = stackLayout.node,
                        .item = item.id,
                    };
                    const UIBoxStyle *style = &tabs.normal;
                    if (m_pressedItem == item.id &&
                        m_tabPressable.isPressed()) {
                        style = &tabs.pressed;
                    } else if (stack->tabs.active() == item.id) {
                        style = &tabs.active;
                    } else if (m_hoveredItem == item.id) {
                        style = &tabs.hovered;
                    }
                    PickingId id = context.pickingId;
                    id.info = pickingInfo++;
                    context.painter.drawBox(
                        makeBox(region.bounds, *style, id, layer + 0.002f));
                    const auto textColor = stack->tabs.active() == item.id
                                               ? tabs.text.color
                                               : tabs.inactiveText;
                    context.painter.drawText(
                        item.title,
                        {
                            .bounds = region.labelBounds,
                            .fontSize = tabs.text.fontSize,
                            .color = textColor,
                            .horizontal = HorizontalTextAlignment::start,
                            .vertical = VerticalTextAlignment::center,
                            .zIndex = layer + 0.003f,
                            .letterSpacing = tabs.text.letterSpacing,
                            .pickingId = id,
                        });
                    if (item.closable && !region.trailingActionBounds.empty()) {
                        const bool closePressed = tab == m_pressedClose;
                        const bool closeHovered = tab == m_hoveredClose;
                        IconPaint closePaint{
                            .glyph = {
                                .bounds = region.trailingActionBounds,
                                .fontSize = std::max(1.f, tabs.closeIconSize),
                                .color = closePressed || closeHovered
                                             ? tabs.closeIconHovered
                                             : tabs.closeIcon,
                                .horizontal =
                                    HorizontalTextAlignment::center,
                                .vertical = VerticalTextAlignment::center,
                                .zIndex = layer + 0.004f,
                                .pickingId = id,
                            },
                        };
                        if (closePressed || closeHovered) {
                            const auto closeStyle =
                                circularStyle(closePressed ? tabs.closePressed
                                                           : tabs.closeHovered,
                                              region.trailingActionBounds);
                            closePaint.background =
                                makeBox(region.trailingActionBounds,
                                        closeStyle,
                                        id,
                                        layer + 0.0035f);
                        }
                        context.painter.drawIcon(
                            Icons::FontAwesomeIcons::FA_XMARK,
                            closePaint);
                    }
                }
            }

            for (const auto &splitLayout : layout.splits) {
                const SplitHit split{.host = host, .node = splitLayout.node};
                const bool active =
                    split == m_hoveredSplit || split == m_draggedSplit;
                const auto color =
                    active ? dock.splitterHovered : dock.splitter;
                auto visualBounds = splitLayout.dividerBounds;
                const auto *splitNode = model.getSplit(splitLayout.node);
                if (splitNode == nullptr) {
                    continue;
                }
                const bool horizontal =
                    splitNode->axis == DockSplitAxis::horizontal;
                const float maximumThickness =
                    horizontal ? visualBounds.size.x : visualBounds.size.y;
                const float requestedThickness =
                    active ? maximumThickness : dock.splitterIdleThickness;
                const float visualThickness = std::clamp(
                    std::isfinite(requestedThickness) ? requestedThickness
                                                      : 0.f,
                    0.f,
                    maximumThickness);
                if (horizontal) {
                    visualBounds.size.x = visualThickness;
                } else {
                    visualBounds.size.y = visualThickness;
                }
                context.painter.drawBox({
                    .bounds = visualBounds,
                    .color = color,
                    .zIndex = layer + 0.004f,
                    .pickingId = context.pickingId,
                });
            }
        };

        paintTopology(
            m_model, calculateLayout(context.bounds, context.state), 0.f, {});
        const ScopedUIClip floatingClip{context.painter, context.bounds};
        for (size_t i = 0; i < m_floatingHosts.size(); ++i) {
            const auto &host = *m_floatingHosts[i];
            const float layer = floatingLayer(i);
            context.painter.drawBox(makeBox(host.bounds,
                                            dock.floatingWindow,
                                            context.pickingId,
                                            layer - 0.005f));
            const auto client = floatingClientBounds(host, context.state);
            paintTopology(host.model,
                          calculateFloatingLayout(host, context.state),
                          layer,
                          host.id,
                          &client);
        }
    }

    void DockSpace::paintOverlay(WidgetPaintContext &context) const {
        const auto &dock = dockStyle(context.state);
        const auto &tabs = tabStyle(context.state);
        for (size_t i = 0; i < m_floatingHosts.size(); ++i) {
            const auto &host = *m_floatingHosts[i];
            const auto *titleItem = host.model.getItem(floatingTitleItem(host));
            if (titleItem == nullptr) {
                continue;
            }

            const float layer = floatingLayer(i);
            auto headerStyle = dock.floatingHeader;
            if (m_tabDrag && m_tabDrag->host == host.id && m_tabDrag->window) {
                headerStyle.background = tabs.pressed.background;
                headerStyle.border = tabs.pressed.border;
            } else if (m_hoveredItem == titleItem->id) {
                headerStyle.background = tabs.hovered.background;
                headerStyle.border = tabs.hovered.border;
            }
            const auto header = floatingHeaderBounds(host, context.state);
            context.painter.drawBox(
                makeBox(header,
                        headerStyle,
                        context.pickingId,
                        layer + kFloatingHeaderLayerOffset));
            const bool closable = titleItem->closable &&
                                  host.model.itemCount() == 1 &&
                                  host.model.stackCount() == 1;
            const auto headerRegion =
                floatingHeaderRegion(host, context.state, closable);
            const HitTab titleTab{
                .host = host.id,
                .stack = host.model.stackForItem(titleItem->id),
                .item = titleItem->id,
            };
            context.painter.drawText(
                titleItem->title,
                {
                    .bounds = headerRegion.labelBounds,
                    .fontSize = tabs.text.fontSize,
                    .color = tabs.text.color,
                    .horizontal = HorizontalTextAlignment::start,
                    .vertical = VerticalTextAlignment::center,
                    .zIndex = layer + kFloatingForegroundLayerOffset,
                    .letterSpacing = tabs.text.letterSpacing,
                    .pickingId = context.pickingId,
                });
            if (closable && !headerRegion.trailingActionBounds.empty()) {
                const bool closePressed = titleTab == m_pressedClose;
                const bool closeHovered = titleTab == m_hoveredClose;
                IconPaint closePaint{
                    .glyph =
                        {
                            .bounds = headerRegion.trailingActionBounds,
                            .fontSize = std::max(1.f, tabs.closeIconSize),
                            .color = closePressed || closeHovered
                                         ? tabs.closeIconHovered
                                         : tabs.closeIcon,
                            .horizontal = HorizontalTextAlignment::center,
                            .vertical = VerticalTextAlignment::center,
                            .zIndex = layer + kFloatingForegroundLayerOffset,
                            .pickingId = context.pickingId,
                        },
                };
                if (closePressed || closeHovered) {
                    const auto closeStyle = circularStyle(
                        closePressed ? tabs.closePressed : tabs.closeHovered,
                        headerRegion.trailingActionBounds);
                    closePaint.background =
                        makeBox(headerRegion.trailingActionBounds,
                                closeStyle,
                                context.pickingId,
                                layer + kFloatingCloseLayerOffset);
                }
                context.painter.drawIcon(Icons::FontAwesomeIcons::FA_XMARK,
                                         closePaint);
            }

            auto outline = dock.floatingWindow;
            outline.background = outline.background.withAlpha(0.f);
            outline.shadow.enabled = false;
            context.painter.drawBox(
                makeBox(host.bounds,
                        outline,
                        context.pickingId,
                        layer + kFloatingOutlineLayerOffset));
        }

        if (m_dropGuides.empty()) {
            return;
        }
        const float guideLayer = floatingLayer(m_floatingHosts.size()) + 1.f;
        for (const auto &guide : m_dropGuides) {
            for (const auto &region : guide.layout.regions) {
                const bool hovered = m_hoveredDrop &&
                                     m_hoveredDrop->host == guide.host &&
                                     m_hoveredDrop->root == guide.root &&
                                     m_hoveredDrop->zone == region.zone;
                if (hovered) {
                    context.painter.drawBox(makeBox(region.previewBounds,
                                                    dock.dropPreview,
                                                    context.pickingId,
                                                    guideLayer));
                }
                const auto &style =
                    hovered ? dock.dropGuideHovered : dock.dropGuide;
                context.painter.drawBox(makeBox(region.indicatorBounds,
                                                style,
                                                context.pickingId,
                                                guideLayer + 0.001f));
                auto glyphStyle = dock.dropPreview;
                glyphStyle.borderThickness = glm::vec4{1.f};
                glyphStyle.cornerRadius = glm::vec4{3.f};
                context.painter.drawBox(
                    makeBox(zoneGlyph(region.indicatorBounds, region.zone),
                            glyphStyle,
                            context.pickingId,
                            guideLayer + 0.002f));
            }
        }
    }

    CursorIcon DockSpace::cursor(const WidgetCursorContext &) const noexcept {
        const FloatingResizeHit resize =
            m_resizeDrag ? m_resizeDrag->hit : m_hoveredResize;
        if (resize) {
            const auto &edges = resize.edges;
            if ((edges.left && edges.top) || (edges.right && edges.bottom)) {
                return CursorIcon::resizeDiagonalNWSE;
            }
            if ((edges.right && edges.top) || (edges.left && edges.bottom)) {
                return CursorIcon::resizeDiagonalNESW;
            }
            if (edges.left || edges.right) {
                return CursorIcon::resizeHorizontal;
            }
            if (edges.top || edges.bottom) {
                return CursorIcon::resizeVertical;
            }
        }

        const SplitHit split =
            m_draggedSplit.node ? m_draggedSplit : m_hoveredSplit;
        if (!split.node) {
            return CursorIcon::inherit;
        }
        const auto *model = modelForHost(split.host);
        const auto *node =
            model != nullptr ? model->getSplit(split.node) : nullptr;
        if (node == nullptr) {
            return CursorIcon::inherit;
        }
        return node->axis == DockSplitAxis::horizontal
                   ? CursorIcon::resizeHorizontal
                   : CursorIcon::resizeVertical;
    }

    UIEventReply DockSpace::onEvent(WidgetEventContext &context,
                                    const UIEvent &event) {
        if (context.phase == UIEventPhase::capture) {
            if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                button != nullptr && button->button == MouseButton::left &&
                button->action == MouseButtonAction::press &&
                context.hasPointerPosition) {
                if (const auto close = hitClose(
                        context.bounds, context.state, context.pointerPosition);
                    close.item) {
                    clearTabInteraction();
                    clearResizeInteraction();
                    m_pressedClose = close;
                    m_hoveredClose = close;
                    return {.handled = true,
                            .stopPropagation = true,
                            .requestFocus = true,
                            .capturePointer = true,
                            .invalidate = WidgetInvalidation::paint};
                }
                if (const auto resize = floatingResizeAt(
                        context.pointerPosition, context.state);
                    resize) {
                    return beginFloatingResize(context, resize);
                }
                if (const DockHostId host = floatingHeaderAt(
                        context.pointerPosition, context.state);
                    host) {
                    return beginFloatingHeaderPress(context, host);
                }
            }
            if (event.is<Input::MouseMoveEvent>() && !m_tabDrag &&
                !m_resizeDrag && context.hasPointerPosition) {
                const auto close = hitClose(
                    context.bounds, context.state, context.pointerPosition);
                const auto resize =
                    floatingResizeAt(context.pointerPosition, context.state);
                const DockHostId host =
                    resize ? DockHostId{}
                           : floatingHeaderAt(context.pointerPosition,
                                              context.state);
                const auto *floating = findFloatingHost(host);
                const DockItemId item = floating != nullptr
                                            ? floatingTitleItem(*floating)
                                            : DockItemId{};
                const bool closeChanged = close != m_hoveredClose;
                const bool resizeChanged = resize != m_hoveredResize;
                const bool itemChanged =
                    item != m_hoveredItem &&
                    (item || isItemFloating(m_hoveredItem));
                if (closeChanged || resizeChanged || itemChanged) {
                    m_hoveredClose = close;
                    m_hoveredResize = resize;
                    m_hoveredItem = item;
                    return {.invalidate = WidgetInvalidation::paint};
                }
            }
            return {};
        }
        if (context.phase != UIEventPhase::target) {
            return {};
        }

        const auto mainLayout = calculateLayout(context.bounds, context.state);
        const auto splitAt = [&](glm::vec2 position) -> SplitHit {
            for (auto it = m_floatingHosts.rbegin();
                 it != m_floatingHosts.rend();
                 ++it) {
                const auto layout =
                    calculateFloatingLayout(**it, context.state);
                if (const auto split = layout.dividerAt(position); split) {
                    return {.host = (*it)->id, .node = split};
                }
                if ((*it)->bounds.contains(position)) {
                    return {};
                }
            }
            return {.node = mainLayout.dividerAt(position)};
        };

        if (const auto *crossing = event.getIf<UIPointerCrossingEvent>();
            crossing != nullptr && !crossing->entered) {
            m_hoveredItem = {};
            m_hoveredClose = {};
            m_hoveredSplit = {};
            m_hoveredResize = {};
            if (m_pressedClose.item) {
                return {.handled = true,
                        .stopPropagation = true,
                        .invalidate = WidgetInvalidation::paint};
            }
            if (m_tabDrag) {
                m_dropGuides.clear();
                m_hoveredDrop.reset();
                return {.handled = true,
                        .stopPropagation = true,
                        .invalidate = WidgetInvalidation::paint};
            }
            if (m_resizeDrag) {
                return {.handled = true,
                        .stopPropagation = true,
                        .invalidate = WidgetInvalidation::paint};
            }
            auto result = m_tabPressable.handle(context, event);
            result.reply.invalidate |= WidgetInvalidation::paint;
            return result.reply;
        }

        if (event.is<Input::MouseMoveEvent>()) {
            if (m_pressedClose.item) {
                m_hoveredClose = context.hasPointerPosition
                                     ? hitClose(context.bounds,
                                                context.state,
                                                context.pointerPosition)
                                     : HitTab{};
                return {.handled = true,
                        .stopPropagation = true,
                        .capturePointer = true,
                        .invalidate = WidgetInvalidation::paint};
            }
            if (m_resizeDrag) {
                updateFloatingResize(context);
                return {.handled = true,
                        .stopPropagation = true,
                        .capturePointer = true,
                        .invalidate = WidgetInvalidation::layout |
                                      WidgetInvalidation::paint};
            }
            if (m_draggedSplit.node) {
                auto *model = modelForHost(m_draggedSplit.host);
                const FloatingHost *host =
                    findFloatingHost(m_draggedSplit.host);
                const auto layout =
                    host != nullptr
                        ? calculateFloatingLayout(*host, context.state)
                        : mainLayout;
                const auto *split = model != nullptr
                                        ? model->getSplit(m_draggedSplit.node)
                                        : nullptr;
                const auto *splitLayout = layout.findSplit(m_draggedSplit.node);
                if (split != nullptr && splitLayout != nullptr) {
                    const float thickness =
                        dockStyle(context.state).splitterThickness;
                    const auto topLeft = splitLayout->bounds.topLeft();
                    const float axisSize =
                        split->axis == DockSplitAxis::horizontal
                            ? splitLayout->bounds.size.x
                            : splitLayout->bounds.size.y;
                    const float available = std::max(1.f, axisSize - thickness);
                    const float coordinate =
                        split->axis == DockSplitAxis::horizontal
                            ? context.pointerPosition.x - topLeft.x
                            : context.pointerPosition.y - topLeft.y;
                    model->setSplitRatio(m_draggedSplit.node,
                                         (coordinate - thickness * 0.5f) /
                                             available);
                }
                return {.handled = true,
                        .stopPropagation = true,
                        .capturePointer = true,
                        .invalidate = WidgetInvalidation::layout |
                                      WidgetInvalidation::paint};
            }

            if (m_tabDrag) {
                if (!m_tabDrag->started) {
                    const auto distance =
                        context.pointerPosition - m_tabDrag->pressPosition;
                    const float threshold =
                        std::max(0.f, dockStyle(context.state).dragThreshold);
                    if (distance.x * distance.x + distance.y * distance.y >=
                        threshold * threshold) {
                        if (m_tabDrag->window) {
                            m_tabDrag->started = true;
                        } else {
                            static_cast<void>(
                                beginTabDrag(context, context.bounds));
                        }
                    }
                }
                if (m_tabDrag && m_tabDrag->window && m_tabDrag->started) {
                    updateFloatingDrag(context);
                }
                return {.handled = true,
                        .stopPropagation = true,
                        .capturePointer = true,
                        .invalidate = WidgetInvalidation::layout |
                                      WidgetInvalidation::paint};
            }

            m_hoveredClose = context.hasPointerPosition
                                 ? hitClose(context.bounds,
                                            context.state,
                                            context.pointerPosition)
                                 : HitTab{};
            m_hoveredResize =
                context.hasPointerPosition
                    ? floatingResizeAt(context.pointerPosition, context.state)
                    : FloatingResizeHit{};
            m_hoveredSplit =
                m_hoveredResize ? SplitHit{} : splitAt(context.pointerPosition);
            const DockHostId headerHost =
                m_hoveredResize
                    ? DockHostId{}
                    : floatingHeaderAt(context.pointerPosition, context.state);
            const auto *header = findFloatingHost(headerHost);
            m_hoveredItem =
                header != nullptr ? floatingTitleItem(*header) : DockItemId{};
            if (!m_hoveredItem && !m_hoveredResize) {
                m_hoveredItem = hitTab(context.bounds,
                                       context.state,
                                       context.pointerPosition)
                                    .item;
            }
            auto result = m_tabPressable.handle(context, event);
            result.reply.invalidate |= WidgetInvalidation::paint;
            return result.reply;
        }

        if (const auto *button = event.getIf<Input::MouseButtonEvent>();
            button != nullptr && button->button == MouseButton::left) {
            if (button->action == MouseButtonAction::release &&
                m_pressedClose.item) {
                const HitTab pressed = m_pressedClose;
                const HitTab releasedOver =
                    context.hasPointerPosition
                        ? hitClose(context.bounds,
                                   context.state,
                                   context.pointerPosition)
                        : HitTab{};
                const bool activated = releasedOver == pressed;
                m_pressedClose = {};
                m_hoveredClose = releasedOver;
                const bool hidden = activated && hidePanel(pressed.item);
                if (hidden && context.hasPointerPosition) {
                    // The next tab can slide under a pointer which has not
                    // moved. Refresh the composite hit regions now rather
                    // than waiting for a future mouse-move event.
                    m_hoveredClose = hitClose(
                        context.bounds, context.state, context.pointerPosition);
                    m_hoveredItem = m_hoveredClose.item;
                    if (!m_hoveredItem) {
                        m_hoveredItem = hitTab(context.bounds,
                                               context.state,
                                               context.pointerPosition)
                                            .item;
                    }
                } else if (hidden) {
                    m_hoveredClose = {};
                    m_hoveredItem = {};
                }
                return {.handled = true,
                        .stopPropagation = true,
                        .releasePointer = true,
                        .invalidate = WidgetInvalidation::layout |
                                      WidgetInvalidation::paint};
            }

            if (button->action == MouseButtonAction::release && m_resizeDrag) {
                if (context.hasPointerPosition) {
                    updateFloatingResize(context);
                }
                m_resizeDrag.reset();
                m_hoveredResize =
                    context.hasPointerPosition
                        ? floatingResizeAt(context.pointerPosition,
                                           context.state)
                        : FloatingResizeHit{};
                return {.handled = true,
                        .stopPropagation = true,
                        .releasePointer = true,
                        .invalidate = WidgetInvalidation::layout |
                                      WidgetInvalidation::paint};
            }

            if (button->action == MouseButtonAction::release && m_tabDrag) {
                if (m_tabDrag->window) {
                    if (m_tabDrag->started && context.hasPointerPosition) {
                        updateFloatingDrag(context);
                        static_cast<void>(finishFloatingDrag());
                    }
                    clearTabInteraction();
                    return {.handled = true,
                            .stopPropagation = true,
                            .releasePointer = true,
                            .invalidate = WidgetInvalidation::layout |
                                          WidgetInvalidation::paint};
                }

                const auto hit = context.hasPointerPosition
                                     ? hitTab(context.bounds,
                                              context.state,
                                              context.pointerPosition)
                                     : HitTab{};
                const DockItemId pressed = m_tabDrag->item;
                const DockHostId sourceHost = m_tabDrag->host;
                auto result = m_tabPressable.handle(context, event);
                if (result.activated && hit.item == pressed &&
                    hit.host == sourceHost) {
                    if (auto *owner = modelForHost(sourceHost)) {
                        static_cast<void>(owner->activateItem(pressed));
                    }
                }
                m_tabDrag.reset();
                m_pressedItem = {};
                m_dropGuides.clear();
                m_hoveredDrop.reset();
                result.reply.invalidate |=
                    WidgetInvalidation::layout | WidgetInvalidation::paint;
                return result.reply;
            }

            if (button->action == MouseButtonAction::press &&
                context.hasPointerPosition) {
                if (const auto close = hitClose(
                        context.bounds, context.state, context.pointerPosition);
                    close.item) {
                    clearTabInteraction();
                    clearResizeInteraction();
                    m_pressedClose = close;
                    m_hoveredClose = close;
                    return {.handled = true,
                            .stopPropagation = true,
                            .requestFocus = true,
                            .capturePointer = true,
                            .invalidate = WidgetInvalidation::paint};
                }
                if (const auto resize = floatingResizeAt(
                        context.pointerPosition, context.state);
                    resize) {
                    return beginFloatingResize(context, resize);
                }
                if (const DockHostId host = floatingHeaderAt(
                        context.pointerPosition, context.state);
                    host) {
                    return beginFloatingHeaderPress(context, host);
                }
            }

            const SplitHit divider = context.hasPointerPosition
                                         ? splitAt(context.pointerPosition)
                                         : SplitHit{};
            if ((button->action == MouseButtonAction::press ||
                 button->action == MouseButtonAction::doubleClick) &&
                divider.node) {
                auto *model = modelForHost(divider.host);
                if (button->action == MouseButtonAction::doubleClick) {
                    if (model != nullptr) {
                        model->setSplitRatio(divider.node, 0.5f);
                    }
                    return {.handled = true,
                            .stopPropagation = true,
                            .requestFocus = true,
                            .invalidate = WidgetInvalidation::layout |
                                          WidgetInvalidation::paint};
                }
                m_draggedSplit = divider;
                m_hoveredSplit = divider;
                return {.handled = true,
                        .stopPropagation = true,
                        .requestFocus = true,
                        .capturePointer = true,
                        .invalidate = WidgetInvalidation::paint};
            }
            if (button->action == MouseButtonAction::release &&
                m_draggedSplit.node) {
                m_draggedSplit = {};
                m_hoveredSplit = divider;
                return {.handled = true,
                        .stopPropagation = true,
                        .releasePointer = true,
                        .invalidate = WidgetInvalidation::paint};
            }

            const auto hit = context.hasPointerPosition
                                 ? hitTab(context.bounds,
                                          context.state,
                                          context.pointerPosition)
                                 : HitTab{};
            if ((button->action == MouseButtonAction::press ||
                 button->action == MouseButtonAction::doubleClick) &&
                !hit.item) {
                return {};
            }
            auto *hitModel = modelForHost(hit.host);
            if (button->action == MouseButtonAction::press) {
                m_pressedItem = hit.item;
                m_focusedHost = hit.host;
                m_focusedStack = hit.stack;
                m_tabDrag = TabDrag{
                    .item = hit.item,
                    .host = hit.host,
                    .pressPosition = context.pointerPosition,
                };
            } else if (button->action == MouseButtonAction::doubleClick) {
                m_pressedItem = hit.item;
                m_focusedHost = hit.host;
                m_focusedStack = hit.stack;
            }
            auto result = m_tabPressable.handle(context, event);
            if (button->action == MouseButtonAction::doubleClick &&
                result.activated && m_pressedItem &&
                hit.item == m_pressedItem && hitModel != nullptr) {
                hitModel->activateItem(m_pressedItem);
                m_pressedItem = {};
            }
            return result.reply;
        }

        if (const auto *focus = event.getIf<UIFocusChangedEvent>();
            focus != nullptr && !focus->focused &&
            (m_pressedClose.item || m_resizeDrag)) {
            clearCloseInteraction();
            clearResizeInteraction();
            return {.handled = true,
                    .releasePointer = true,
                    .invalidate = WidgetInvalidation::paint};
        }

        if (const auto *key = event.getIf<Input::KeyEvent>();
            key != nullptr && context.focused &&
            key->action == KeyAction::press && m_focusedStack) {
            auto *model = modelForHost(m_focusedHost);
            const auto *stack =
                model != nullptr ? model->getStack(m_focusedStack) : nullptr;
            if (stack != nullptr) {
                DockItemId next;
                if (key->key == KeyCode::arrowRight) {
                    next = stack->tabs.nextEnabled(stack->tabs.active(), 1);
                } else if (key->key == KeyCode::arrowLeft) {
                    next = stack->tabs.nextEnabled(stack->tabs.active(), -1);
                } else if (key->key == KeyCode::home) {
                    for (const auto &item : stack->tabs.items()) {
                        if (item.enabled) {
                            next = item.id;
                            break;
                        }
                    }
                } else if (key->key == KeyCode::end) {
                    const auto items = stack->tabs.items();
                    for (auto it = items.rbegin(); it != items.rend(); ++it) {
                        if (it->enabled) {
                            next = it->id;
                            break;
                        }
                    }
                }
                if (next) {
                    model->activateItem(next);
                    return {.handled = true,
                            .stopPropagation = true,
                            .invalidate = WidgetInvalidation::layout |
                                          WidgetInvalidation::paint};
                }
            }
        }

        return m_tabPressable.handle(context, event).reply;
    }

    DockSpaceModel &DockSpace::model() noexcept {
        return m_model;
    }

    const DockSpaceModel &DockSpace::model() const noexcept {
        return m_model;
    }

    DockPanelHandle DockSpace::createPanel(WidgetTree &state,
                                           WidgetId dockSpace,
                                           std::string title,
                                           std::unique_ptr<Widget> content,
                                           DockNodeId target,
                                           DockZone zone,
                                           bool closable) {
        if (state.getWidget<DockSpace>(dockSpace) != this) {
            return {};
        }
        if (!target && !m_model.empty()) {
            target = m_model.firstStack();
        }

        auto panel = std::make_unique<DockPanel>(title);
        const DockItemId itemId = panel->itemId();
        const WidgetId panelId = state.addWidget(std::move(panel), dockSpace);
        if (!panelId) {
            return {};
        }
        if (content && !state.addWidget(std::move(content), panelId)) {
            state.removeWidget(panelId);
            return {};
        }

        const DockItemId inserted = m_model.addItem({.id = itemId,
                                                     .title = std::move(title),
                                                     .content = panelId,
                                                     .closable = closable},
                                                    target,
                                                    zone);
        if (!inserted) {
            state.removeWidget(panelId);
            return {};
        }
        return DockPanelHandle{state, dockSpace, inserted, panelId};
    }

    bool DockSpace::removePanel(WidgetTree &state, DockItemId item) {
        if (state.getWidget<DockSpace>(m_mountedId) != this) {
            return false;
        }
        if (const auto hidden = m_hiddenPanels.find(item);
            hidden != m_hiddenPanels.end()) {
            const auto *entry = hidden->second.item.get();
            if (entry == nullptr || !entry->content) {
                return false;
            }
            const WidgetId panel = entry->content;
            if (state.getWidget<DockPanel>(panel) == nullptr) {
                m_hiddenPanels.erase(hidden);
                return false;
            }
            // Remove visibility metadata before widget callbacks can run so a
            // child cannot resurrect a panel during its own teardown.
            m_hiddenPanels.erase(hidden);
            const bool removed = state.removeWidget(panel);
            if (removed) {
                state.invalidate(m_mountedId,
                                 WidgetInvalidation::layout |
                                     WidgetInvalidation::paint);
            }
            return removed;
        }
        if (m_tabDrag && m_tabDrag->item == item) {
            clearTabInteraction();
            state.releasePointer(m_mountedId);
        }
        if (m_pressedClose.item == item) {
            clearCloseInteraction();
            state.releasePointer(m_mountedId);
        }
        if (m_hoveredItem == item) {
            m_hoveredItem = {};
        }
        if (m_hoveredClose.item == item) {
            m_hoveredClose = {};
        }
        DockHostId hostId;
        DockSpaceModel *owner = &m_model;
        if (m_model.getItem(item) == nullptr) {
            auto *host = findFloating(item);
            if (host == nullptr) {
                return false;
            }
            hostId = host->id;
            owner = &host->model;
        }
        const auto *dockItem = owner->getItem(item);
        if (dockItem == nullptr) {
            return false;
        }
        const WidgetId panel = dockItem->content;
        if (!owner->removeItem(item)) {
            return false;
        }
        removeFloatingHostIfEmpty(hostId);
        state.removeWidget(panel);
        state.invalidate(m_mountedId,
                         WidgetInvalidation::layout |
                             WidgetInvalidation::paint);
        return true;
    }

    bool DockSpace::setPanelTitle(WidgetTree &state,
                                  DockItemId item,
                                  std::string title) {
        if (state.getWidget<DockSpace>(m_mountedId) != this) {
            return false;
        }
        if (auto hidden = m_hiddenPanels.find(item);
            hidden != m_hiddenPanels.end()) {
            auto *dockItem = hidden->second.item.m_item
                                 ? &*hidden->second.item.m_item
                                 : nullptr;
            auto *panel = dockItem != nullptr
                              ? state.getWidget<DockPanel>(dockItem->content)
                              : nullptr;
            if (dockItem == nullptr || panel == nullptr) {
                return false;
            }
            dockItem->title = title;
            panel->setTitle(std::move(title));
            state.invalidate(m_mountedId, WidgetInvalidation::paint);
            return true;
        }
        DockSpaceModel *owner = &m_model;
        if (m_model.getItem(item) == nullptr) {
            auto *host = findFloating(item);
            owner = host != nullptr ? &host->model : nullptr;
        }
        const auto *dockItem =
            owner != nullptr ? owner->getItem(item) : nullptr;
        auto *panel = dockItem != nullptr
                          ? state.getWidget<DockPanel>(dockItem->content)
                          : nullptr;
        if (panel == nullptr) {
            return false;
        }
        panel->setTitle(title);
        const bool updated = owner->setItemTitle(item, std::move(title));
        if (updated) {
            state.invalidate(m_mountedId, WidgetInvalidation::paint);
        }
        return updated;
    }

    bool DockSpace::hidePanel(DockItemId item) {
        if (!item || m_mountedState == nullptr || !m_mountedId) {
            return false;
        }
        if (m_hiddenPanels.contains(item)) {
            return true;
        }

        FloatingHost *floatingHost = findFloating(item);
        DockSpaceModel *owner =
            floatingHost != nullptr ? &floatingHost->model : &m_model;
        const auto *entry = owner->getItem(item);
        const DockNodeId stackId = owner->stackForItem(item);
        const auto *stack = owner->getStack(stackId);
        if (entry == nullptr || stack == nullptr || !entry->content ||
            m_mountedState->getWidget<DockPanel>(entry->content) == nullptr) {
            return false;
        }

        auto [hiddenIt, inserted] = m_hiddenPanels.try_emplace(item);
        if (!inserted) {
            return true;
        }
        auto &hidden = hiddenIt->second;
        hidden.originHost =
            floatingHost != nullptr ? floatingHost->id : DockHostId{};
        hidden.stack = stackId;
        hidden.tabIndex = stack->tabs.indexOf(item);
        hidden.fallbackBounds =
            floatingHost != nullptr
                ? floatingHost->bounds
                : dockedFallbackBounds(stackId, *m_mountedState);

        auto detached = owner->detachItem(item);
        if (!detached) {
            m_hiddenPanels.erase(hiddenIt);
            return false;
        }
        hidden.item = std::move(detached);

        bool releasePointer = false;
        if ((m_tabDrag && m_tabDrag->item == item) || m_pressedItem == item) {
            clearTabInteraction();
            releasePointer = true;
        }
        if (m_pressedClose.item == item) {
            clearCloseInteraction();
            releasePointer = true;
        }
        if (m_hoveredItem == item) {
            m_hoveredItem = {};
        }
        if (m_hoveredClose.item == item) {
            m_hoveredClose = {};
        }
        if (releasePointer) {
            m_mountedState->releasePointer(m_mountedId);
        }

        const DockHostId previousHost = hidden.originHost;
        if (m_focusedHost == previousHost && m_focusedStack == stackId) {
            m_focusedStack = owner->firstStack();
            if (!m_focusedStack) {
                m_focusedHost = {};
            }
        }
        removeFloatingHostIfEmpty(previousHost);
        m_mountedState->invalidate(m_mountedId,
                                   WidgetInvalidation::layout |
                                       WidgetInvalidation::paint);
        return true;
    }

    bool DockSpace::showPanel(DockItemId item) {
        if (!item || m_mountedState == nullptr || !m_mountedId) {
            return false;
        }
        if (isPanelVisible(item)) {
            return true;
        }
        const auto hiddenIt = m_hiddenPanels.find(item);
        if (hiddenIt == m_hiddenPanels.end() || !hiddenIt->second.item) {
            return false;
        }

        auto &hidden = hiddenIt->second;
        const auto *entry = hidden.item.get();
        if (entry == nullptr || !entry->content ||
            m_mountedState->getWidget<DockPanel>(entry->content) == nullptr) {
            return false;
        }

        const auto destination = findStackOwner(hidden.stack);
        bool restored = false;
        if (destination.model != nullptr) {
            restored = destination.model->attachItem(std::move(hidden.item),
                                                     hidden.stack,
                                                     DockZone::main,
                                                     hidden.tabIndex);
            if (restored) {
                m_focusedHost = destination.host;
                m_focusedStack = destination.model->stackForItem(item);
            }
        }
        if (!restored) {
            restored = restoreHiddenAsFloating(hidden);
        }
        if (!restored) {
            return false;
        }

        m_hiddenPanels.erase(hiddenIt);
        m_mountedState->invalidate(m_mountedId,
                                   WidgetInvalidation::layout |
                                       WidgetInvalidation::paint);
        return true;
    }

    bool DockSpace::isPanelVisible(DockItemId item) const noexcept {
        return item && (m_model.getItem(item) != nullptr ||
                        findFloating(item) != nullptr);
    }

    bool DockSpace::isPanelHidden(DockItemId item) const noexcept {
        return item && m_hiddenPanels.contains(item);
    }

    size_t DockSpace::hiddenPanelCount() const noexcept {
        return m_hiddenPanels.size();
    }

    bool DockSpace::floatItem(DockItemId item, WidgetBounds bounds) {
        if (isItemFloating(item) || bounds.empty() ||
            !finiteVec(bounds.center) || !finiteVec(bounds.size)) {
            return false;
        }
        auto detached = m_model.detachItem(item);
        if (!detached) {
            return false;
        }
        auto host = std::make_unique<FloatingHost>();
        host->bounds = bounds;
        if (!host->model.attachItem(std::move(detached))) {
            static_cast<void>(m_model.attachItem(
                std::move(detached), m_model.firstStack(), DockZone::main));
            return false;
        }
        m_floatingHosts.push_back(std::move(host));
        return true;
    }

    bool DockSpace::dockFloatingItem(DockItemId item,
                                     DockNodeId target,
                                     DockZone zone,
                                     size_t tabIndex) {
        const auto *source = findFloating(item);
        if (source == nullptr) {
            return false;
        }
        if (!target && !m_model.empty()) {
            target = m_model.firstStack();
        }
        return transferItem(item,
                            source->id,
                            {.node = target,
                             .zone = zone,
                             .root = false,
                             .tabIndex = tabIndex});
    }

    bool DockSpace::isItemFloating(DockItemId item) const noexcept {
        return findFloating(item) != nullptr;
    }

    size_t DockSpace::floatingItemCount() const noexcept {
        size_t count = 0;
        for (const auto &host : m_floatingHosts) {
            count += host->model.itemCount();
        }
        return count;
    }

    size_t DockSpace::floatingWindowCount() const noexcept {
        return m_floatingHosts.size();
    }

    std::optional<WidgetBounds>
    DockSpace::floatingItemBounds(DockItemId item) const noexcept {
        const auto *host = findFloating(item);
        return host != nullptr ? std::optional{host->bounds} : std::nullopt;
    }

    const UIDockStyle &DockSpace::dockStyle(const WidgetTree &state) const {
        return m_options.dockStyle ? *m_options.dockStyle : state.theme().dock;
    }

    const UITabStyle &DockSpace::tabStyle(const WidgetTree &state) const {
        return m_options.tabStyle ? *m_options.tabStyle : state.theme().tabs;
    }

    DockLayoutResult DockSpace::calculateLayout(WidgetBounds bounds,
                                                const WidgetTree &state) const {
        return calculateLayout(m_model, bounds, state, false);
    }

    DockLayoutResult DockSpace::calculateLayout(const DockSpaceModel &model,
                                                WidgetBounds bounds,
                                                const WidgetTree &state,
                                                bool hideSingleTab) const {
        const auto &dock = dockStyle(state);
        const auto &tabs = tabStyle(state);
        const float tabHeight =
            hideSingleTab && model.itemCount() == 1 && model.stackCount() == 1
                ? 0.f
                : tabs.height;
        return model.layout(bounds, tabHeight, dock.splitterThickness);
    }

    DockLayoutResult
    DockSpace::calculateFloatingLayout(const FloatingHost &host,
                                       const WidgetTree &state) const {
        return calculateLayout(
            host.model, floatingClientBounds(host, state), state, true);
    }

    DockSpace::HitTab DockSpace::hitTab(WidgetBounds bounds,
                                        const WidgetTree &state,
                                        glm::vec2 position) const {
        for (auto it = m_floatingHosts.rbegin(); it != m_floatingHosts.rend();
             ++it) {
            const auto layout = calculateFloatingLayout(**it, state);
            if (auto hit =
                    hitTab((*it)->model, (*it)->id, layout, state, position);
                hit.item) {
                return hit;
            }
            if ((*it)->bounds.contains(position)) {
                return {};
            }
        }
        return hitTab(
            m_model, {}, calculateLayout(bounds, state), state, position);
    }

    DockSpace::HitTab DockSpace::hitTab(const DockSpaceModel &model,
                                        DockHostId host,
                                        const DockLayoutResult &layout,
                                        const WidgetTree &state,
                                        glm::vec2 position) const {
        const auto &tabs = tabStyle(state);
        for (const auto &stackLayout : layout.stacks) {
            if (stackLayout.tabBarBounds.size.y <= 0.f ||
                !stackLayout.tabBarBounds.contains(position)) {
                continue;
            }
            const auto *stack = model.getStack(stackLayout.node);
            if (stack == nullptr) {
                continue;
            }
            const auto regions = TabStripLayout::calculate(
                stackLayout.tabBarBounds, stack->tabs.size(), metrics(tabs));
            const auto index = TabStripLayout::hitTest(regions, position);
            const auto items = stack->tabs.items();
            if (index && *index < items.size() && items[*index].enabled) {
                return {.host = host,
                        .stack = stackLayout.node,
                        .item = items[*index].id};
            }
        }
        return {};
    }

    DockSpace::HitTab DockSpace::hitClose(WidgetBounds bounds,
                                          const WidgetTree &state,
                                          glm::vec2 position) const {
        for (auto it = m_floatingHosts.rbegin(); it != m_floatingHosts.rend();
             ++it) {
            const auto &host = **it;
            if (host.model.itemCount() == 1 && host.model.stackCount() == 1) {
                const DockItemId itemId = floatingTitleItem(host);
                const auto *item = host.model.getItem(itemId);
                if (item != nullptr && item->enabled && item->closable) {
                    const auto region = floatingHeaderRegion(host, state, true);
                    if (region.trailingActionBounds.contains(position)) {
                        return {
                            .host = host.id,
                            .stack = host.model.stackForItem(itemId),
                            .item = itemId,
                        };
                    }
                }
            }

            const auto layout = calculateFloatingLayout(host, state);
            if (auto hit =
                    hitClose(host.model, host.id, layout, state, position);
                hit.item) {
                return hit;
            }
            if (host.bounds.contains(position)) {
                return {};
            }
        }
        return hitClose(
            m_model, {}, calculateLayout(bounds, state), state, position);
    }

    DockSpace::HitTab DockSpace::hitClose(const DockSpaceModel &model,
                                          DockHostId host,
                                          const DockLayoutResult &layout,
                                          const WidgetTree &state,
                                          glm::vec2 position) const {
        const auto &tabs = tabStyle(state);
        for (const auto &stackLayout : layout.stacks) {
            if (stackLayout.tabBarBounds.size.y <= 0.f ||
                !stackLayout.tabBarBounds.contains(position)) {
                continue;
            }
            const auto *stack = model.getStack(stackLayout.node);
            if (stack == nullptr) {
                continue;
            }
            const auto regions = TabStripLayout::calculate(
                stackLayout.tabBarBounds, stack->tabs.size(), metrics(tabs));
            const auto items = stack->tabs.items();
            for (size_t i = 0; i < regions.size() && i < items.size(); ++i) {
                const auto &item = items[i];
                if (!item.enabled || !item.closable) {
                    continue;
                }
                const auto region = withCloseButton(
                    regions[i], tabs, tabs.closeButtonTrailingPadding);
                if (region.trailingActionBounds.contains(position)) {
                    return {
                        .host = host,
                        .stack = stackLayout.node,
                        .item = item.id,
                    };
                }
            }
        }
        return {};
    }

    TabStripRegion
    DockSpace::floatingHeaderRegion(const FloatingHost &host,
                                    const WidgetTree &state,
                                    bool reserveClose) const noexcept {
        const auto header = floatingHeaderBounds(host, state);
        const float padding = std::min(
            std::max(0.f, dockStyle(state).floatingTitleHorizontalPadding),
            header.size.x * 0.5f);
        TabStripRegion region{
            .bounds = header,
            .labelBounds =
                {
                    .center = header.center,
                    .size = {std::max(0.f, header.size.x - padding * 2.f),
                             header.size.y},
                },
        };
        return reserveClose ? withCloseButton(
                                  std::move(region), tabStyle(state), padding)
                            : region;
    }

    DockSpace::FloatingHost *DockSpace::findFloating(DockItemId item) noexcept {
        const auto it =
            std::find_if(m_floatingHosts.begin(),
                         m_floatingHosts.end(),
                         [item](const auto &host) {
                             return host->model.getItem(item) != nullptr;
                         });
        return it != m_floatingHosts.end() ? it->get() : nullptr;
    }

    const DockSpace::FloatingHost *
    DockSpace::findFloating(DockItemId item) const noexcept {
        const auto it =
            std::find_if(m_floatingHosts.begin(),
                         m_floatingHosts.end(),
                         [item](const auto &host) {
                             return host->model.getItem(item) != nullptr;
                         });
        return it != m_floatingHosts.end() ? it->get() : nullptr;
    }

    DockSpace::FloatingHost *
    DockSpace::findFloatingHost(DockHostId host) noexcept {
        const auto it = std::find_if(
            m_floatingHosts.begin(),
            m_floatingHosts.end(),
            [host](const auto &entry) { return entry->id == host; });
        return it != m_floatingHosts.end() ? it->get() : nullptr;
    }

    const DockSpace::FloatingHost *
    DockSpace::findFloatingHost(DockHostId host) const noexcept {
        const auto it = std::find_if(
            m_floatingHosts.begin(),
            m_floatingHosts.end(),
            [host](const auto &entry) { return entry->id == host; });
        return it != m_floatingHosts.end() ? it->get() : nullptr;
    }

    DockSpaceModel *DockSpace::modelForHost(DockHostId host) noexcept {
        if (!host) {
            return &m_model;
        }
        auto *floating = findFloatingHost(host);
        return floating != nullptr ? &floating->model : nullptr;
    }

    const DockSpaceModel *
    DockSpace::modelForHost(DockHostId host) const noexcept {
        if (!host) {
            return &m_model;
        }
        const auto *floating = findFloatingHost(host);
        return floating != nullptr ? &floating->model : nullptr;
    }

    DockSpace::StackOwner DockSpace::findStackOwner(DockNodeId stack) noexcept {
        if (!stack) {
            return {};
        }
        if (m_model.getStack(stack) != nullptr) {
            return {.model = &m_model};
        }
        for (const auto &host : m_floatingHosts) {
            if (host->model.getStack(stack) != nullptr) {
                return {.model = &host->model, .host = host->id};
            }
        }
        return {};
    }

    DockHostId
    DockSpace::floatingHeaderAt(glm::vec2 position,
                                const WidgetTree &state) const noexcept {
        for (auto it = m_floatingHosts.rbegin(); it != m_floatingHosts.rend();
             ++it) {
            if (floatingHeaderBounds(**it, state).contains(position)) {
                return (*it)->id;
            }
            if ((*it)->bounds.contains(position)) {
                return {};
            }
        }
        return {};
    }

    DockSpace::FloatingResizeHit
    DockSpace::floatingResizeAt(glm::vec2 position,
                                const WidgetTree &state) const noexcept {
        const auto &style = dockStyle(state);
        const float requestedThickness =
            std::isfinite(style.floatingResizeHandleThickness)
                ? style.floatingResizeHandleThickness
                : 0.f;
        const float thickness = std::clamp(requestedThickness, 1.f, 24.f);
        const float requestedCorner =
            std::isfinite(style.floatingResizeCornerSize)
                ? style.floatingResizeCornerSize
                : thickness;
        const float corner = std::clamp(requestedCorner, thickness, 64.f);

        for (auto it = m_floatingHosts.rbegin(); it != m_floatingHosts.rend();
             ++it) {
            const auto &host = **it;
            const auto expanded = host.bounds.inset(-thickness);
            if (!expanded.contains(position)) {
                continue;
            }

            const auto topLeft = host.bounds.topLeft();
            const auto bottomRight = host.bounds.bottomRight();
            const bool nearLeft = std::abs(position.x - topLeft.x) <= thickness;
            const bool nearRight =
                std::abs(position.x - bottomRight.x) <= thickness;
            const bool nearTop = std::abs(position.y - topLeft.y) <= thickness;
            const bool nearBottom =
                std::abs(position.y - bottomRight.y) <= thickness;

            FloatingResizeEdges edges{
                .left = nearLeft,
                .right = nearRight,
                .top = nearTop,
                .bottom = nearBottom,
            };
            // Extend diagonal targets along each adjoining edge. This mirrors
            // native desktop windows: corners are easier to acquire than the
            // thin edge itself, without consuming header/content hit space.
            if (nearTop && position.x <= topLeft.x + corner) {
                edges.left = true;
            }
            if (nearTop && position.x >= bottomRight.x - corner) {
                edges.right = true;
            }
            if (nearBottom && position.x <= topLeft.x + corner) {
                edges.left = true;
            }
            if (nearBottom && position.x >= bottomRight.x - corner) {
                edges.right = true;
            }
            if (nearLeft && position.y <= topLeft.y + corner) {
                edges.top = true;
            }
            if (nearLeft && position.y >= bottomRight.y - corner) {
                edges.bottom = true;
            }
            if (nearRight && position.y <= topLeft.y + corner) {
                edges.top = true;
            }
            if (nearRight && position.y >= bottomRight.y - corner) {
                edges.bottom = true;
            }

            if (edges.any()) {
                return {.host = host.id, .edges = edges};
            }
            // A front floating host occludes resize chrome belonging to hosts
            // below it, even though all hosts are owned by one DockSpace.
            if (host.bounds.contains(position)) {
                return {};
            }
        }
        return {};
    }

    DockItemId
    DockSpace::floatingTitleItem(const FloatingHost &host) const noexcept {
        const auto *stack = host.model.getStack(host.model.firstStack());
        return stack != nullptr ? stack->tabs.active() : DockItemId{};
    }

    WidgetBounds
    DockSpace::floatingHeaderBounds(const FloatingHost &host,
                                    const WidgetTree &state) const noexcept {
        const float height =
            std::min(std::max(0.f, dockStyle(state).floatingTitleBarHeight),
                     host.bounds.size.y);
        return {
            .center = {host.bounds.center.x,
                       host.bounds.topLeft().y + height * 0.5f},
            .size = {host.bounds.size.x, height},
        };
    }

    WidgetBounds
    DockSpace::floatingClientBounds(const FloatingHost &host,
                                    const WidgetTree &state) const noexcept {
        const auto &style = dockStyle(state);
        const float border = std::max({0.f,
                                       style.floatingWindow.borderThickness.x,
                                       style.floatingWindow.borderThickness.y,
                                       style.floatingWindow.borderThickness.z,
                                       style.floatingWindow.borderThickness.w});
        const auto header = floatingHeaderBounds(host, state);
        const float height =
            std::max(0.f, host.bounds.size.y - header.size.y - border * 2.f);
        return {
            .center = {host.bounds.center.x,
                       host.bounds.topLeft().y + header.size.y + border +
                           height * 0.5f},
            .size = {std::max(0.f, host.bounds.size.x - border * 2.f), height},
        };
    }

    WidgetBounds DockSpace::normalizedFloatingBounds(
        WidgetBounds requested,
        WidgetBounds dockBounds,
        const WidgetTree &state) const noexcept {
        if (dockBounds.empty()) {
            return {.center = dockBounds.center, .size = {0.f, 0.f}};
        }
        const auto &style = dockStyle(state);
        const glm::vec2 available = glm::max(dockBounds.size, glm::vec2{1.f});
        const auto limits = floatingSizeLimits(style, available);
        requested.size = glm::clamp(glm::max(requested.size, glm::vec2{1.f}),
                                    limits.minimum,
                                    limits.maximum);

        // Windows may leave the DockSpace and are clipped by its child clip.
        // Retaining a small title-bar grip prevents an unrecoverable window
        // without destroying the out-of-bounds movement illusion.
        const float margin = std::max(0.f, style.floatingMargin);
        const glm::vec2 effectiveMargin =
            glm::min(glm::vec2{margin},
                     glm::max((dockBounds.size - glm::vec2{1.f}) * 0.5f,
                              glm::vec2{0.f}));
        const auto min = dockBounds.topLeft() + effectiveMargin;
        const auto max = dockBounds.bottomRight() - effectiveMargin;
        const auto half = requested.size * 0.5f;
        const float visibleWidth =
            std::min({std::max(1.f, style.floatingVisibleTitleWidth),
                      requested.size.x,
                      std::max(1.f, max.x - min.x)});
        const float titleHeight = std::min(
            std::max(1.f, style.floatingTitleBarHeight), requested.size.y);
        const float visibleHeight =
            std::min({std::max(1.f, style.floatingVisibleTitleHeight),
                      titleHeight,
                      std::max(1.f, max.y - min.y)});
        requested.center.x = std::clamp(requested.center.x,
                                        min.x - half.x + visibleWidth,
                                        max.x + half.x - visibleWidth);
        requested.center.y =
            std::clamp(requested.center.y,
                       min.y + half.y - titleHeight + visibleHeight,
                       max.y + half.y - visibleHeight);
        return requested;
    }

    bool DockSpace::beginTabDrag(WidgetEventContext &context,
                                 WidgetBounds dockBounds) {
        if (!m_tabDrag || m_tabDrag->window) {
            return false;
        }
        DockSpaceModel *sourceModel = modelForHost(m_tabDrag->host);
        if (sourceModel == nullptr) {
            return false;
        }
        if (m_tabDrag->host && sourceModel->itemCount() == 1) {
            auto *sourceHost = findFloatingHost(m_tabDrag->host);
            if (sourceHost == nullptr) {
                return false;
            }
            m_tabDrag->grabOffset =
                m_tabDrag->pressPosition - sourceHost->bounds.center;
            m_tabDrag->window = true;
            m_tabDrag->started = true;
            updateFloatingDrag(context);
            return true;
        }

        const DockNodeId sourceStack =
            sourceModel->stackForItem(m_tabDrag->item);
        const FloatingHost *sourceHost = findFloatingHost(m_tabDrag->host);
        const auto sourceLayout =
            sourceHost != nullptr
                ? calculateFloatingLayout(*sourceHost, context.state)
                : calculateLayout(dockBounds, context.state);
        const auto *source = sourceLayout.findStack(sourceStack);
        if (source == nullptr) {
            return false;
        }

        WidgetBounds floatingBounds = source->bounds;
        const auto &style = dockStyle(context.state);
        const auto limits = floatingSizeLimits(style, dockBounds.size);
        floatingBounds.size =
            glm::clamp(glm::max(floatingBounds.size, glm::vec2{1.f}),
                       limits.minimum,
                       limits.maximum);
        floatingBounds.center =
            source->bounds.topLeft() + floatingBounds.size * 0.5f;
        floatingBounds =
            normalizedFloatingBounds(floatingBounds, dockBounds, context.state);

        auto detached = sourceModel->detachItem(m_tabDrag->item);
        if (!detached) {
            return false;
        }
        auto host = std::make_unique<FloatingHost>();
        host->bounds = floatingBounds;
        if (!host->model.attachItem(std::move(detached))) {
            static_cast<void>(sourceModel->attachItem(std::move(detached),
                                                      sourceModel->firstStack(),
                                                      DockZone::main));
            return false;
        }
        const DockHostId previousHost = m_tabDrag->host;
        m_tabDrag->host = host->id;
        m_tabDrag->grabOffset = m_tabDrag->pressPosition - host->bounds.center;
        m_tabDrag->window = true;
        m_tabDrag->started = true;
        m_floatingHosts.push_back(std::move(host));
        removeFloatingHostIfEmpty(previousHost);
        m_tabPressable.reset();
        updateFloatingDrag(context);
        return true;
    }

    void DockSpace::updateFloatingDrag(WidgetEventContext &context) {
        if (!m_tabDrag || !m_tabDrag->window) {
            return;
        }
        auto *host = findFloatingHost(m_tabDrag->host);
        if (host == nullptr) {
            clearTabInteraction();
            return;
        }
        host->bounds.center = context.pointerPosition - m_tabDrag->grabOffset;
        host->bounds = normalizedFloatingBounds(
            host->bounds, context.bounds, context.state);
        refreshDropGuides(
            context.bounds, context.state, context.pointerPosition);
    }

    void DockSpace::updateFloatingResize(WidgetEventContext &context) {
        if (!m_resizeDrag || !context.hasPointerPosition) {
            return;
        }
        auto *host = findFloatingHost(m_resizeDrag->hit.host);
        if (host == nullptr) {
            clearResizeInteraction();
            return;
        }

        const auto &style = dockStyle(context.state);
        const glm::vec2 available =
            glm::max(context.bounds.size, glm::vec2{1.f});
        const auto limits = floatingSizeLimits(style, available);

        const auto &initial = m_resizeDrag->initialBounds;
        const glm::vec2 delta =
            context.pointerPosition - m_resizeDrag->pressPosition;
        float left = initial.topLeft().x;
        float right = initial.bottomRight().x;
        float top = initial.topLeft().y;
        float bottom = initial.bottomRight().y;
        const auto &edges = m_resizeDrag->hit.edges;

        if (edges.left) {
            left = std::clamp(left + delta.x,
                              right - limits.maximum.x,
                              right - limits.minimum.x);
        } else if (edges.right) {
            right = std::clamp(right + delta.x,
                               left + limits.minimum.x,
                               left + limits.maximum.x);
        }
        if (edges.top) {
            top = std::clamp(top + delta.y,
                             bottom - limits.maximum.y,
                             bottom - limits.minimum.y);
        } else if (edges.bottom) {
            bottom = std::clamp(bottom + delta.y,
                                top + limits.minimum.y,
                                top + limits.maximum.y);
        }

        host->bounds = normalizedFloatingBounds(
            {
                .center = {(left + right) * 0.5f, (top + bottom) * 0.5f},
                .size = {right - left, bottom - top},
            },
            context.bounds,
            context.state);
        m_hoveredResize = m_resizeDrag->hit;
    }

    bool DockSpace::finishFloatingDrag() {
        if (!m_tabDrag || !m_tabDrag->window || !m_hoveredDrop) {
            return false;
        }
        const auto *source = findFloatingHost(m_tabDrag->host);
        if (source == nullptr || source->id == m_hoveredDrop->host) {
            return false;
        }
        return source->model.itemCount() == 1
                   ? transferItem(m_tabDrag->item, source->id, *m_hoveredDrop)
                   : transferHost(source->id, *m_hoveredDrop);
    }

    void DockSpace::refreshDropGuides(WidgetBounds bounds,
                                      const WidgetTree &state,
                                      glm::vec2 position) {
        m_dropGuides.clear();
        m_hoveredDrop.reset();
        if (!bounds.contains(position) || !m_tabDrag) {
            return;
        }

        const DockHostId source = m_tabDrag->host;
        const auto guideMetrics = dropGuideMetrics(state);
        if (m_model.empty()) {
            auto emptyGuide =
                DockDropGuideLayoutSolver::calculate(bounds, {}, guideMetrics);
            std::erase_if(emptyGuide.regions, [](const DockDropRegion &region) {
                return region.zone != DockZone::main;
            });
            m_dropGuides.push_back({.layout = std::move(emptyGuide)});
        } else {
            m_dropGuides.push_back({
                .root = true,
                .layout = DockDropGuideLayoutSolver::calculateRootEdges(
                    bounds, m_model.root(), guideMetrics),
            });
        }

        for (const auto &host : m_floatingHosts) {
            if (host->id == source) {
                continue;
            }
            const auto client = floatingClientBounds(*host, state);
            m_dropGuides.push_back({
                .host = host->id,
                .root = true,
                .layout = DockDropGuideLayoutSolver::calculateRootEdges(
                    client, host->model.root(), guideMetrics),
            });
        }

        // Root edges have priority: their preview always means "beside the
        // whole host", even when a small leaf's local guide overlaps it.
        for (auto it = m_dropGuides.rbegin(); it != m_dropGuides.rend(); ++it) {
            if (!it->root) {
                continue;
            }
            if (const auto *region = it->layout.regionAt(position)) {
                m_hoveredDrop = DropDestination{.host = it->host,
                                                .node = it->layout.target,
                                                .zone = region->zone,
                                                .root = true};
                break;
            }
        }

        DockHostId candidateHost;
        WidgetBounds candidateBounds = bounds;
        const DockSpaceModel *candidateModel = &m_model;
        DockLayoutResult candidateLayout = calculateLayout(bounds, state);
        for (auto it = m_floatingHosts.rbegin(); it != m_floatingHosts.rend();
             ++it) {
            if ((*it)->id == source || !(*it)->bounds.contains(position)) {
                continue;
            }
            candidateHost = (*it)->id;
            candidateBounds = floatingClientBounds(**it, state);
            candidateModel = &(*it)->model;
            candidateLayout = calculateFloatingLayout(**it, state);
            break;
        }

        DockNodeId target = candidateLayout.stackAt(position);
        const DockStackLayout *stack = candidateLayout.findStack(target);
        if (stack == nullptr && candidateModel != nullptr &&
            !candidateModel->empty() && candidateBounds.contains(position)) {
            target = candidateModel->firstStack();
            stack = candidateLayout.findStack(target);
        }
        if (stack != nullptr) {
            auto nodeGuide = DockDropGuideLayoutSolver::calculate(
                stack->bounds, target, guideMetrics);
            const auto *sourceHost = findFloatingHost(source);
            if (sourceHost != nullptr && sourceHost->model.stackCount() > 1) {
                std::erase_if(nodeGuide.regions,
                              [](const DockDropRegion &region) {
                                  return region.zone == DockZone::main;
                              });
            }
            m_dropGuides.push_back({
                .host = candidateHost,
                .layout = std::move(nodeGuide),
            });
            if (!m_hoveredDrop) {
                if (const auto *region =
                        m_dropGuides.back().layout.regionAt(position)) {
                    m_hoveredDrop = DropDestination{.host = candidateHost,
                                                    .node = target,
                                                    .zone = region->zone};
                }
            }
        } else if (candidateModel != nullptr && candidateModel->empty()) {
            const auto &guide = m_dropGuides.front();
            if (const auto *region = guide.layout.regionAt(position)) {
                m_hoveredDrop = DropDestination{.zone = region->zone};
            }
        }
    }

    bool DockSpace::transferItem(DockItemId item,
                                 DockHostId source,
                                 const DropDestination &destination) {
        if (source == destination.host) {
            return false;
        }
        auto *sourceModel = modelForHost(source);
        auto *destinationModel = modelForHost(destination.host);
        if (sourceModel == nullptr || destinationModel == nullptr ||
            sourceModel->getItem(item) == nullptr) {
            return false;
        }

        auto detached = sourceModel->detachItem(item);
        if (!detached) {
            return false;
        }
        if (!attachDetached(
                *destinationModel, std::move(detached), destination)) {
            static_cast<void>(sourceModel->attachItem(std::move(detached),
                                                      sourceModel->firstStack(),
                                                      DockZone::main));
            return false;
        }
        removeFloatingHostIfEmpty(source);
        m_focusedHost = destination.host;
        m_focusedStack = destinationModel->stackForItem(item);
        if (m_mountedState != nullptr) {
            m_mountedState->invalidate(m_mountedId,
                                       WidgetInvalidation::layout |
                                           WidgetInvalidation::paint);
        }
        return true;
    }

    bool DockSpace::transferHost(DockHostId source,
                                 const DropDestination &destination) {
        if (!source || source == destination.host) {
            return false;
        }
        auto *sourceHost = findFloatingHost(source);
        auto *destinationModel = modelForHost(destination.host);
        if (sourceHost == nullptr || destinationModel == nullptr) {
            return false;
        }
        const auto items = sourceHost->model.itemIds();
        if (items.empty()) {
            return false;
        }

        if (sourceHost->model.stackCount() > 1) {
            bool attached = false;
            if (destinationModel->empty()) {
                attached =
                    destinationModel->attachTree(std::move(sourceHost->model));
            } else if (destination.root) {
                attached = destinationModel->attachTreeAtRoot(
                    std::move(sourceHost->model), destination.zone);
            } else {
                attached =
                    destinationModel->attachTree(std::move(sourceHost->model),
                                                 destination.node,
                                                 destination.zone);
            }
            if (!attached) {
                return false;
            }
            removeFloatingHostIfEmpty(source);
            m_focusedHost = destination.host;
            m_focusedStack = destinationModel->stackForItem(items.front());
            if (m_mountedState != nullptr) {
                m_mountedState->invalidate(m_mountedId,
                                           WidgetInvalidation::layout |
                                               WidgetInvalidation::paint);
            }
            return true;
        }

        DockNodeId mergedStack;
        std::vector<DockItemId> transferred;
        for (const auto item : items) {
            auto detached = sourceHost->model.detachItem(item);
            if (!detached) {
                break;
            }
            const bool attached = transferred.empty()
                                      ? attachDetached(*destinationModel,
                                                       std::move(detached),
                                                       destination)
                                      : attachDetached(*destinationModel,
                                                       std::move(detached),
                                                       destination,
                                                       mergedStack);
            if (!attached) {
                static_cast<void>(
                    sourceHost->model.attachItem(std::move(detached),
                                                 sourceHost->model.firstStack(),
                                                 DockZone::main));
                break;
            }
            if (transferred.empty()) {
                mergedStack = destinationModel->stackForItem(item);
            }
            transferred.push_back(item);
        }

        if (transferred.size() != items.size()) {
            // Defensive rollback. The original split shape cannot be
            // reconstructed without a serialized tree token, but ownership
            // and every DockItem identity remain intact.
            for (const auto item : transferred) {
                auto detached = destinationModel->detachItem(item);
                if (detached) {
                    static_cast<void>(sourceHost->model.attachItem(
                        std::move(detached),
                        sourceHost->model.firstStack(),
                        DockZone::main));
                }
            }
            return false;
        }

        removeFloatingHostIfEmpty(source);
        m_focusedHost = destination.host;
        m_focusedStack = mergedStack;
        if (m_mountedState != nullptr) {
            m_mountedState->invalidate(m_mountedId,
                                       WidgetInvalidation::layout |
                                           WidgetInvalidation::paint);
        }
        return true;
    }

    bool DockSpace::attachDetached(DockSpaceModel &destination,
                                   DetachedDockItem &&item,
                                   const DropDestination &drop,
                                   DockNodeId overrideTarget) {
        if (destination.empty()) {
            return destination.attachItem(std::move(item));
        }
        if (overrideTarget) {
            return destination.attachItem(
                std::move(item), overrideTarget, DockZone::main);
        }
        if (drop.root) {
            return destination.attachItemAtRoot(std::move(item), drop.zone);
        }
        return destination.attachItem(
            std::move(item), drop.node, drop.zone, drop.tabIndex);
    }

    bool DockSpace::restoreHiddenAsFloating(HiddenPanel &hidden) {
        if (m_mountedState == nullptr || !m_mountedId || !hidden.item) {
            return false;
        }
        const WidgetBounds dockBounds = m_mountedState->getBounds(m_mountedId);
        WidgetBounds requested = hidden.fallbackBounds;
        if (requested.empty() || !finiteVec(requested.center) ||
            !finiteVec(requested.size)) {
            requested = {
                .center = dockBounds.center,
                .size = glm::max(dockStyle(*m_mountedState).floatingMinimumSize,
                                 glm::vec2{1.f}),
            };
        }

        auto host = std::make_unique<FloatingHost>();
        host->bounds = dockBounds.empty()
                           ? requested
                           : normalizedFloatingBounds(
                                 requested, dockBounds, *m_mountedState);
        m_floatingHosts.push_back(std::move(host));
        auto &created = *m_floatingHosts.back();
        if (!created.model.attachItem(std::move(hidden.item))) {
            m_floatingHosts.pop_back();
            return false;
        }
        m_focusedHost = created.id;
        m_focusedStack = created.model.firstStack();
        return true;
    }

    WidgetBounds
    DockSpace::dockedFallbackBounds(DockNodeId stack,
                                    const WidgetTree &state) const {
        const WidgetBounds dockBounds = state.getBounds(m_mountedId);
        const auto layout = calculateLayout(dockBounds, state);
        if (const auto *stackLayout = layout.findStack(stack);
            stackLayout != nullptr && !stackLayout->bounds.empty()) {
            return stackLayout->bounds;
        }
        return {
            .center = dockBounds.center,
            .size =
                glm::max(dockStyle(state).floatingMinimumSize, glm::vec2{1.f}),
        };
    }

    void DockSpace::removeFloatingHostIfEmpty(DockHostId host) {
        if (!host) {
            return;
        }
        const auto it = std::find_if(
            m_floatingHosts.begin(),
            m_floatingHosts.end(),
            [host](const auto &entry) { return entry->id == host; });
        if (it != m_floatingHosts.end() && (*it)->model.empty()) {
            m_floatingHosts.erase(it);
            const bool wasResizing =
                m_resizeDrag && m_resizeDrag->hit.host == host;
            if (wasResizing || m_hoveredResize.host == host) {
                clearResizeInteraction();
                if (wasResizing && m_mountedState != nullptr) {
                    m_mountedState->releasePointer(m_mountedId);
                }
            }
        }
    }

    void DockSpace::clearTabInteraction() noexcept {
        m_tabDrag.reset();
        m_dropGuides.clear();
        m_hoveredDrop.reset();
        m_pressedItem = {};
        m_tabPressable.reset();
    }

    void DockSpace::clearCloseInteraction() noexcept {
        m_hoveredClose = {};
        m_pressedClose = {};
    }

    void DockSpace::clearResizeInteraction() noexcept {
        m_hoveredResize = {};
        m_resizeDrag.reset();
    }

    void DockSpace::bringFloatingToFront(WidgetEventContext &context,
                                         DockHostId host) {
        const auto it = std::find_if(
            m_floatingHosts.begin(),
            m_floatingHosts.end(),
            [host](const auto &entry) { return entry->id == host; });
        if (it == m_floatingHosts.end()) {
            return;
        }
        if (std::next(it) != m_floatingHosts.end()) {
            auto moved = std::move(*it);
            m_floatingHosts.erase(it);
            m_floatingHosts.push_back(std::move(moved));
        }
        const auto *front = findFloatingHost(host);
        if (front == nullptr) {
            return;
        }
        for (const auto item : front->model.itemIds()) {
            const auto *entry = front->model.getItem(item);
            if (entry != nullptr && entry->content) {
                static_cast<void>(context.state.reparentWidget(
                    entry->content, context.id, WidgetTree::append));
            }
        }
    }

    UIEventReply DockSpace::beginFloatingResize(WidgetEventContext &context,
                                                FloatingResizeHit hit) {
        if (!hit) {
            return {};
        }
        bringFloatingToFront(context, hit.host);
        const auto *entry = findFloatingHost(hit.host);
        if (entry == nullptr) {
            return {};
        }

        clearTabInteraction();
        clearCloseInteraction();
        m_draggedSplit = {};
        m_hoveredSplit = {};
        m_focusedHost = hit.host;
        m_focusedStack = entry->model.firstStack();
        m_hoveredResize = hit;
        m_resizeDrag = FloatingResizeDrag{
            .hit = hit,
            .initialBounds = entry->bounds,
            .pressPosition = context.pointerPosition,
        };
        return {.handled = true,
                .stopPropagation = true,
                .requestFocus = true,
                .capturePointer = true,
                .invalidate = WidgetInvalidation::paint};
    }

    UIEventReply
    DockSpace::beginFloatingHeaderPress(WidgetEventContext &context,
                                        DockHostId host) {
        bringFloatingToFront(context, host);
        const auto *entry = findFloatingHost(host);
        if (entry == nullptr) {
            return {};
        }
        clearResizeInteraction();
        m_tabPressable.reset();
        m_pressedItem = floatingTitleItem(*entry);
        m_focusedHost = host;
        m_focusedStack = entry->model.stackForItem(m_pressedItem);
        m_tabDrag = TabDrag{
            .item = m_pressedItem,
            .host = host,
            .pressPosition = context.pointerPosition,
            .grabOffset = context.pointerPosition - entry->bounds.center,
            .window = true,
        };
        return {.handled = true,
                .stopPropagation = true,
                .requestFocus = true,
                .capturePointer = true,
                .invalidate = WidgetInvalidation::paint};
    }

    DockDropGuideMetrics
    DockSpace::dropGuideMetrics(const WidgetTree &state) const noexcept {
        const auto &style = dockStyle(state);
        return {
            .indicatorSize = style.dropGuideSize,
            .indicatorGap = style.dropGuideGap,
            .previewInset = style.dropPreviewInset,
        };
    }

    DockPanelHandle::DockPanelHandle(WidgetTree &state,
                                     WidgetId dockSpace,
                                     DockItemId itemId,
                                     WidgetId panelId) noexcept
        : item(itemId),
          panel(panelId),
          m_dockSpace(state, dockSpace) {
    }

    DockPanelHandle::operator bool() const noexcept {
        const auto *state = m_dockSpace.tree();
        return item && panel && m_dockSpace && state != nullptr &&
               state->getWidget<DockPanel>(panel) != nullptr;
    }

    WidgetRef<DockPanel> DockPanelHandle::widget() const noexcept {
        auto *state = m_dockSpace.tree();
        return state != nullptr ? WidgetRef<DockPanel>{*state, panel}
                                : WidgetRef<DockPanel>{};
    }

    bool DockPanelHandle::hide() const {
        auto *dockSpace = m_dockSpace.get();
        return dockSpace != nullptr && dockSpace->hidePanel(item);
    }

    bool DockPanelHandle::show() const {
        auto *dockSpace = m_dockSpace.get();
        return dockSpace != nullptr && dockSpace->showPanel(item);
    }

    bool DockPanelHandle::isVisible() const noexcept {
        const auto *dockSpace = m_dockSpace.get();
        return dockSpace != nullptr && dockSpace->isPanelVisible(item);
    }

    bool DockPanelHandle::isHidden() const noexcept {
        const auto *dockSpace = m_dockSpace.get();
        return dockSpace != nullptr && dockSpace->isPanelHidden(item);
    }
} // namespace Bess::UI
