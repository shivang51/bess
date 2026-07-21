#include "controls/dock_space.h"

#include "controls/tab_bar.h"
#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
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
            };
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
        context.layout.setWidthStretch();
        context.layout.setHeightStretch();
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
        context.layout.setWidthStretch();
        context.layout.setHeightStretch();
        m_modelConnection = m_model.changed().connect(
            [state = &context.state, id = context.id](const DockModelChange &) {
                state->invalidate(
                    id, WidgetInvalidation::layout | WidgetInvalidation::paint);
            });
    }

    void DockSpace::onUnmount(WidgetTree &, WidgetId) {
        m_modelConnection.disconnect();
    }

    void DockSpace::arrange(WidgetArrangeContext &context) {
        for (const auto child : context.children()) {
            if (m_model.itemForContent(child)) {
                context.setChildVisible(child, false);
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
                } else if (m_hoveredItem == item.id) {
                    style = &tabs.hovered;
                } else if (stack->tabs.active() == item.id) {
                    style = &tabs.active;
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
    }

    UIEventReply DockSpace::onEvent(WidgetEventContext &context,
                                    const UIEvent &event) {
        if (context.phase != UIEventPhase::target) {
            return {};
        }

        const auto layout = calculateLayout(context.bounds, context.state);
        if (const auto *crossing = event.getIf<UIPointerCrossingEvent>();
            crossing != nullptr && !crossing->entered) {
            m_hoveredItem = {};
            m_hoveredSplit = {};
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

            m_hoveredSplit = layout.dividerAt(context.pointerPosition);
            const auto hit =
                hitTab(context.bounds, context.state, context.pointerPosition);
            m_hoveredItem = hit.item;
            auto result = m_tabPressable.handle(context, event);
            result.reply.invalidate |= WidgetInvalidation::paint;
            return result.reply;
        }

        if (const auto *button = event.getIf<Input::MouseButtonEvent>();
            button != nullptr && button->button == MouseButton::left) {
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
            if (button->action == MouseButtonAction::press ||
                button->action == MouseButtonAction::doubleClick) {
                m_pressedItem = hit.item;
                m_focusedStack = hit.stack;
            }
            auto result = m_tabPressable.handle(context, event);
            if (result.activated && m_pressedItem &&
                hit.item == m_pressedItem) {
                m_model.activateItem(m_pressedItem);
            }
            if (button->action == MouseButtonAction::release) {
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
        const auto *dockItem = m_model.getItem(item);
        if (dockItem == nullptr) {
            return false;
        }
        const WidgetId panel = dockItem->content;
        if (!m_model.removeItem(item)) {
            return false;
        }
        state.removeWidget(panel);
        return true;
    }

    bool DockSpace::setPanelTitle(WidgetTree &state,
                                  DockItemId item,
                                  std::string title) {
        const auto *dockItem = m_model.getItem(item);
        if (dockItem == nullptr) {
            return false;
        }
        auto *panel = state.getWidget<DockPanel>(dockItem->content);
        if (panel == nullptr) {
            return false;
        }
        panel->setTitle(title);
        return m_model.setItemTitle(item, std::move(title));
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
} // namespace Bess::UI
