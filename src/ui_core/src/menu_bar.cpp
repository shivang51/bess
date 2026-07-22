#include "controls/menu_bar.h"

#include "bess_core/ui/icons/font_awesome_icons.h"
#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Bess::UI {
    namespace {
        float textWidth(std::string_view text, const UITextStyle &style) {
            return std::max(
                0.f,
                static_cast<float>(text.size()) * style.fontSize * 0.6f +
                    style.letterSpacing * static_cast<float>(text.size()));
        }

        float preferredBarWidth(const MenuModel &model,
                                const UIMenuStyle &style) {
            float width = 0.f;
            for (const auto &menu : model.menus()) {
                width += textWidth(menu.name, style.barText) +
                         std::max(0.f, style.barHorizontalPadding) * 2.f;
            }
            return width;
        }

        float menuBarControlHeight(const UIMenuStyle &style) {
            return std::max(1.f, style.barHeight) +
                   std::max(0.f, style.barVerticalMargin) * 2.f;
        }

        WidgetBounds menuBarContentBounds(WidgetBounds controlBounds,
                                          const UIMenuStyle &style) {
            const float margin =
                std::min(std::max(0.f, style.barVerticalMargin),
                         std::max(0.f, controlBounds.size.y) * 0.5f);
            controlBounds.size.y =
                std::max(0.f, controlBounds.size.y - margin * 2.f);
            return controlBounds;
        }

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

        WidgetBounds fromTopLeft(glm::vec2 topLeft, glm::vec2 size) {
            return {.center = topLeft + size * 0.5f, .size = size};
        }

        glm::vec2 constrainedPopupTopLeft(glm::vec2 desired,
                                          glm::vec2 size,
                                          WidgetBounds viewport) {
            constexpr float margin = 4.f;
            const glm::vec2 minimum = viewport.topLeft() + glm::vec2{margin};
            const glm::vec2 maximum =
                viewport.bottomRight() - glm::vec2{margin} - size;
            if (maximum.x >= minimum.x) {
                desired.x = std::clamp(desired.x, minimum.x, maximum.x);
            } else {
                desired.x = minimum.x;
            }
            if (maximum.y >= minimum.y) {
                desired.y = std::clamp(desired.y, minimum.y, maximum.y);
            } else {
                desired.y = minimum.y;
            }
            return desired;
        }

        const MenuItem *findDirect(const std::vector<MenuItem> &items,
                                   MenuItemId id) {
            const auto it = std::find_if(
                items.begin(), items.end(), [id](const MenuItem &item) {
                    return item.id == id;
                });
            return it != items.end() ? &*it : nullptr;
        }

        const MenuHeadingLayout *findHeading(const MenuBarLayout &layout,
                                             MenuId id) {
            const auto it =
                std::find_if(layout.headings.begin(),
                             layout.headings.end(),
                             [id](const MenuHeadingLayout &heading) {
                                 return heading.menu == id;
                             });
            return it != layout.headings.end() ? &*it : nullptr;
        }

        struct MenuPopupMetrics {
            float width = 0.f;
            float contentHeight = 0.f;
            float shortcutWidth = 0.f;
            bool hasShortcut = false;
            bool hasSubmenu = false;
        };

        MenuPopupMetrics measureMenuPopup(std::span<const MenuItem> items,
                                          const UIMenuStyle &style) {
            MenuPopupMetrics result;
            float nameWidth = 0.f;
            result.contentHeight = std::max(0.f, style.popupPadding) * 2.f;
            for (const auto &item : items) {
                if (item.kind == MenuItemKind::separator) {
                    result.contentHeight +=
                        std::max(1.f, style.separatorHeight);
                    continue;
                }
                nameWidth =
                    std::max(nameWidth, textWidth(item.name, style.text));
                result.shortcutWidth = std::max(
                    result.shortcutWidth, textWidth(item.shortcut, style.text));
                result.hasShortcut =
                    result.hasShortcut || !item.shortcut.empty();
                result.hasSubmenu = result.hasSubmenu || item.isSubmenu();
                result.contentHeight += std::max(1.f, style.itemHeight);
            }

            result.width = std::max(0.f, style.itemHorizontalPadding) * 2.f +
                           std::max(0.f, style.iconColumnWidth) + nameWidth;
            if (result.hasShortcut) {
                result.width +=
                    std::max(0.f, style.shortcutGap) + result.shortcutWidth;
            }
            if (result.hasSubmenu) {
                result.width += std::max(0.f, style.submenuIndicatorWidth);
            }
            const float minimum = std::max(1.f, style.popupMinimumWidth);
            const float maximum = std::max(minimum, style.popupMaximumWidth);
            result.width = std::clamp(result.width, minimum, maximum);
            return result;
        }
    } // namespace

    const MenuPopupItemLayout *
    MenuPopupLayout::itemAt(glm::vec2 position) const noexcept {
        const auto it = std::find_if(
            items.begin(), items.end(), [position](const auto &item) {
                return item.bounds.contains(position);
            });
        return it != items.end() ? &*it : nullptr;
    }

    glm::vec2
    MenuPopupLayoutSolver::preferredSize(std::span<const MenuItem> items,
                                         const UIMenuStyle &style) {
        const auto metrics = measureMenuPopup(items, style);
        return {metrics.width, metrics.contentHeight};
    }

    MenuPopupLayout
    MenuPopupLayoutSolver::calculate(WidgetBounds bounds,
                                     std::span<const MenuItem> items,
                                     const UIMenuStyle &style,
                                     size_t depth,
                                     float scrollOffset) {
        const auto metrics = measureMenuPopup(items, style);
        bounds.size = glm::max(bounds.size, glm::vec2{0.f});
        const float maximumScroll =
            std::max(0.f, metrics.contentHeight - bounds.size.y);
        scrollOffset =
            std::clamp(std::isfinite(scrollOffset) ? scrollOffset : 0.f,
                       0.f,
                       maximumScroll);

        MenuPopupLayout result{
            .depth = depth,
            .bounds = bounds,
            .contentHeight = metrics.contentHeight,
        };
        result.items.reserve(items.size());

        const float popupPadding = std::max(0.f, style.popupPadding);
        const float horizontalPadding =
            std::max(0.f, style.itemHorizontalPadding);
        const float contentLeft = bounds.topLeft().x + popupPadding;
        const float contentWidth =
            std::max(0.f, bounds.size.x - popupPadding * 2.f);
        float top = bounds.topLeft().y + popupPadding - scrollOffset;
        for (const auto &item : items) {
            const float rowHeight = item.kind == MenuItemKind::separator
                                        ? std::max(1.f, style.separatorHeight)
                                        : std::max(1.f, style.itemHeight);
            MenuPopupItemLayout row{
                .item = item.id,
                .bounds =
                    fromTopLeft({contentLeft, top}, {contentWidth, rowHeight}),
                .separator = item.kind == MenuItemKind::separator,
            };
            if (!row.separator) {
                float left = contentLeft + horizontalPadding;
                const float right =
                    contentLeft + contentWidth - horizontalPadding;
                const float iconWidth =
                    std::min(std::max(0.f, style.iconColumnWidth),
                             std::max(0.f, right - left));
                row.iconBounds =
                    fromTopLeft({left, top}, {iconWidth, rowHeight});
                left += iconWidth;

                // These are panel columns, not per-item columns. Even an item
                // without a shortcut/child receives the same reserved boxes,
                // so every visible trailing affordance shares an exact edge.
                const float indicatorWidth =
                    metrics.hasSubmenu
                        ? std::min(std::max(0.f, style.submenuIndicatorWidth),
                                   std::max(0.f, right - left))
                        : 0.f;
                row.submenuIndicatorBounds = fromTopLeft(
                    {right - indicatorWidth, top}, {indicatorWidth, rowHeight});
                float textRight = right - indicatorWidth;
                const float shortcutWidth =
                    metrics.hasShortcut
                        ? std::min(metrics.shortcutWidth,
                                   std::max(0.f, textRight - left))
                        : 0.f;
                row.shortcutBounds =
                    fromTopLeft({textRight - shortcutWidth, top},
                                {shortcutWidth, rowHeight});
                if (shortcutWidth > 0.f) {
                    textRight = std::max(left,
                                         textRight - shortcutWidth -
                                             std::max(0.f, style.shortcutGap));
                }
                row.labelBounds = fromTopLeft(
                    {left, top}, {std::max(0.f, textRight - left), rowHeight});
            }
            result.items.push_back(row);
            top += rowHeight;
        }
        return result;
    }

    void MenuPopupPresenter::paint(UIPainter &painter,
                                   const MenuPopupLayout &layout,
                                   const MenuModel &model,
                                   const UIMenuStyle &style,
                                   PickingId pickingId,
                                   MenuPopupPaintOptions options) {
        painter.drawBox(
            makeBox(layout.bounds, style.popup, pickingId, options.panelDepth));
        const ScopedUIClip popupClip{painter, layout.bounds};
        for (const auto &row : layout.items) {
            if (row.bounds.bottomRight().y < layout.bounds.topLeft().y ||
                row.bounds.topLeft().y > layout.bounds.bottomRight().y) {
                continue;
            }
            const auto *item = model.findItem(row.item);
            if (item == nullptr) {
                continue;
            }
            if (row.separator) {
                painter.drawBox({
                    .bounds =
                        {
                            .center = row.bounds.center,
                            .size = {std::max(
                                         0.f,
                                         row.bounds.size.x -
                                             std::max(
                                                 0.f,
                                                 style.itemHorizontalPadding) *
                                                 2.f),
                                     1.f},
                        },
                    .color = style.separator,
                    .zIndex = options.contentDepth,
                    .pickingId = pickingId,
                });
                continue;
            }

            const bool activeBranch =
                std::find(options.activePath.begin(),
                          options.activePath.end(),
                          item->id) != options.activePath.end();
            if (options.hotItem == item->id ||
                options.pressedItem == item->id || activeBranch) {
                const auto &box = options.pressedItem == item->id
                                      ? style.itemPressed
                                      : style.itemHovered;
                painter.drawBox(makeBox(
                    row.bounds, box, pickingId, options.rowChromeDepth));
            }

            const auto textColor =
                item->enabled ? style.text.color : style.disabledText;
            const std::string_view icon =
                !item->icon.empty()
                    ? std::string_view{item->icon}
                    : (item->checked
                           ? std::string_view{Icons::FontAwesomeIcons::FA_CHECK}
                           : std::string_view{});
            if (!icon.empty()) {
                painter.drawIcon(
                    icon,
                    {.glyph = {.bounds = row.iconBounds,
                               .fontSize = style.text.fontSize,
                               .color = item->enabled ? style.iconColor
                                                      : style.disabledText,
                               .horizontal = HorizontalTextAlignment::center,
                               .vertical = VerticalTextAlignment::center,
                               .zIndex = options.contentDepth,
                               .letterSpacing = style.text.letterSpacing,
                               .pickingId = pickingId}});
            }
            painter.drawText(item->name,
                             {.bounds = row.labelBounds,
                              .fontSize = style.text.fontSize,
                              .color = textColor,
                              .horizontal = HorizontalTextAlignment::start,
                              .vertical = VerticalTextAlignment::center,
                              .zIndex = options.contentDepth,
                              .letterSpacing = style.text.letterSpacing,
                              .pickingId = pickingId});
            if (!item->shortcut.empty()) {
                painter.drawText(item->shortcut,
                                 {.bounds = row.shortcutBounds,
                                  .fontSize = style.text.fontSize,
                                  .color = item->enabled ? style.shortcutColor
                                                         : style.disabledText,
                                  .horizontal = HorizontalTextAlignment::end,
                                  .vertical = VerticalTextAlignment::center,
                                  .zIndex = options.contentDepth,
                                  .letterSpacing = style.text.letterSpacing,
                                  .pickingId = pickingId});
            }
            if (item->isSubmenu()) {
                painter.drawIcon(
                    Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT,
                    {.glyph = {.bounds = row.submenuIndicatorBounds,
                               .fontSize =
                                   std::max(1.f, style.submenuChevronSize),
                               .color = item->enabled ? style.iconColor
                                                      : style.disabledText,
                               .horizontal = HorizontalTextAlignment::center,
                               .vertical = VerticalTextAlignment::center,
                               .zIndex = options.contentDepth,
                               .pickingId = pickingId}});
            }
        }
    }

    const MenuHeadingLayout *
    MenuBarLayout::headingAt(glm::vec2 position) const noexcept {
        const auto it = std::find_if(
            headings.begin(), headings.end(), [position](const auto &heading) {
                return heading.bounds.contains(position);
            });
        return it != headings.end() ? &*it : nullptr;
    }

    const MenuPopupItemLayout *
    MenuBarLayout::itemAt(glm::vec2 position, size_t *depth) const noexcept {
        for (auto it = popups.rbegin(); it != popups.rend(); ++it) {
            if (const auto *item = it->itemAt(position)) {
                if (depth != nullptr) {
                    *depth = it->depth;
                }
                return item;
            }
        }
        return nullptr;
    }

    bool MenuBarLayout::contains(glm::vec2 position) const noexcept {
        if (barBounds.contains(position)) {
            return true;
        }
        return std::any_of(
            popups.begin(), popups.end(), [position](const auto &popup) {
                return popup.bounds.contains(position);
            });
    }

    MenuBarLayout
    MenuBarLayoutSolver::calculate(WidgetBounds controlBounds,
                                   WidgetBounds viewportBounds,
                                   const MenuModel &model,
                                   MenuId activeMenu,
                                   std::span<const MenuItemId> openSubmenus,
                                   const UIMenuStyle &style) {
        const WidgetBounds barBounds =
            menuBarContentBounds(controlBounds, style);
        MenuBarLayout result{.barBounds = barBounds};
        float headingLeft = barBounds.topLeft().x;
        for (const auto &menu : model.menus()) {
            const float width = textWidth(menu.name, style.barText) +
                                std::max(0.f, style.barHorizontalPadding) * 2.f;
            const WidgetBounds bounds =
                fromTopLeft({headingLeft, barBounds.topLeft().y},
                            {width, barBounds.size.y});
            result.headings.push_back({.menu = menu.id, .bounds = bounds});
            headingLeft += width;
        }

        const auto *active = model.findMenu(activeMenu);
        const auto *heading = findHeading(result, activeMenu);
        if (active == nullptr || heading == nullptr || !active->enabled) {
            return result;
        }

        const std::vector<MenuItem> *items = &active->items;
        glm::vec2 desired = {heading->bounds.topLeft().x,
                             barBounds.bottomRight().y -
                                 std::max(0.f, style.popupOverlap)};
        const MenuPopupItemLayout *parentItemLayout = nullptr;
        for (size_t depth = 0; items != nullptr; ++depth) {
            const glm::vec2 size =
                MenuPopupLayoutSolver::preferredSize(*items, style);

            if (parentItemLayout != nullptr) {
                const float overlap = std::max(0.f, style.popupOverlap);
                const float right =
                    result.popups.back().bounds.bottomRight().x - overlap;
                const float left =
                    result.popups.back().bounds.topLeft().x - size.x + overlap;
                desired.x = right + size.x <= viewportBounds.bottomRight().x
                                ? right
                                : left;
                desired.y = parentItemLayout->bounds.topLeft().y -
                            std::max(0.f, style.popupPadding);
            }
            desired = constrainedPopupTopLeft(desired, size, viewportBounds);
            result.popups.push_back(MenuPopupLayoutSolver::calculate(
                fromTopLeft(desired, size), *items, style, depth));

            if (depth >= openSubmenus.size()) {
                break;
            }
            const auto *open = findDirect(*items, openSubmenus[depth]);
            if (open == nullptr || !open->enabled || !open->isSubmenu()) {
                break;
            }
            const auto &currentPopup = result.popups.back();
            const auto row =
                std::find_if(currentPopup.items.begin(),
                             currentPopup.items.end(),
                             [open](const MenuPopupItemLayout &item) {
                                 return item.item == open->id;
                             });
            if (row == currentPopup.items.end()) {
                break;
            }
            parentItemLayout = &*row;
            items = &open->children;
        }
        return result;
    }

    MenuBar::MenuBar(std::shared_ptr<MenuModel> model, MenuBarOptions options)
        : m_model(model ? std::move(model) : std::make_shared<MenuModel>()),
          m_options(std::move(options)) {
    }

    std::string_view MenuBar::typeName() const noexcept {
        return "MenuBar";
    }

    WidgetTraits MenuBar::traits() const noexcept {
        return {
            .focusable = true, .hitTestVisible = true, .clipChildren = false};
    }

    void MenuBar::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        const auto &menuStyle = style(context.state);
        if (m_options.stretchWidth) {
            context.layout.setWidthPercent(1.f);
        } else {
            context.layout.setWidth(preferredBarWidth(*m_model, menuStyle));
        }
        context.layout.setHeight(menuBarControlHeight(menuStyle));
        context.layout.setFlexShrink(0.f);
        context.layout.setZVal(10.f);
        reconnectModel();
    }

    void MenuBar::onUnmount(WidgetTree &, WidgetId) {
        m_modelConnection.disconnect();
        m_state = nullptr;
        m_id = {};
        m_layout = {};
    }

    void MenuBar::updateLayout(WidgetLayoutContext &context) {
        const auto &menuStyle = style(context.state);
        if (m_options.stretchWidth) {
            context.layout.setWidthPercent(1.f);
        } else {
            context.layout.setWidth(preferredBarWidth(*m_model, menuStyle));
        }
        context.layout.setHeight(menuBarControlHeight(menuStyle));
    }

    void MenuBar::arrange(WidgetArrangeContext &context) {
        rebuildLayout(context.bounds, context.state);
    }

    void MenuBar::paint(WidgetPaintContext &context) const {
        rebuildLayout(context.bounds, context.state);
        const auto &menuStyle = style(context.state);
        if (m_options.drawBackground) {
            context.painter.drawBox(
                makeBox(m_layout.barBounds, menuStyle.bar, context.pickingId));
        }
        for (const auto &heading : m_layout.headings) {
            const auto *menu = m_model->findMenu(heading.menu);
            if (menu == nullptr) {
                continue;
            }
            const UIBoxStyle *box = &menuStyle.barItem;
            if (heading.menu == m_activeMenu) {
                box = &menuStyle.barItemActive;
            } else if (heading.menu == m_hotHeading) {
                box = &menuStyle.barItemHovered;
            }
            context.painter.drawBox(
                makeBox(heading.bounds, *box, context.pickingId, 0.001f));
            context.painter.drawText(
                menu->name,
                {.bounds = heading.bounds,
                 .fontSize = menuStyle.barText.fontSize,
                 .color = menu->enabled ? menuStyle.barText.color
                                        : menuStyle.disabledText,
                 .horizontal = HorizontalTextAlignment::center,
                 .vertical = VerticalTextAlignment::center,
                 .zIndex = 0.002f,
                 .letterSpacing = menuStyle.barText.letterSpacing,
                 .pickingId = context.pickingId});
        }
    }

    void MenuBar::paintOverlay(WidgetPaintContext &context) const {
        rebuildLayout(context.bounds, context.state);
        const auto &menuStyle = style(context.state);
        float layer = 0.10f;
        for (const auto &popup : m_layout.popups) {
            MenuPopupPresenter::paint(context.painter,
                                      popup,
                                      *m_model,
                                      menuStyle,
                                      context.pickingId,
                                      {.hotItem = m_hotItem,
                                       .pressedItem = m_pressedItem,
                                       .activePath = m_openSubmenus,
                                       .panelDepth = layer,
                                       .rowChromeDepth = layer + 0.001f,
                                       .contentDepth = layer + 0.002f});
            layer += 0.01f;
        }
    }

    bool MenuBar::hitTest(WidgetBounds bounds,
                          glm::vec2 position) const noexcept {
        // An open menu is a lightweight modal popup layer: the first outside
        // click dismisses it and is intentionally not delivered through to
        // the obscured control.
        return m_activeMenu || bounds.contains(position) ||
               m_layout.contains(position);
    }

    UIEventReply MenuBar::onEvent(WidgetEventContext &context,
                                  const UIEvent &event) {
        if (context.phase != UIEventPhase::target) {
            return {};
        }
        rebuildLayout(context.bounds, context.state);

        if (const auto *focus = event.getIf<UIFocusChangedEvent>()) {
            if (!focus->focused) {
                close();
                rebuildLayout(context.bounds, context.state);
                return {.invalidate = WidgetInvalidation::paint};
            }
            return {};
        }
        if (const auto *crossing = event.getIf<UIPointerCrossingEvent>()) {
            if (!crossing->entered) {
                m_hotHeading = {};
                m_hotItem = {};
                return {.invalidate = WidgetInvalidation::paint};
            }
            return {};
        }

        if (event.is<Input::MouseMoveEvent>() && context.hasPointerPosition) {
            if (const auto *heading =
                    m_layout.headingAt(context.pointerPosition)) {
                m_hotHeading = heading->menu;
                m_hotItem = {};
                const auto *menu = m_model->findMenu(heading->menu);
                if (m_activeMenu && heading->menu != m_activeMenu &&
                    menu != nullptr && menu->enabled) {
                    openMenu(heading->menu);
                    rebuildLayout(context.bounds, context.state);
                }
            } else {
                m_hotHeading = {};
                size_t depth = 0;
                const auto *row =
                    m_layout.itemAt(context.pointerPosition, &depth);
                const auto *item =
                    row != nullptr ? m_model->findItem(row->item) : nullptr;
                if (item != nullptr && item->kind != MenuItemKind::separator) {
                    m_hotItem = item->id;
                    m_hotDepth = depth;
                    if (item->enabled && item->isSubmenu()) {
                        openSubmenu(item->id, depth, false);
                    } else {
                        closeFromDepth(depth);
                    }
                    rebuildLayout(context.bounds, context.state);
                } else {
                    m_hotItem = {};
                }
            }
            return {.handled = true, .invalidate = WidgetInvalidation::paint};
        }

        if (const auto *button = event.getIf<Input::MouseButtonEvent>();
            button != nullptr && button->button == MouseButton::left &&
            context.hasPointerPosition) {
            const auto *heading = m_layout.headingAt(context.pointerPosition);
            size_t depth = 0;
            const auto *row = m_layout.itemAt(context.pointerPosition, &depth);
            const auto *item =
                row != nullptr ? m_model->findItem(row->item) : nullptr;
            if (button->action == MouseButtonAction::press) {
                if (heading != nullptr) {
                    const auto *menu = m_model->findMenu(heading->menu);
                    if (menu != nullptr && menu->enabled) {
                        if (m_activeMenu == heading->menu) {
                            close();
                        } else {
                            openMenu(heading->menu);
                        }
                    }
                    rebuildLayout(context.bounds, context.state);
                    return {.handled = true,
                            .stopPropagation = true,
                            .requestFocus = true,
                            .capturePointer = true,
                            .invalidate = WidgetInvalidation::paint};
                }
                if (item != nullptr) {
                    if (item->enabled &&
                        item->kind != MenuItemKind::separator) {
                        m_hotItem = item->id;
                        m_hotDepth = depth;
                        m_pressedItem = item->id;
                        if (item->isSubmenu()) {
                            openSubmenu(item->id, depth, false);
                        }
                    }
                    rebuildLayout(context.bounds, context.state);
                    return {.handled = true,
                            .stopPropagation = true,
                            .requestFocus = true,
                            .capturePointer = true,
                            .invalidate = WidgetInvalidation::paint};
                }
                close();
                rebuildLayout(context.bounds, context.state);
                return {.clearFocus = true,
                        .invalidate = WidgetInvalidation::paint};
            }
            if (button->action == MouseButtonAction::release) {
                const MenuItemId pressed = m_pressedItem;
                m_pressedItem = {};
                const bool activate = pressed && item != nullptr &&
                                      item->id == pressed && item->enabled &&
                                      !item->isSubmenu() &&
                                      item->kind == MenuItemKind::command;
                if (activate) {
                    close();
                    rebuildLayout(context.bounds, context.state);
                    static_cast<void>(m_model->activate(pressed));
                }
                return {.handled = static_cast<bool>(pressed),
                        .stopPropagation = static_cast<bool>(pressed),
                        .releasePointer = true,
                        .invalidate = WidgetInvalidation::paint};
            }
        }

        if (const auto *key = event.getIf<Input::KeyEvent>();
            key != nullptr && context.focused &&
            (key->action == KeyAction::press ||
             key->action == KeyAction::hold)) {
            bool handled = false;
            if (!m_activeMenu) {
                if (key->key == KeyCode::arrowDown ||
                    key->key == KeyCode::enter || key->key == KeyCode::space ||
                    key->key == KeyCode::arrowRight ||
                    key->key == KeyCode::arrowLeft) {
                    const int direction =
                        key->key == KeyCode::arrowLeft ? -1 : 1;
                    handled = moveHeading(direction);
                    if (handled) {
                        static_cast<void>(moveSelection(1));
                    }
                }
            } else if (key->key == KeyCode::arrowDown) {
                handled = moveSelection(1);
            } else if (key->key == KeyCode::arrowUp) {
                handled = moveSelection(-1);
            } else if (key->key == KeyCode::arrowRight) {
                const auto *selected = selectedItem();
                if (selected != nullptr && selected->enabled &&
                    selected->isSubmenu()) {
                    openSubmenu(selected->id, selectedDepth(), true);
                    handled = true;
                } else {
                    handled = moveHeading(1);
                    if (handled) {
                        static_cast<void>(moveSelection(1));
                    }
                }
            } else if (key->key == KeyCode::arrowLeft) {
                if (m_hotDepth > 0 && !m_openSubmenus.empty()) {
                    m_hotItem = m_openSubmenus[m_hotDepth - 1];
                    --m_hotDepth;
                    m_openSubmenus.resize(m_hotDepth);
                    handled = true;
                } else {
                    handled = moveHeading(-1);
                    if (handled) {
                        static_cast<void>(moveSelection(1));
                    }
                }
            } else if (key->key == KeyCode::enter ||
                       key->key == KeyCode::space) {
                handled = activateSelection();
            } else if (key->key == KeyCode::escape) {
                if (!m_openSubmenus.empty()) {
                    m_hotItem = m_openSubmenus.back();
                    m_hotDepth = m_openSubmenus.size() - 1;
                    m_openSubmenus.pop_back();
                } else {
                    close();
                }
                handled = true;
            }
            if (handled) {
                rebuildLayout(context.bounds, context.state);
                return {.handled = true,
                        .stopPropagation = true,
                        .invalidate = WidgetInvalidation::paint};
            }
        }
        return {};
    }

    std::shared_ptr<MenuModel> MenuBar::model() const noexcept {
        return m_model;
    }

    void MenuBar::setModel(std::shared_ptr<MenuModel> model) {
        m_model = model ? std::move(model) : std::make_shared<MenuModel>();
        close();
        reconnectModel();
        if (m_state != nullptr && m_id) {
            m_state->invalidate(
                m_id, WidgetInvalidation::layout | WidgetInvalidation::paint);
        }
    }

    bool MenuBar::isOpen() const noexcept {
        return static_cast<bool>(m_activeMenu);
    }

    MenuId MenuBar::activeMenu() const noexcept {
        return m_activeMenu;
    }

    void MenuBar::close() {
        const bool wasOpen = static_cast<bool>(m_activeMenu);
        m_activeMenu = {};
        m_openSubmenus.clear();
        m_hotItem = {};
        m_hotHeading = {};
        m_pressedItem = {};
        m_hotDepth = 0;
        m_layout.popups.clear();
        if (wasOpen && m_state != nullptr && m_id) {
            m_state->invalidate(m_id, WidgetInvalidation::paint);
        }
    }

    const UIMenuStyle &MenuBar::style(const WidgetTree &state) const {
        return m_options.style ? *m_options.style : state.theme().menus;
    }

    void MenuBar::rebuildLayout(WidgetBounds bounds,
                                const WidgetTree &state) const {
        const glm::vec2 viewportSize = state.getViewportSize();
        m_layout = MenuBarLayoutSolver::calculate(
            bounds,
            {.center = {0.f, 0.f}, .size = viewportSize},
            *m_model,
            m_activeMenu,
            m_openSubmenus,
            style(state));
    }

    void MenuBar::reconnectModel() {
        m_modelConnection.disconnect();
        if (!m_model) {
            return;
        }
        m_modelConnection =
            m_model->changed().connect([this](const MenuModelChange &) {
                normalizeOpenPath();
                if (m_state != nullptr && m_id) {
                    rebuildLayout(m_state->getBounds(m_id), *m_state);
                    m_state->invalidate(m_id,
                                        WidgetInvalidation::layout |
                                            WidgetInvalidation::paint);
                }
            });
    }

    void MenuBar::normalizeOpenPath() {
        const auto *menu = m_model->findMenu(m_activeMenu);
        if (menu == nullptr || !menu->enabled) {
            close();
            return;
        }
        const std::vector<MenuItem> *items = &menu->items;
        size_t valid = 0;
        for (const auto id : m_openSubmenus) {
            const auto *item = findDirect(*items, id);
            if (item == nullptr || !item->enabled || !item->isSubmenu()) {
                break;
            }
            ++valid;
            items = &item->children;
        }
        m_openSubmenus.resize(valid);
        if (m_hotItem && m_model->findItem(m_hotItem) == nullptr) {
            m_hotItem = {};
            m_hotDepth = 0;
        }
    }

    void MenuBar::openMenu(MenuId menu) {
        const auto *entry = m_model->findMenu(menu);
        if (entry == nullptr || !entry->enabled) {
            return;
        }
        m_activeMenu = menu;
        m_openSubmenus.clear();
        m_hotItem = {};
        m_hotHeading = menu;
        m_pressedItem = {};
        m_hotDepth = 0;
    }

    void MenuBar::openSubmenu(MenuItemId item, size_t depth, bool selectChild) {
        const auto *entry = m_model->findItem(item);
        if (entry == nullptr || !entry->enabled || !entry->isSubmenu()) {
            return;
        }
        m_openSubmenus.resize(std::min(depth, m_openSubmenus.size()));
        m_openSubmenus.push_back(item);
        if (!selectChild) {
            return;
        }
        m_hotDepth = depth + 1;
        m_hotItem = {};
        static_cast<void>(moveSelection(1));
    }

    void MenuBar::closeFromDepth(size_t depth) {
        if (m_openSubmenus.size() > depth) {
            m_openSubmenus.resize(depth);
        }
    }

    bool MenuBar::moveHeading(int direction) {
        const auto &menus = m_model->menus();
        if (menus.empty()) {
            return false;
        }
        size_t start = 0;
        const auto current =
            std::find_if(menus.begin(), menus.end(), [this](const auto &menu) {
                return menu.id == m_activeMenu;
            });
        if (current != menus.end()) {
            start = static_cast<size_t>(std::distance(menus.begin(), current));
        } else if (direction < 0) {
            start = 0;
        } else {
            start = menus.size() - 1;
        }
        for (size_t step = 1; step <= menus.size(); ++step) {
            const auto raw = static_cast<long long>(start) +
                             static_cast<long long>(direction) *
                                 static_cast<long long>(step);
            const size_t index = static_cast<size_t>(
                (raw % static_cast<long long>(menus.size()) +
                 static_cast<long long>(menus.size())) %
                static_cast<long long>(menus.size()));
            if (menus[index].enabled) {
                openMenu(menus[index].id);
                return true;
            }
        }
        return false;
    }

    bool MenuBar::moveSelection(int direction) {
        size_t depth = m_hotItem ? m_hotDepth : m_openSubmenus.size();
        const auto *items = itemsAtDepth(depth);
        if (items == nullptr || items->empty()) {
            return false;
        }
        long long current = direction > 0 ? -1 : 0;
        const auto selected = std::find_if(
            items->begin(), items->end(), [this](const MenuItem &item) {
                return item.id == m_hotItem;
            });
        if (selected != items->end()) {
            current = std::distance(items->begin(), selected);
        }
        for (size_t step = 1; step <= items->size(); ++step) {
            const auto raw = current + static_cast<long long>(direction) *
                                           static_cast<long long>(step);
            const size_t index = static_cast<size_t>(
                (raw % static_cast<long long>(items->size()) +
                 static_cast<long long>(items->size())) %
                static_cast<long long>(items->size()));
            const auto &item = (*items)[index];
            if (item.enabled && item.kind != MenuItemKind::separator) {
                m_hotItem = item.id;
                m_hotDepth = depth;
                closeFromDepth(depth);
                return true;
            }
        }
        return false;
    }

    bool MenuBar::activateSelection() {
        const auto *item = selectedItem();
        if (item == nullptr || !item->enabled ||
            item->kind == MenuItemKind::separator) {
            return false;
        }
        if (item->isSubmenu()) {
            openSubmenu(item->id, selectedDepth(), true);
            return true;
        }
        const MenuItemId id = item->id;
        close();
        return m_model->activate(id);
    }

    const std::vector<MenuItem> *
    MenuBar::itemsAtDepth(size_t depth) const noexcept {
        const auto *menu = m_model->findMenu(m_activeMenu);
        if (menu == nullptr) {
            return nullptr;
        }
        const std::vector<MenuItem> *items = &menu->items;
        for (size_t current = 0; current < depth; ++current) {
            if (current >= m_openSubmenus.size()) {
                return nullptr;
            }
            const auto *parent = findDirect(*items, m_openSubmenus[current]);
            if (parent == nullptr || !parent->isSubmenu()) {
                return nullptr;
            }
            items = &parent->children;
        }
        return items;
    }

    const MenuItem *MenuBar::selectedItem() const noexcept {
        const auto *items = itemsAtDepth(m_hotDepth);
        return items != nullptr ? findDirect(*items, m_hotItem) : nullptr;
    }

    size_t MenuBar::selectedDepth() const noexcept {
        return m_hotDepth;
    }
} // namespace Bess::UI
