#include "controls/dock_space.h"

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

        bool finiteVec(glm::vec2 value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        constexpr float floatingLayer(size_t index) noexcept {
            return 0.10f + static_cast<float>(index) * 0.025f;
        }

        WidgetBounds zoneGlyph(WidgetBounds bounds, DockZone zone) noexcept {
            const float inset = std::max(4.f, bounds.size.x * 0.24f);
            return DockDropGuideLayoutSolver::regionBounds(bounds, zone, inset);
        }
    } // namespace

    DockPanel::DockPanel(std::string title, DockPanelOptions options)
        : m_title(std::move(title)),
          m_options(std::move(options)) {
    }

    std::string_view DockPanel::typeName() const noexcept {
        return "DockPanel";
    }

    WidgetTraits DockPanel::traits() const noexcept {
        return {.focusable = false,
                .hitTestVisible = false,
                .clipChildren = m_options.clipContent};
    }

    void DockPanel::onMount(WidgetMountContext &context) {
        // DockSpace is the sole geometry authority for panels. Removing them
        // from normal flex flow prevents hidden/inactive panels from imposing
        // a combined intrinsic minimum width on the dock space during a
        // viewport shrink; arrange() supplies the real bounds afterward.
        context.layout.setPosMode(PosMode::absolute);
        context.layout.setWidth(0.f);
        context.layout.setHeight(0.f);
    }

    void DockPanel::arrange(WidgetArrangeContext &context) {
        for (const auto child : context.children()) {
            context.setChildBounds(child, context.bounds);
        }
    }

    void DockPanel::paint(WidgetPaintContext &context) const {
        if (m_options.background) {
            context.painter.drawBox(makeBox(
                context.bounds, *m_options.background, context.pickingId));
        }
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
    }

    void DockSpace::arrange(WidgetArrangeContext &context) {
        for (const auto child : context.children()) {
            if (context.state.getWidget<DockPanel>(child) != nullptr) {
                context.setChildVisible(child, false);
                if (m_model.itemForContent(child)) {
                    if (auto *layout = context.state.getLayout(child)) {
                        layout->setZVal(0.f);
                    }
                }
            }
        }

        const auto layout = calculateLayout(context.bounds, context.state);
        for (const auto &stackLayout : layout.stacks) {
            const auto *item = m_model.getItem(stackLayout.activeItem);
            if (item == nullptr || !context.isDirectChild(item->content)) {
                continue;
            }
            context.setChildBounds(item->content, stackLayout.contentBounds);
            context.setChildVisible(item->content, true);
        }

        for (size_t i = 0; i < m_floatingItems.size(); ++i) {
            auto &floating = m_floatingItems[i];
            floating.bounds = normalizedFloatingBounds(
                floating.bounds, context.bounds, context.state);
            const auto *item = floating.detached.get();
            if (item == nullptr || !context.isDirectChild(item->content)) {
                continue;
            }
            context.setChildBounds(
                item->content, floatingContentBounds(floating, context.state));
            context.setChildVisible(item->content, true);
            if (auto *layout = context.state.getLayout(item->content)) {
                layout->setZVal(floatingLayer(i));
            }
        }
    }

    void DockSpace::paint(WidgetPaintContext &context) const {
        const auto &dock = dockStyle(context.state);
        const auto &tabs = tabStyle(context.state);
        context.painter.drawBox(
            makeBox(context.bounds, dock.background, context.pickingId));

        const auto layout = calculateLayout(context.bounds, context.state);
        uint32_t pickingInfo = 1;
        for (const auto &stackLayout : layout.stacks) {
            context.painter.drawBox(
                makeBox(stackLayout.bounds, dock.stack, context.pickingId));
            context.painter.drawBox(makeBox(stackLayout.tabBarBounds,
                                            tabs.strip,
                                            context.pickingId,
                                            0.001f));

            const auto *stack = m_model.getStack(stackLayout.node);
            if (stack == nullptr) {
                continue;
            }
            const auto regions = TabStripLayout::calculate(
                stackLayout.tabBarBounds, stack->tabs.size(), metrics(tabs));
            const auto items = stack->tabs.items();
            for (size_t i = 0; i < regions.size() && i < items.size(); ++i) {
                const auto &item = items[i];
                const UIBoxStyle *style = &tabs.normal;
                if (m_pressedItem == item.id && m_tabPressable.isPressed()) {
                    style = &tabs.pressed;
                } else if (stack->tabs.active() == item.id) {
                    style = &tabs.active;
                } else if (m_hoveredItem == item.id) {
                    style = &tabs.hovered;
                }
                PickingId id = context.pickingId;
                id.info = pickingInfo++;
                context.painter.drawBox(
                    makeBox(regions[i].bounds, *style, id, 0.002f));
                const auto textColor = stack->tabs.active() == item.id
                                           ? tabs.text.color
                                           : tabs.inactiveText;
                context.painter.drawText(
                    item.title,
                    {
                        .bounds = regions[i].labelBounds,
                        .fontSize = tabs.text.fontSize,
                        .color = textColor,
                        .horizontal = HorizontalTextAlignment::start,
                        .vertical = VerticalTextAlignment::center,
                        .zIndex = 0.003f,
                        .letterSpacing = tabs.text.letterSpacing,
                        .pickingId = id,
                    });
            }
        }

        for (const auto &splitLayout : layout.splits) {
            const auto color = splitLayout.node == m_hoveredSplit ||
                                       splitLayout.node == m_draggedSplit
                                   ? dock.splitterHovered
                                   : dock.splitter;
            context.painter.drawBox({
                .bounds = splitLayout.dividerBounds,
                .color = color,
                .zIndex = 0.004f,
                .pickingId = context.pickingId,
            });
        }

        // Floating panels are painted after dock chrome, and their content
        // widgets are kept at the end of the child order. Header chrome and
        // outlines are repeated in paintOverlay so docked content can never
        // obscure them.
        for (size_t i = 0; i < m_floatingItems.size(); ++i) {
            const auto &floating = m_floatingItems[i];
            context.painter.drawBox(makeBox(floating.bounds,
                                            dock.floatingWindow,
                                            context.pickingId,
                                            floatingLayer(i) - 0.005f));
        }
    }

    void DockSpace::paintOverlay(WidgetPaintContext &context) const {
        const auto &dock = dockStyle(context.state);
        const auto &tabs = tabStyle(context.state);
        for (size_t i = 0; i < m_floatingItems.size(); ++i) {
            const auto &floating = m_floatingItems[i];
            const auto *item = floating.detached.get();
            if (item == nullptr) {
                continue;
            }

            auto outline = dock.floatingWindow;
            outline.background = Core::Renderer::Color{0.f, 0.f, 0.f, 0.f};
            outline.shadow.enabled = false;
            const float layer = floatingLayer(i);
            context.painter.drawBox(makeBox(
                floating.bounds, outline, context.pickingId, layer + 0.010f));

            auto headerStyle = dock.floatingHeader;
            if (m_pressedItem == item->id && m_tabDrag.has_value()) {
                headerStyle.background = tabs.pressed.background;
                headerStyle.border = tabs.pressed.border;
            } else if (m_hoveredItem == item->id) {
                headerStyle.background = tabs.hovered.background;
                headerStyle.border = tabs.hovered.border;
            }
            const auto header = floatingHeaderBounds(floating, context.state);
            context.painter.drawBox(makeBox(
                header, headerStyle, context.pickingId, layer + 0.011f));
            context.painter.drawText(
                item->title,
                {
                    .bounds =
                        {
                            .center = header.center,
                            .size = {std::max(0.f,
                                              header.size.x -
                                                  tabs.horizontalPadding * 2.f),
                                     header.size.y},
                        },
                    .fontSize = tabs.text.fontSize,
                    .color = tabs.text.color,
                    .horizontal = HorizontalTextAlignment::start,
                    .vertical = VerticalTextAlignment::center,
                    .zIndex = layer + 0.012f,
                    .letterSpacing = tabs.text.letterSpacing,
                    .pickingId = context.pickingId,
                });
        }

        if (!m_dropGuide.has_value()) {
            return;
        }

        const float guideLayer = floatingLayer(m_floatingItems.size()) + 1.f;

        if (m_hoveredDropZone.has_value()) {
            if (const auto *hovered = m_dropGuide->region(*m_hoveredDropZone)) {
                context.painter.drawBox(makeBox(hovered->previewBounds,
                                                dock.dropPreview,
                                                context.pickingId,
                                                guideLayer));
            }
        }

        for (const auto &region : m_dropGuide->regions) {
            const bool hovered = m_hoveredDropZone == region.zone;
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

    UIEventReply DockSpace::onEvent(WidgetEventContext &context,
                                    const UIEvent &event) {
        if (context.phase == UIEventPhase::capture) {
            if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                button != nullptr && button->button == MouseButton::left &&
                button->action == MouseButtonAction::press &&
                context.hasPointerPosition) {
                const DockItemId floating =
                    floatingHeaderAt(context.pointerPosition, context.state);
                if (floating) {
                    return beginFloatingHeaderPress(context, floating);
                }
            }
            if (event.is<Input::MouseMoveEvent>() && !m_tabDrag.has_value() &&
                context.hasPointerPosition) {
                const DockItemId floating =
                    floatingHeaderAt(context.pointerPosition, context.state);
                if ((floating || isItemFloating(m_hoveredItem)) &&
                    m_hoveredItem != floating) {
                    m_hoveredItem = floating;
                    return {.invalidate = WidgetInvalidation::paint};
                }
            }
            return {};
        }
        if (context.phase != UIEventPhase::target) {
            return {};
        }

        const auto layout = calculateLayout(context.bounds, context.state);
        if (const auto *crossing = event.getIf<UIPointerCrossingEvent>();
            crossing != nullptr && !crossing->entered) {
            m_hoveredItem = {};
            m_hoveredSplit = {};
            if (m_tabDrag.has_value()) {
                m_dropGuide.reset();
                m_hoveredDropZone.reset();
                return {.handled = true,
                        .stopPropagation = true,
                        .invalidate = WidgetInvalidation::paint};
            }
            auto result = m_tabPressable.handle(context, event);
            result.reply.invalidate |= WidgetInvalidation::paint;
            return result.reply;
        }

        if (event.is<Input::MouseMoveEvent>()) {
            if (m_draggedSplit) {
                const auto *split = m_model.getSplit(m_draggedSplit);
                const auto *splitLayout = layout.findSplit(m_draggedSplit);
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
                    m_model.setSplitRatio(m_draggedSplit,
                                          (coordinate - thickness * 0.5f) /
                                              available);
                }
                return {.handled = true,
                        .stopPropagation = true,
                        .capturePointer = true,
                        .invalidate = WidgetInvalidation::layout |
                                      WidgetInvalidation::paint};
            }

            if (m_tabDrag.has_value()) {
                if (!m_tabDrag->started) {
                    const auto distance =
                        context.pointerPosition - m_tabDrag->pressPosition;
                    const float threshold =
                        std::max(0.f, dockStyle(context.state).dragThreshold);
                    if (distance.x * distance.x + distance.y * distance.y >=
                        threshold * threshold) {
                        if (m_tabDrag->floating) {
                            m_tabDrag->started = true;
                        } else {
                            static_cast<void>(beginTabDrag(context, layout));
                        }
                    }
                }
                if (m_tabDrag.has_value() && m_tabDrag->floating &&
                    m_tabDrag->started) {
                    updateFloatingDrag(context);
                }
                return {.handled = true,
                        .stopPropagation = true,
                        .capturePointer = true,
                        .invalidate = WidgetInvalidation::layout |
                                      WidgetInvalidation::paint};
            }

            m_hoveredSplit = layout.dividerAt(context.pointerPosition);
            m_hoveredItem =
                floatingHeaderAt(context.pointerPosition, context.state);
            if (!m_hoveredItem) {
                const auto hit = hitTab(
                    context.bounds, context.state, context.pointerPosition);
                m_hoveredItem = hit.item;
            }
            auto result = m_tabPressable.handle(context, event);
            result.reply.invalidate |= WidgetInvalidation::paint;
            return result.reply;
        }

        if (const auto *button = event.getIf<Input::MouseButtonEvent>();
            button != nullptr && button->button == MouseButton::left) {
            if (button->action == MouseButtonAction::release &&
                m_tabDrag.has_value()) {
                if (m_tabDrag->floating) {
                    if (m_tabDrag->started) {
                        if (context.hasPointerPosition) {
                            // The release coordinate is authoritative. It may
                            // arrive without a preceding move (for example
                            // after crossing the target boundary), so refresh
                            // both the floating bounds and drop hit here.
                            updateFloatingDrag(context);
                        } else {
                            m_dropGuide.reset();
                            m_hoveredDropZone.reset();
                        }
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
                auto result = m_tabPressable.handle(context, event);
                if (result.activated && hit.item == pressed) {
                    static_cast<void>(m_model.activateItem(pressed));
                }
                m_tabDrag.reset();
                m_pressedItem = {};
                result.reply.invalidate |= WidgetInvalidation::paint;
                return result.reply;
            }

            if (button->action == MouseButtonAction::press &&
                context.hasPointerPosition) {
                const DockItemId floating =
                    floatingHeaderAt(context.pointerPosition, context.state);
                if (floating) {
                    return beginFloatingHeaderPress(context, floating);
                }
            }

            const DockNodeId divider =
                context.hasPointerPosition
                    ? layout.dividerAt(context.pointerPosition)
                    : DockNodeId{};
            if ((button->action == MouseButtonAction::press ||
                 button->action == MouseButtonAction::doubleClick) &&
                divider) {
                if (button->action == MouseButtonAction::doubleClick) {
                    m_model.setSplitRatio(divider, 0.5f);
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
                m_draggedSplit) {
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
            if (button->action == MouseButtonAction::press) {
                m_pressedItem = hit.item;
                m_focusedStack = hit.stack;
                m_tabDrag = TabDrag{
                    .item = hit.item,
                    .pressPosition = context.pointerPosition,
                };
            } else if (button->action == MouseButtonAction::doubleClick) {
                m_pressedItem = hit.item;
                m_focusedStack = hit.stack;
            }
            auto result = m_tabPressable.handle(context, event);
            if (button->action == MouseButtonAction::doubleClick &&
                result.activated && m_pressedItem &&
                hit.item == m_pressedItem) {
                m_model.activateItem(m_pressedItem);
                m_pressedItem = {};
            }
            return result.reply;
        }

        if (const auto *key = event.getIf<Input::KeyEvent>();
            key != nullptr && context.focused &&
            key->action == KeyAction::press && m_focusedStack) {
            const auto *stack = m_model.getStack(m_focusedStack);
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
                    m_model.activateItem(next);
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

        const DockItemId inserted = m_model.addItem(
            {
                .id = itemId,
                .title = std::move(title),
                .content = panelId,
                .closable = closable,
            },
            target,
            zone);
        if (!inserted) {
            state.removeWidget(panelId);
            return {};
        }
        return {.item = inserted, .panel = panelId};
    }

    bool DockSpace::removePanel(WidgetTree &state, DockItemId item) {
        if (state.getWidget<DockSpace>(m_mountedId) != this) {
            return false;
        }
        if (m_tabDrag.has_value() && m_tabDrag->item == item) {
            clearTabInteraction();
            state.releasePointer(m_mountedId);
        }
        if (m_hoveredItem == item) {
            m_hoveredItem = {};
        }
        const auto *dockItem = m_model.getItem(item);
        if (dockItem != nullptr) {
            const WidgetId panel = dockItem->content;
            if (!m_model.removeItem(item)) {
                return false;
            }
            state.removeWidget(panel);
            return true;
        }

        const auto floating =
            std::find_if(m_floatingItems.begin(),
                         m_floatingItems.end(),
                         [item](const FloatingItem &entry) {
                             const auto *value = entry.detached.get();
                             return value != nullptr && value->id == item;
                         });
        if (floating == m_floatingItems.end()) {
            return false;
        }
        const WidgetId panel = floating->detached.get()->content;
        m_floatingItems.erase(floating);
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
        const auto *dockItem = m_model.getItem(item);
        auto *floating = findFloating(item);
        const WidgetId content =
            dockItem != nullptr ? dockItem->content
            : floating != nullptr && floating->detached.get() != nullptr
                ? floating->detached.get()->content
                : WidgetId{};
        auto *panel = state.getWidget<DockPanel>(content);
        if (panel == nullptr) {
            return false;
        }
        panel->setTitle(title);
        if (dockItem != nullptr) {
            return m_model.setItemTitle(item, std::move(title));
        }
        floating->detached.m_item->title = std::move(title);
        state.invalidate(m_mountedId, WidgetInvalidation::paint);
        return true;
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
        const WidgetId content = detached.get()->content;
        m_floatingItems.push_back(
            {.detached = std::move(detached), .bounds = bounds});
        if (m_mountedState != nullptr && m_mountedId && content) {
            static_cast<void>(m_mountedState->reparentWidget(
                content, m_mountedId, WidgetTree::append));
        }
        return true;
    }

    bool DockSpace::dockFloatingItem(DockItemId item,
                                     DockNodeId target,
                                     DockZone zone,
                                     size_t tabIndex) {
        const auto floating =
            std::find_if(m_floatingItems.begin(),
                         m_floatingItems.end(),
                         [item](const FloatingItem &entry) {
                             const auto *value = entry.detached.get();
                             return value != nullptr && value->id == item;
                         });
        if (floating == m_floatingItems.end()) {
            return false;
        }
        if (!target && !m_model.empty()) {
            target = m_model.firstStack();
        }
        if (!m_model.attachItem(
                std::move(floating->detached), target, zone, tabIndex)) {
            return false;
        }
        m_floatingItems.erase(floating);
        m_focusedStack = m_model.stackForItem(item);
        return true;
    }

    bool DockSpace::isItemFloating(DockItemId item) const noexcept {
        return findFloating(item) != nullptr;
    }

    size_t DockSpace::floatingItemCount() const noexcept {
        return m_floatingItems.size();
    }

    std::optional<WidgetBounds>
    DockSpace::floatingItemBounds(DockItemId item) const noexcept {
        const auto *floating = findFloating(item);
        return floating != nullptr ? std::optional{floating->bounds}
                                   : std::nullopt;
    }

    const UIDockStyle &DockSpace::dockStyle(const WidgetTree &state) const {
        return m_options.dockStyle ? *m_options.dockStyle : state.theme().dock;
    }

    const UITabStyle &DockSpace::tabStyle(const WidgetTree &state) const {
        return m_options.tabStyle ? *m_options.tabStyle : state.theme().tabs;
    }

    DockLayoutResult DockSpace::calculateLayout(WidgetBounds bounds,
                                                const WidgetTree &state) const {
        const auto &dock = dockStyle(state);
        const auto &tabs = tabStyle(state);
        return m_model.layout(bounds, tabs.height, dock.splitterThickness);
    }

    DockSpace::HitTab DockSpace::hitTab(WidgetBounds bounds,
                                        const WidgetTree &state,
                                        glm::vec2 position) const {
        const auto layout = calculateLayout(bounds, state);
        const auto &tabs = tabStyle(state);
        for (const auto &stackLayout : layout.stacks) {
            if (!stackLayout.tabBarBounds.contains(position)) {
                continue;
            }
            const auto *stack = m_model.getStack(stackLayout.node);
            if (stack == nullptr) {
                continue;
            }
            const auto regions = TabStripLayout::calculate(
                stackLayout.tabBarBounds, stack->tabs.size(), metrics(tabs));
            const auto index = TabStripLayout::hitTest(regions, position);
            const auto items = stack->tabs.items();
            if (index && *index < items.size() && items[*index].enabled) {
                return {.stack = stackLayout.node, .item = items[*index].id};
            }
        }
        return {};
    }

    DockSpace::FloatingItem *DockSpace::findFloating(DockItemId item) noexcept {
        const auto it =
            std::find_if(m_floatingItems.begin(),
                         m_floatingItems.end(),
                         [item](const FloatingItem &entry) {
                             const auto *value = entry.detached.get();
                             return value != nullptr && value->id == item;
                         });
        return it != m_floatingItems.end() ? &*it : nullptr;
    }

    const DockSpace::FloatingItem *
    DockSpace::findFloating(DockItemId item) const noexcept {
        const auto it =
            std::find_if(m_floatingItems.begin(),
                         m_floatingItems.end(),
                         [item](const FloatingItem &entry) {
                             const auto *value = entry.detached.get();
                             return value != nullptr && value->id == item;
                         });
        return it != m_floatingItems.end() ? &*it : nullptr;
    }

    DockItemId
    DockSpace::floatingHeaderAt(glm::vec2 position,
                                const WidgetTree &state) const noexcept {
        for (auto it = m_floatingItems.rbegin(); it != m_floatingItems.rend();
             ++it) {
            const auto *item = it->detached.get();
            if (item != nullptr &&
                floatingHeaderBounds(*it, state).contains(position)) {
                return item->id;
            }
        }
        return {};
    }

    WidgetBounds
    DockSpace::floatingHeaderBounds(const FloatingItem &item,
                                    const WidgetTree &state) const noexcept {
        const float height =
            std::min(std::max(0.f, tabStyle(state).height), item.bounds.size.y);
        return {
            .center = {item.bounds.center.x,
                       item.bounds.topLeft().y + height * 0.5f},
            .size = {item.bounds.size.x, height},
        };
    }

    WidgetBounds
    DockSpace::floatingContentBounds(const FloatingItem &item,
                                     const WidgetTree &state) const noexcept {
        const float border =
            std::max(0.f, dockStyle(state).floatingWindow.borderThickness.x);
        const auto header = floatingHeaderBounds(item, state);
        const float height =
            std::max(0.f, item.bounds.size.y - header.size.y - border * 2.f);
        return {
            .center = {item.bounds.center.x,
                       item.bounds.topLeft().y + header.size.y + border +
                           height * 0.5f},
            .size = {std::max(0.f, item.bounds.size.x - border * 2.f), height},
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
        const float margin = std::max(0.f, style.floatingMargin);
        const glm::vec2 effectiveMargin =
            glm::min(glm::vec2{margin},
                     glm::max((dockBounds.size - glm::vec2{1.f}) * 0.5f,
                              glm::vec2{0.f}));
        const glm::vec2 available =
            glm::max(dockBounds.size - effectiveMargin * 2.f, glm::vec2{1.f});
        const glm::vec2 minimum = glm::min(
            glm::max(style.floatingMinimumSize, glm::vec2{1.f}), available);
        const glm::vec2 maximum = glm::max(
            minimum,
            glm::min(glm::max(style.floatingMaximumSize, glm::vec2{1.f}),
                     available));
        requested.size = glm::clamp(
            glm::max(requested.size, glm::vec2{1.f}), minimum, maximum);

        const auto dockMin = dockBounds.topLeft() + effectiveMargin;
        const auto dockMax = dockBounds.bottomRight() - effectiveMargin;
        const auto half = requested.size * 0.5f;
        requested.center =
            glm::clamp(requested.center, dockMin + half, dockMax - half);
        return requested;
    }

    bool DockSpace::beginTabDrag(WidgetEventContext &context,
                                 const DockLayoutResult &layout) {
        if (!m_tabDrag.has_value() || m_tabDrag->floating) {
            return false;
        }
        const DockNodeId stack = m_model.stackForItem(m_tabDrag->item);
        const auto *source = layout.findStack(stack);
        if (source == nullptr) {
            return false;
        }

        const auto &style = dockStyle(context.state);
        WidgetBounds floatingBounds = source->bounds;
        floatingBounds.size = glm::clamp(
            floatingBounds.size,
            glm::min(style.floatingMinimumSize, context.bounds.size),
            glm::max(glm::min(style.floatingMinimumSize, context.bounds.size),
                     glm::min(style.floatingMaximumSize, context.bounds.size)));
        // Preserve the tab's top-left anchoring when the source leaf is larger
        // than a practical floating panel, avoiding a jump at drag start.
        floatingBounds.center =
            source->bounds.topLeft() + floatingBounds.size * 0.5f;
        floatingBounds = normalizedFloatingBounds(
            floatingBounds, context.bounds, context.state);
        if (!floatItem(m_tabDrag->item, floatingBounds)) {
            return false;
        }

        const auto *floating = findFloating(m_tabDrag->item);
        if (floating == nullptr) {
            return false;
        }
        m_tabDrag->grabOffset =
            m_tabDrag->pressPosition - floating->bounds.center;
        m_tabDrag->floating = true;
        m_tabDrag->started = true;
        m_tabPressable.reset();
        updateFloatingDrag(context);
        return true;
    }

    void DockSpace::updateFloatingDrag(WidgetEventContext &context) {
        if (!m_tabDrag.has_value() || !m_tabDrag->floating) {
            return;
        }
        auto *floating = findFloating(m_tabDrag->item);
        if (floating == nullptr) {
            clearTabInteraction();
            return;
        }
        floating->bounds.center =
            context.pointerPosition - m_tabDrag->grabOffset;
        floating->bounds = normalizedFloatingBounds(
            floating->bounds, context.bounds, context.state);
        refreshDropGuide(
            context.bounds, context.state, context.pointerPosition);
    }

    bool DockSpace::finishFloatingDrag() {
        if (!m_tabDrag.has_value() || !m_tabDrag->floating ||
            !m_dropGuide.has_value() || !m_hoveredDropZone.has_value()) {
            return false;
        }
        return dockFloatingItem(
            m_tabDrag->item, m_dropGuide->target, *m_hoveredDropZone);
    }

    void DockSpace::refreshDropGuide(WidgetBounds bounds,
                                     const WidgetTree &state,
                                     glm::vec2 position) {
        m_dropGuide.reset();
        m_hoveredDropZone.reset();
        if (!bounds.contains(position)) {
            return;
        }

        WidgetBounds targetBounds = bounds;
        DockNodeId target;
        if (!m_model.empty()) {
            const auto layout = calculateLayout(bounds, state);
            target = layout.stackAt(position);
            const auto *stack = layout.findStack(target);
            if (stack == nullptr) {
                return;
            }
            targetBounds = stack->bounds;
        }

        m_dropGuide = DockDropGuideLayoutSolver::calculate(
            targetBounds, target, dropGuideMetrics(state));
        if (const auto *region = m_dropGuide->regionAt(position)) {
            m_hoveredDropZone = region->zone;
        }
    }

    void DockSpace::clearTabInteraction() noexcept {
        m_tabDrag.reset();
        m_dropGuide.reset();
        m_hoveredDropZone.reset();
        m_pressedItem = {};
        m_tabPressable.reset();
    }

    void DockSpace::bringFloatingToFront(WidgetEventContext &context,
                                         DockItemId item) {
        const auto it =
            std::find_if(m_floatingItems.begin(),
                         m_floatingItems.end(),
                         [item](const FloatingItem &entry) {
                             const auto *value = entry.detached.get();
                             return value != nullptr && value->id == item;
                         });
        if (it == m_floatingItems.end()) {
            return;
        }
        const WidgetId content = it->detached.get()->content;
        if (std::next(it) != m_floatingItems.end()) {
            FloatingItem moved = std::move(*it);
            m_floatingItems.erase(it);
            m_floatingItems.push_back(std::move(moved));
        }
        if (content) {
            static_cast<void>(context.state.reparentWidget(
                content, context.id, WidgetTree::append));
        }
    }

    UIEventReply
    DockSpace::beginFloatingHeaderPress(WidgetEventContext &context,
                                        DockItemId item) {
        bringFloatingToFront(context, item);
        const auto *entry = findFloating(item);
        if (entry == nullptr) {
            return {};
        }
        m_tabPressable.reset();
        m_pressedItem = item;
        m_tabDrag = TabDrag{
            .item = item,
            .pressPosition = context.pointerPosition,
            .grabOffset = context.pointerPosition - entry->bounds.center,
            .floating = true,
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
} // namespace Bess::UI
