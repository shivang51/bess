#include "controls/popup_controls.h"

#include "bess_core/ui/icons/font_awesome_icons.h"
#include "ui_composer.h"
#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Bess::UI {
    namespace Detail {
        struct AutocompleteSession {
            static constexpr size_t npos = std::numeric_limits<size_t>::max();

            std::vector<AutocompleteItem> items;
            size_t selected = npos;
            uint64_t revision = 0;
        };
    } // namespace Detail

    namespace {
        constexpr float kChromeZ = 0.001f;
        constexpr float kContentZ = 0.002f;

        glm::vec2 estimatedTextSize(std::string_view text,
                                    const UITextStyle &style) {
            return {std::max(0.f,
                             static_cast<float>(text.size()) * style.fontSize *
                                 0.6f),
                    std::max(1.f, style.fontSize * 1.25f)};
        }

        BoxPaint makeBox(WidgetBounds bounds,
                         const UIBoxStyle &style,
                         PickingId id,
                         float z = kChromeZ) {
            return {.bounds = bounds,
                    .color = style.background,
                    .borderColor = style.border,
                    .cornerRadius = style.cornerRadius,
                    .borderThickness = style.borderThickness,
                    .shadow = style.shadow,
                    .zIndex = z,
                    .pickingId = id};
        }

        UIBoxStyle transparentBox(const UIBoxStyle &source) {
            auto result = source;
            result.background = result.background.withAlpha(0.f);
            result.border = result.border.withAlpha(0.f);
            result.borderThickness = glm::vec4{0.f};
            result.shadow.enabled = false;
            return result;
        }

        WidgetBounds inset(WidgetBounds bounds, float amount) {
            return bounds.inset(std::max(0.f, amount));
        }

        class AutocompleteList final : public Widget {
          public:
            using Completed = std::function<void(size_t)>;

            AutocompleteList(
                std::shared_ptr<Detail::AutocompleteSession> session,
                Completed completed,
                std::optional<UIDropdownStyle> style,
                float maximumHeight)
                : m_session(std::move(session)),
                  m_completed(std::move(completed)),
                  m_style(std::move(style)),
                  m_maximumHeight(std::max(1.f, maximumHeight)) {
            }

            std::string_view typeName() const noexcept override {
                return "AutocompleteList";
            }

            WidgetTraits traits() const noexcept override {
                return {.focusable = false,
                        .hitTestVisible = true,
                        .clipChildren = false};
            }

            void onMount(WidgetMountContext &context) override {
                updateLayoutImpl(context.state, context.layout);
            }

            void updateLayout(WidgetLayoutContext &context) override {
                if (m_revision != m_session->revision || context.themeChanged) {
                    updateLayoutImpl(context.state, context.layout);
                }
            }

            void paint(WidgetPaintContext &context) const override {
                const auto &style = resolvedStyle(context.state);
                const auto &menu = context.state.theme().menus;
                ensureSelectionVisible(context.bounds, style, menu);
                const auto rows = rowBounds(context.bounds, style, menu);
                const ScopedUIClip clip{context.painter, context.bounds};
                for (size_t index = 0; index < rows.size(); ++index) {
                    const auto &row = rows[index];
                    if (row.bottomRight().y < context.bounds.topLeft().y ||
                        row.topLeft().y > context.bounds.bottomRight().y)
                        continue;
                    const auto &item = m_session->items[index];
                    if (index == m_session->selected || index == m_pressed) {
                        context.painter.drawBox(makeBox(row,
                                                        index == m_pressed
                                                            ? menu.itemPressed
                                                            : menu.itemHovered,
                                                        context.pickingId));
                    }

                    float left = row.topLeft().x + style.itemHorizontalPadding;
                    if (!item.icon.empty()) {
                        const WidgetBounds icon{
                            .center = {left + style.iconColumnWidth * 0.5f,
                                       row.center.y},
                            .size = {style.iconColumnWidth, row.size.y},
                        };
                        context.painter.drawIcon(
                            item.icon,
                            {.glyph = {
                                 .bounds = icon,
                                 .fontSize = menu.text.fontSize,
                                 .color = item.enabled ? menu.iconColor
                                                       : menu.disabledText,
                                 .horizontal = HorizontalTextAlignment::center,
                                 .vertical = VerticalTextAlignment::center,
                                 .zIndex = kContentZ,
                                 .pickingId = context.pickingId}});
                        left += style.iconColumnWidth;
                    }
                    const float right =
                        row.bottomRight().x - style.itemHorizontalPadding;
                    const float detailWidth =
                        item.detail.empty()
                            ? 0.f
                            : estimatedTextSize(item.detail, menu.text).x;
                    if (!item.detail.empty()) {
                        context.painter.drawText(
                            item.detail,
                            {.bounds = {.center = {right - detailWidth * 0.5f,
                                                   row.center.y},
                                        .size = {detailWidth, row.size.y}},
                             .fontSize = menu.text.fontSize,
                             .color = item.enabled ? menu.shortcutColor
                                                   : menu.disabledText,
                             .horizontal = HorizontalTextAlignment::end,
                             .vertical = VerticalTextAlignment::center,
                             .zIndex = kContentZ,
                             .letterSpacing = menu.text.letterSpacing,
                             .pickingId = context.pickingId});
                    }
                    const float labelRight =
                        right - (detailWidth > 0.f ? detailWidth + 12.f : 0.f);
                    context.painter.drawText(
                        item.label,
                        {.bounds = {.center = {(left + labelRight) * 0.5f,
                                               row.center.y},
                                    .size = {std::max(0.f, labelRight - left),
                                             row.size.y}},
                         .fontSize = menu.text.fontSize,
                         .color =
                             item.enabled ? menu.text.color : menu.disabledText,
                         .horizontal = HorizontalTextAlignment::start,
                         .vertical = VerticalTextAlignment::center,
                         .zIndex = kContentZ,
                         .letterSpacing = menu.text.letterSpacing,
                         .pickingId = context.pickingId});
                }
            }

            CursorIcon
            cursor(const WidgetCursorContext &) const noexcept override {
                return CursorIcon::pointer;
            }

            UIEventReply onEvent(WidgetEventContext &context,
                                 const UIEvent &event) override {
                if (context.phase != UIEventPhase::target)
                    return {};
                const auto &style = resolvedStyle(context.state);
                const auto &menu = context.state.theme().menus;
                if (event.is<Input::MouseMoveEvent>() &&
                    context.hasPointerPosition) {
                    const size_t next = itemAt(
                        context.bounds, context.pointerPosition, style, menu);
                    if (next != Detail::AutocompleteSession::npos &&
                        m_session->items[next].enabled &&
                        m_session->selected != next) {
                        m_session->selected = next;
                        return {.invalidate = WidgetInvalidation::paint};
                    }
                }
                if (const auto *wheel = event.getIf<Input::MouseWheelEvent>()) {
                    const float maximum =
                        maxScroll(context.bounds, style, menu);
                    const float next = std::clamp(
                        m_scrollOffset - wheel->offset.y * style.itemHeight,
                        0.f,
                        maximum);
                    if (next != m_scrollOffset) {
                        m_scrollOffset = next;
                        return {.handled = true,
                                .stopPropagation = true,
                                .invalidate = WidgetInvalidation::paint};
                    }
                }
                if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                    button != nullptr && button->button == MouseButton::left) {
                    const size_t hit = context.hasPointerPosition
                                           ? itemAt(context.bounds,
                                                    context.pointerPosition,
                                                    style,
                                                    menu)
                                           : Detail::AutocompleteSession::npos;
                    if (button->action == MouseButtonAction::press &&
                        hit != Detail::AutocompleteSession::npos &&
                        m_session->items[hit].enabled) {
                        m_pressed = hit;
                        m_session->selected = hit;
                        return {.handled = true,
                                .stopPropagation = true,
                                .capturePointer = true,
                                .invalidate = WidgetInvalidation::paint};
                    }
                    if (button->action == MouseButtonAction::release &&
                        m_pressed != Detail::AutocompleteSession::npos) {
                        const size_t pressed = m_pressed;
                        m_pressed = Detail::AutocompleteSession::npos;
                        if (pressed == hit && m_completed)
                            m_completed(pressed);
                        return {.handled = true,
                                .stopPropagation = true,
                                .releasePointer = true,
                                .invalidate = WidgetInvalidation::paint};
                    }
                }
                return {};
            }

          private:
            const UIDropdownStyle &
            resolvedStyle(const WidgetTree &state) const {
                return m_style ? *m_style : state.theme().dropdown;
            }

            float totalHeight(const UIDropdownStyle &style,
                              const UIMenuStyle &menu) const noexcept {
                return std::max(0.f, menu.popupPadding) * 2.f +
                       static_cast<float>(m_session->items.size()) *
                           std::max(1.f, style.itemHeight);
            }

            float maxScroll(WidgetBounds bounds,
                            const UIDropdownStyle &style,
                            const UIMenuStyle &menu) const noexcept {
                return std::max(0.f, totalHeight(style, menu) - bounds.size.y);
            }

            std::vector<WidgetBounds> rowBounds(WidgetBounds bounds,
                                                const UIDropdownStyle &style,
                                                const UIMenuStyle &menu) const {
                std::vector<WidgetBounds> result;
                result.reserve(m_session->items.size());
                const float padding = std::max(0.f, menu.popupPadding);
                const float height = std::max(1.f, style.itemHeight);
                float top = bounds.topLeft().y + padding - m_scrollOffset;
                for (size_t index = 0; index < m_session->items.size();
                     ++index) {
                    result.push_back(
                        {.center = {bounds.center.x, top + height * 0.5f},
                         .size = {std::max(0.f, bounds.size.x - padding * 2.f),
                                  height}});
                    top += height;
                }
                return result;
            }

            size_t itemAt(WidgetBounds bounds,
                          glm::vec2 position,
                          const UIDropdownStyle &style,
                          const UIMenuStyle &menu) const {
                if (!bounds.contains(position))
                    return Detail::AutocompleteSession::npos;
                const auto rows = rowBounds(bounds, style, menu);
                for (size_t index = 0; index < rows.size(); ++index) {
                    if (rows[index].contains(position))
                        return index;
                }
                return Detail::AutocompleteSession::npos;
            }

            void ensureSelectionVisible(WidgetBounds bounds,
                                        const UIDropdownStyle &style,
                                        const UIMenuStyle &menu) const {
                if (m_session->selected == Detail::AutocompleteSession::npos ||
                    m_session->selected >= m_session->items.size())
                    return;
                const float padding = std::max(0.f, menu.popupPadding);
                const float height = std::max(1.f, style.itemHeight);
                const float top =
                    padding + static_cast<float>(m_session->selected) * height;
                const float bottom = top + height;
                if (top < m_scrollOffset)
                    m_scrollOffset = top;
                else if (bottom > m_scrollOffset + bounds.size.y)
                    m_scrollOffset = bottom - bounds.size.y;
                m_scrollOffset = std::clamp(
                    m_scrollOffset, 0.f, maxScroll(bounds, style, menu));
            }

            void updateLayoutImpl(WidgetTree &state, LayoutNode &layout) {
                const auto &style = resolvedStyle(state);
                const auto &menu = state.theme().menus;
                float width = style.minimumWidth;
                for (const auto &item : m_session->items) {
                    width = std::max(
                        width,
                        estimatedTextSize(item.label, menu.text).x +
                            estimatedTextSize(item.detail, menu.text).x +
                            style.itemHorizontalPadding * 2.f +
                            (item.icon.empty() ? 0.f : style.iconColumnWidth) +
                            (item.detail.empty() ? 0.f : 12.f));
                }
                layout.setWidth(width);
                layout.setHeight(
                    std::min(totalHeight(style, menu), m_maximumHeight));
                m_revision = m_session->revision;
            }

            std::shared_ptr<Detail::AutocompleteSession> m_session;
            Completed m_completed;
            std::optional<UIDropdownStyle> m_style;
            float m_maximumHeight = 280.f;
            uint64_t m_revision = 0;
            mutable float m_scrollOffset = 0.f;
            size_t m_pressed = Detail::AutocompleteSession::npos;
        };

        class DropdownList final : public Widget {
          public:
            using Selected = std::function<void(DropdownItemId)>;

            DropdownList(std::shared_ptr<DropdownModel> model,
                         Selected selected,
                         std::optional<UIDropdownStyle> style)
                : m_model(std::move(model)),
                  m_selected(std::move(selected)),
                  m_style(std::move(style)) {
            }

            std::string_view typeName() const noexcept override {
                return "DropdownList";
            }

            WidgetTraits traits() const noexcept override {
                return {.focusable = true,
                        .hitTestVisible = true,
                        .clipChildren = false};
            }

            void onMount(WidgetMountContext &context) override {
                m_state = &context.state;
                m_id = context.id;
                reconnect();
                updateLayoutImpl(context.state, context.layout);
            }

            void onUnmount(WidgetTree &, WidgetId) override {
                m_connection.disconnect();
                m_state = nullptr;
                m_id = {};
            }

            void updateLayout(WidgetLayoutContext &context) override {
                if (m_layoutDirty || context.themeChanged) {
                    updateLayoutImpl(context.state, context.layout);
                }
            }

            void paint(WidgetPaintContext &context) const override {
                const auto &dropdown = resolvedStyle(context.state);
                const auto &menu = context.state.theme().menus;
                const float padding = std::max(0.f, menu.popupPadding);
                const auto rows = rowBounds(context.bounds, dropdown, padding);
                const ScopedUIClip clip{context.painter, context.bounds};
                for (size_t index = 0; index < rows.size(); ++index) {
                    const auto &item = m_model->items()[index];
                    const auto &row = rows[index];
                    if (row.bottomRight().y < context.bounds.topLeft().y ||
                        row.topLeft().y > context.bounds.bottomRight().y) {
                        continue;
                    }
                    if (item.id == m_hot || item.id == m_model->selection()) {
                        const auto &box = item.id == m_pressed
                                              ? menu.itemPressed
                                              : menu.itemHovered;
                        context.painter.drawBox(
                            makeBox(row, box, context.pickingId, kChromeZ));
                    }
                    const float left =
                        row.topLeft().x + dropdown.itemHorizontalPadding;
                    float labelLeft = left;
                    if (!item.icon.empty()) {
                        const WidgetBounds iconBounds{
                            .center = {left + dropdown.iconColumnWidth * 0.5f,
                                       row.center.y},
                            .size = {dropdown.iconColumnWidth, row.size.y},
                        };
                        context.painter.drawIcon(
                            item.icon,
                            {.glyph = {
                                 .bounds = iconBounds,
                                 .fontSize = menu.text.fontSize,
                                 .color = item.enabled ? menu.iconColor
                                                       : menu.disabledText,
                                 .horizontal = HorizontalTextAlignment::center,
                                 .vertical = VerticalTextAlignment::center,
                                 .zIndex = kContentZ,
                                 .pickingId = context.pickingId}});
                        labelLeft += dropdown.iconColumnWidth;
                    }
                    const float labelWidth = std::max(
                        0.f,
                        row.bottomRight().x - dropdown.itemHorizontalPadding -
                            labelLeft);
                    context.painter.drawText(
                        item.label,
                        {.bounds = {.center = {labelLeft + labelWidth * 0.5f,
                                               row.center.y},
                                    .size = {labelWidth, row.size.y}},
                         .fontSize = menu.text.fontSize,
                         .color =
                             item.enabled ? menu.text.color : menu.disabledText,
                         .horizontal = HorizontalTextAlignment::start,
                         .vertical = VerticalTextAlignment::center,
                         .zIndex = kContentZ,
                         .letterSpacing = menu.text.letterSpacing,
                         .pickingId = context.pickingId});
                }

                const float total = totalHeight(dropdown, padding);
                if (total > context.bounds.size.y) {
                    const float viewport = context.bounds.size.y;
                    const float thumbHeight = std::max(
                        18.f, viewport * viewport / std::max(total, 1.f));
                    const float travel = std::max(0.f, viewport - thumbHeight);
                    const float maxScroll = std::max(0.f, total - viewport);
                    const float ratio =
                        maxScroll > 0.f
                            ? static_cast<float>(m_scrollOffset / maxScroll)
                            : 0.f;
                    WidgetBounds thumb{
                        .center = {context.bounds.bottomRight().x - 2.f,
                                   context.bounds.topLeft().y +
                                       thumbHeight * 0.5f + travel * ratio},
                        .size = {3.f, thumbHeight},
                    };
                    context.painter.drawBox(
                        makeBox(thumb,
                                context.state.theme().scroll.thumb,
                                context.pickingId,
                                kContentZ + 0.001f));
                }
            }

            CursorIcon
            cursor(const WidgetCursorContext &) const noexcept override {
                return CursorIcon::pointer;
            }

            UIEventReply onEvent(WidgetEventContext &context,
                                 const UIEvent &event) override {
                if (context.phase != UIEventPhase::target) {
                    return {};
                }
                UIEventReply reply;
                const auto &dropdown = resolvedStyle(context.state);
                const float padding =
                    std::max(0.f, context.state.theme().menus.popupPadding);
                if (const auto *focus = event.getIf<UIFocusChangedEvent>()) {
                    if (focus->focused) {
                        m_hot = m_model->selection();
                    } else {
                        m_pressed = {};
                    }
                    return {.invalidate = WidgetInvalidation::paint};
                }
                if (event.is<Input::MouseMoveEvent>() &&
                    context.hasPointerPosition) {
                    const auto next = itemAt(context.bounds,
                                             context.pointerPosition,
                                             dropdown,
                                             padding);
                    if (m_hot != next) {
                        m_hot = next;
                        reply.invalidate = WidgetInvalidation::paint;
                    }
                    return reply;
                }
                if (const auto *wheel = event.getIf<Input::MouseWheelEvent>();
                    wheel != nullptr) {
                    const float maximum = std::max(
                        0.f,
                        totalHeight(dropdown, padding) - context.bounds.size.y);
                    const float next = std::clamp(
                        m_scrollOffset - wheel->offset.y * dropdown.itemHeight,
                        0.f,
                        maximum);
                    if (next != m_scrollOffset) {
                        m_scrollOffset = next;
                        reply.handled = true;
                        reply.invalidate = WidgetInvalidation::paint;
                    }
                    return reply;
                }
                if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                    button != nullptr && button->button == MouseButton::left) {
                    const DropdownItemId hit =
                        context.hasPointerPosition
                            ? itemAt(context.bounds,
                                     context.pointerPosition,
                                     dropdown,
                                     padding)
                            : DropdownItemId{};
                    const auto *item = m_model->find(hit);
                    if (button->action == MouseButtonAction::press &&
                        item != nullptr && item->enabled) {
                        m_pressed = hit;
                        m_hot = hit;
                        return {.handled = true,
                                .stopPropagation = true,
                                .requestFocus = true,
                                .capturePointer = true,
                                .invalidate = WidgetInvalidation::paint};
                    }
                    if (button->action == MouseButtonAction::release &&
                        m_pressed) {
                        const DropdownItemId pressed = m_pressed;
                        m_pressed = {};
                        if (pressed == hit && m_selected) {
                            m_selected(pressed);
                        }
                        return {.handled = true,
                                .stopPropagation = true,
                                .releasePointer = true,
                                .invalidate = WidgetInvalidation::paint};
                    }
                }
                if (const auto *key = event.getIf<Input::KeyEvent>();
                    key != nullptr && context.focused &&
                    (key->action == KeyAction::press ||
                     key->action == KeyAction::hold)) {
                    int direction = 0;
                    if (key->key == KeyCode::arrowDown) {
                        direction = 1;
                    } else if (key->key == KeyCode::arrowUp) {
                        direction = -1;
                    }
                    if (direction != 0) {
                        const auto next = m_model->nextEnabled(
                            m_hot ? m_hot : m_model->selection(), direction);
                        if (next) {
                            m_hot = next;
                            ensureVisible(
                                context.bounds, next, dropdown, padding);
                        }
                        return {.handled = true,
                                .stopPropagation = true,
                                .invalidate = WidgetInvalidation::paint};
                    }
                    if ((key->key == KeyCode::enter ||
                         key->key == KeyCode::space) &&
                        m_hot && m_selected) {
                        m_selected(m_hot);
                        return {.handled = true, .stopPropagation = true};
                    }
                }
                return reply;
            }

          private:
            const UIDropdownStyle &
            resolvedStyle(const WidgetTree &state) const {
                return m_style ? *m_style : state.theme().dropdown;
            }

            float totalHeight(const UIDropdownStyle &style,
                              float padding) const noexcept {
                return padding * 2.f +
                       static_cast<float>(m_model->items().size()) *
                           std::max(1.f, style.itemHeight);
            }

            std::vector<WidgetBounds> rowBounds(WidgetBounds bounds,
                                                const UIDropdownStyle &style,
                                                float padding) const {
                std::vector<WidgetBounds> result;
                result.reserve(m_model->items().size());
                const float height = std::max(1.f, style.itemHeight);
                float top = bounds.topLeft().y + padding - m_scrollOffset;
                for (size_t index = 0; index < m_model->items().size();
                     ++index) {
                    result.push_back(
                        {.center = {bounds.center.x, top + height * 0.5f},
                         .size = {std::max(0.f, bounds.size.x - padding * 2.f),
                                  height}});
                    top += height;
                }
                return result;
            }

            DropdownItemId itemAt(WidgetBounds bounds,
                                  glm::vec2 point,
                                  const UIDropdownStyle &style,
                                  float padding) const {
                const auto rows = rowBounds(bounds, style, padding);
                for (size_t index = 0; index < rows.size(); ++index) {
                    if (rows[index].contains(point) && bounds.contains(point)) {
                        return m_model->items()[index].id;
                    }
                }
                return {};
            }

            void ensureVisible(WidgetBounds bounds,
                               DropdownItemId item,
                               const UIDropdownStyle &style,
                               float padding) {
                const size_t index = m_model->indexOf(item);
                if (index == DropdownModel::npos) {
                    return;
                }
                const float height = std::max(1.f, style.itemHeight);
                const float rowTop =
                    padding + static_cast<float>(index) * height;
                const float rowBottom = rowTop + height;
                const float viewport = bounds.size.y;
                if (rowTop < m_scrollOffset) {
                    m_scrollOffset = rowTop;
                } else if (rowBottom > m_scrollOffset + viewport) {
                    m_scrollOffset = rowBottom - viewport;
                }
                m_scrollOffset = std::clamp(
                    m_scrollOffset,
                    0.f,
                    std::max(0.f, totalHeight(style, padding) - viewport));
            }

            void updateLayoutImpl(WidgetTree &state, LayoutNode &layout) {
                const auto &style = resolvedStyle(state);
                const auto &menu = state.theme().menus;
                float width = style.minimumWidth;
                for (const auto &item : m_model->items()) {
                    width = std::max(
                        width,
                        estimatedTextSize(item.label, menu.text).x +
                            style.itemHorizontalPadding * 2.f +
                            (item.icon.empty() ? 0.f : style.iconColumnWidth));
                }
                const float padding = std::max(0.f, menu.popupPadding);
                const float height = std::min(
                    totalHeight(style, padding),
                    std::max(style.itemHeight, style.popupMaximumHeight));
                layout.setWidth(width);
                layout.setHeight(height);
                m_layoutDirty = false;
            }

            void reconnect() {
                m_connection.disconnect();
                if (m_model == nullptr || m_state == nullptr) {
                    return;
                }
                m_connection = m_model->changed().connect([this](const auto &) {
                    m_layoutDirty = true;
                    if (m_state != nullptr && m_state->contains(m_id)) {
                        m_state->invalidate(m_id,
                                            WidgetInvalidation::layout |
                                                WidgetInvalidation::paint);
                    }
                });
            }

            std::shared_ptr<DropdownModel> m_model;
            Selected m_selected;
            std::optional<UIDropdownStyle> m_style;
            DropdownModel::ChangedSignal::Connection m_connection;
            WidgetTree *m_state = nullptr;
            WidgetId m_id;
            DropdownItemId m_hot;
            DropdownItemId m_pressed;
            float m_scrollOffset = 0.f;
            bool m_layoutDirty = true;
        };

        struct ContextRow {
            MenuItemId item;
            WidgetBounds bounds;
            bool separator = false;
        };

        struct ContextPanel {
            WidgetBounds bounds;
            std::vector<ContextRow> rows;
            float contentHeight = 0.f;
        };

        class ContextMenuWidget final : public Widget {
          public:
            ContextMenuWidget(std::shared_ptr<MenuModel> model,
                              MenuId menu,
                              std::optional<UIMenuStyle> style,
                              std::function<void()> dismissed,
                              float maximumHeight)
                : m_model(std::move(model)),
                  m_menu(menu),
                  m_style(std::move(style)),
                  m_dismissed(std::move(dismissed)),
                  m_maximumHeight(std::max(1.f, maximumHeight)) {
            }

            std::string_view typeName() const noexcept override {
                return "ContextMenu";
            }

            WidgetTraits traits() const noexcept override {
                return {.focusable = true,
                        .hitTestVisible = true,
                        .clipChildren = false};
            }

            void onMount(WidgetMountContext &context) override {
                m_state = &context.state;
                m_id = context.id;
                reconnect();
                updateLayoutImpl(context.state, context.layout);
            }

            void onUnmount(WidgetTree &, WidgetId) override {
                m_connection.disconnect();
                m_state = nullptr;
                m_id = {};
            }

            void updateLayout(WidgetLayoutContext &context) override {
                if (m_layoutDirty || context.themeChanged) {
                    updateLayoutImpl(context.state, context.layout);
                }
            }

            void paint(WidgetPaintContext &context) const override {
                rebuild(context.bounds, context.state);
                const auto &style = resolvedStyle(context.state);
                for (size_t depth = 0; depth < m_panels.size(); ++depth) {
                    const auto &panel = m_panels[depth];
                    context.painter.drawBox(
                        makeBox(panel.bounds,
                                style.popup,
                                context.pickingId,
                                kChromeZ + static_cast<float>(depth) * 0.01f));
                    const ScopedUIClip clip{context.painter, panel.bounds};
                    for (const auto &row : panel.rows) {
                        if (row.bounds.bottomRight().y <
                                panel.bounds.topLeft().y ||
                            row.bounds.topLeft().y >
                                panel.bounds.bottomRight().y) {
                            continue;
                        }
                        if (row.separator) {
                            WidgetBounds line{
                                .center = row.bounds.center,
                                .size = {std::max(
                                             0.f,
                                             row.bounds.size.x -
                                                 style.itemHorizontalPadding *
                                                     2.f),
                                         1.f},
                            };
                            UIBoxStyle separator{.background = style.separator};
                            context.painter.drawBox(makeBox(
                                line, separator, context.pickingId, kContentZ));
                            continue;
                        }
                        const auto *item = m_model->findItem(row.item);
                        if (item == nullptr) {
                            continue;
                        }
                        if (row.item == m_hot || row.item == m_pressed) {
                            context.painter.drawBox(makeBox(
                                row.bounds,
                                row.item == m_pressed ? style.itemPressed
                                                      : style.itemHovered,
                                context.pickingId,
                                kContentZ));
                        }
                        paintRow(context, row.bounds, *item, style, depth);
                    }
                    if (panel.contentHeight > panel.bounds.size.y) {
                        const float viewport = panel.bounds.size.y;
                        const float thumbHeight = std::max(
                            18.f, viewport * viewport / panel.contentHeight);
                        const float maximum =
                            panel.contentHeight - panel.bounds.size.y;
                        const float travel = viewport - thumbHeight;
                        const float ratio =
                            maximum > 0.f && depth < m_panelScroll.size()
                                ? m_panelScroll[depth] / maximum
                                : 0.f;
                        const WidgetBounds thumb{
                            .center = {panel.bounds.bottomRight().x - 2.f,
                                       panel.bounds.topLeft().y +
                                           thumbHeight * 0.5f + travel * ratio},
                            .size = {3.f, thumbHeight},
                        };
                        context.painter.drawBox(
                            makeBox(thumb,
                                    context.state.theme().scroll.thumb,
                                    context.pickingId,
                                    kContentZ + 0.001f +
                                        static_cast<float>(depth) * 0.01f));
                    }
                }
            }

            bool hitTest(WidgetBounds bounds,
                         glm::vec2 position) const noexcept override {
                if (m_state != nullptr) {
                    rebuild(bounds, *m_state);
                }
                return std::any_of(m_panels.begin(),
                                   m_panels.end(),
                                   [position](const auto &panel) {
                                       return panel.bounds.contains(position);
                                   });
            }

            CursorIcon
            cursor(const WidgetCursorContext &) const noexcept override {
                return CursorIcon::pointer;
            }

            UIEventReply onEvent(WidgetEventContext &context,
                                 const UIEvent &event) override {
                if (context.phase != UIEventPhase::target) {
                    return {};
                }
                rebuild(context.bounds, context.state);
                if (const auto *focus = event.getIf<UIFocusChangedEvent>()) {
                    if (focus->focused && !m_hot) {
                        selectFirst(0);
                    }
                    if (!focus->focused) {
                        m_pressed = {};
                    }
                    return {.invalidate = WidgetInvalidation::paint};
                }
                if (event.is<Input::MouseMoveEvent>() &&
                    context.hasPointerPosition) {
                    size_t depth = 0;
                    const MenuItemId hit =
                        itemAt(context.pointerPosition, depth);
                    if (hit != m_hot || depth != m_hotDepth) {
                        m_hot = hit;
                        m_hotDepth = depth;
                        updateOpenPathForHot();
                        return {.invalidate = WidgetInvalidation::paint};
                    }
                    return {};
                }
                if (const auto *wheel = event.getIf<Input::MouseWheelEvent>();
                    wheel != nullptr && context.hasPointerPosition) {
                    for (size_t depth = m_panels.size(); depth-- > 0;) {
                        const auto &panel = m_panels[depth];
                        if (!panel.bounds.contains(context.pointerPosition) ||
                            panel.contentHeight <= panel.bounds.size.y) {
                            continue;
                        }
                        if (m_panelScroll.size() <= depth)
                            m_panelScroll.resize(depth + 1, 0.f);
                        const float maximum =
                            panel.contentHeight - panel.bounds.size.y;
                        const float next = std::clamp(
                            m_panelScroll[depth] -
                                wheel->offset.y *
                                    styleForWheel(context).itemHeight,
                            0.f,
                            maximum);
                        if (next != m_panelScroll[depth]) {
                            m_panelScroll[depth] = next;
                            rebuild(context.bounds, context.state);
                            return {.handled = true,
                                    .stopPropagation = true,
                                    .invalidate = WidgetInvalidation::paint};
                        }
                        return {.handled = true, .stopPropagation = true};
                    }
                }
                if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                    button != nullptr && button->button == MouseButton::left) {
                    size_t depth = 0;
                    const MenuItemId hit =
                        context.hasPointerPosition
                            ? itemAt(context.pointerPosition, depth)
                            : MenuItemId{};
                    const auto *item = m_model->findItem(hit);
                    if (button->action == MouseButtonAction::press &&
                        item != nullptr && item->enabled &&
                        item->kind == MenuItemKind::command) {
                        m_pressed = hit;
                        m_hot = hit;
                        m_hotDepth = depth;
                        updateOpenPathForHot();
                        return {.handled = true,
                                .stopPropagation = true,
                                .requestFocus = true,
                                .capturePointer = true,
                                .invalidate = WidgetInvalidation::paint};
                    }
                    if (button->action == MouseButtonAction::release &&
                        m_pressed) {
                        const MenuItemId pressed = m_pressed;
                        m_pressed = {};
                        if (pressed == hit) {
                            activate(pressed);
                        }
                        return {.handled = true,
                                .stopPropagation = true,
                                .releasePointer = true,
                                .invalidate = WidgetInvalidation::paint};
                    }
                }
                if (const auto *key = event.getIf<Input::KeyEvent>();
                    key != nullptr && context.focused &&
                    (key->action == KeyAction::press ||
                     key->action == KeyAction::hold)) {
                    switch (key->key) {
                    case KeyCode::arrowDown:
                        moveSelection(1);
                        break;
                    case KeyCode::arrowUp:
                        moveSelection(-1);
                        break;
                    case KeyCode::arrowRight:
                        openSelectedSubmenu();
                        break;
                    case KeyCode::arrowLeft:
                        closeSelectedSubmenu();
                        break;
                    case KeyCode::enter:
                    case KeyCode::space:
                        if (const auto *item = m_model->findItem(m_hot);
                            item != nullptr && item->isSubmenu()) {
                            openSelectedSubmenu();
                        } else {
                            activate(m_hot);
                        }
                        break;
                    default:
                        return {};
                    }
                    rebuild(context.bounds, context.state);
                    ensureHotVisible();
                    return {.handled = true,
                            .stopPropagation = true,
                            .invalidate = WidgetInvalidation::paint};
                }
                return {};
            }

          private:
            const UIMenuStyle &resolvedStyle(const WidgetTree &state) const {
                return m_style ? *m_style : state.theme().menus;
            }

            const UIMenuStyle &
            styleForWheel(const WidgetEventContext &context) const {
                return resolvedStyle(context.state);
            }

            const std::vector<MenuItem> *itemsAtDepth(size_t depth) const {
                const auto *definition = m_model->findMenu(m_menu);
                if (definition == nullptr) {
                    return nullptr;
                }
                const std::vector<MenuItem> *items = &definition->items;
                for (size_t current = 0; current < depth; ++current) {
                    if (current >= m_openPath.size()) {
                        return nullptr;
                    }
                    const auto *parent = m_model->findItem(m_openPath[current]);
                    if (parent == nullptr || !parent->isSubmenu()) {
                        return nullptr;
                    }
                    items = &parent->children;
                }
                return items;
            }

            glm::vec2 panelSize(const std::vector<MenuItem> &items,
                                const UIMenuStyle &style) const {
                float width = style.popupMinimumWidth;
                float height = std::max(0.f, style.popupPadding) * 2.f;
                for (const auto &item : items) {
                    if (item.kind == MenuItemKind::separator) {
                        height += style.separatorHeight;
                        continue;
                    }
                    const float text =
                        estimatedTextSize(item.name, style.text).x;
                    const float shortcut =
                        estimatedTextSize(item.shortcut, style.text).x;
                    width = std::max(width,
                                     style.itemHorizontalPadding * 2.f +
                                         style.iconColumnWidth + text +
                                         (item.shortcut.empty()
                                              ? 0.f
                                              : style.shortcutGap + shortcut) +
                                         (item.isSubmenu()
                                              ? style.submenuIndicatorWidth
                                              : 0.f));
                    height += style.itemHeight;
                }
                width = std::clamp(
                    width, style.popupMinimumWidth, style.popupMaximumWidth);
                return {width, std::min(height, m_maximumHeight)};
            }

            void rebuild(WidgetBounds rootBounds,
                         const WidgetTree &state) const {
                const auto &style = resolvedStyle(state);
                const auto *rootItems = itemsAtDepth(0);
                m_panels.clear();
                if (rootItems == nullptr) {
                    return;
                }
                WidgetBounds viewport{.center = {},
                                      .size = state.getViewportSize()};
                appendPanel(rootBounds, *rootItems, style, 0);
                for (size_t depth = 0; depth < m_openPath.size(); ++depth) {
                    if (depth >= m_panels.size()) {
                        break;
                    }
                    const auto row = std::find_if(
                        m_panels[depth].rows.begin(),
                        m_panels[depth].rows.end(),
                        [this, depth](const ContextRow &candidate) {
                            return candidate.item == m_openPath[depth];
                        });
                    const auto *items = itemsAtDepth(depth + 1);
                    if (row == m_panels[depth].rows.end() || items == nullptr) {
                        break;
                    }
                    const glm::vec2 size = panelSize(*items, style);
                    float left = m_panels[depth].bounds.bottomRight().x -
                                 std::max(0.f, style.popupOverlap);
                    if (left + size.x > viewport.bottomRight().x - 8.f) {
                        left = m_panels[depth].bounds.topLeft().x - size.x +
                               std::max(0.f, style.popupOverlap);
                    }
                    float top = row->bounds.topLeft().y -
                                std::max(0.f, style.popupPadding);
                    left = std::clamp(left,
                                      viewport.topLeft().x + 8.f,
                                      viewport.bottomRight().x - 8.f - size.x);
                    top = std::clamp(top,
                                     viewport.topLeft().y + 8.f,
                                     viewport.bottomRight().y - 8.f - size.y);
                    appendPanel(
                        {.center = {left + size.x * 0.5f, top + size.y * 0.5f},
                         .size = size},
                        *items,
                        style,
                        depth + 1);
                }
            }

            void appendPanel(WidgetBounds bounds,
                             const std::vector<MenuItem> &items,
                             const UIMenuStyle &style,
                             size_t depth) const {
                const float padding = std::max(0.f, style.popupPadding);
                float contentHeight = padding * 2.f;
                for (const auto &item : items) {
                    contentHeight += item.kind == MenuItemKind::separator
                                         ? style.separatorHeight
                                         : style.itemHeight;
                }
                if (m_panelScroll.size() <= depth)
                    m_panelScroll.resize(depth + 1, 0.f);
                m_panelScroll[depth] =
                    std::clamp(m_panelScroll[depth],
                               0.f,
                               std::max(0.f, contentHeight - bounds.size.y));
                ContextPanel panel{.bounds = bounds,
                                   .contentHeight = contentHeight};
                float top = bounds.topLeft().y + padding - m_panelScroll[depth];
                for (const auto &item : items) {
                    const float height = item.kind == MenuItemKind::separator
                                             ? style.separatorHeight
                                             : style.itemHeight;
                    panel.rows.push_back(
                        {.item = item.id,
                         .bounds =
                             {.center = {bounds.center.x, top + height * 0.5f},
                              .size = {std::max(0.f,
                                                bounds.size.x - padding * 2.f),
                                       height}},
                         .separator = item.kind == MenuItemKind::separator});
                    top += height;
                }
                if (m_panels.size() == depth) {
                    m_panels.push_back(std::move(panel));
                }
            }

            MenuItemId itemAt(glm::vec2 point, size_t &depth) const {
                for (size_t index = m_panels.size(); index-- > 0;) {
                    if (!m_panels[index].bounds.contains(point)) {
                        continue;
                    }
                    depth = index;
                    for (const auto &row : m_panels[index].rows) {
                        if (!row.separator && row.bounds.contains(point)) {
                            return row.item;
                        }
                    }
                    return {};
                }
                return {};
            }

            void paintRow(WidgetPaintContext &context,
                          WidgetBounds row,
                          const MenuItem &item,
                          const UIMenuStyle &style,
                          size_t depth) const {
                const float left =
                    row.topLeft().x + style.itemHorizontalPadding;
                const WidgetBounds iconBounds{
                    .center = {left + style.iconColumnWidth * 0.5f,
                               row.center.y},
                    .size = {style.iconColumnWidth, row.size.y},
                };
                if (!item.icon.empty()) {
                    context.painter.drawIcon(
                        item.icon,
                        {.glyph = {.bounds = iconBounds,
                                   .fontSize = style.text.fontSize,
                                   .color = item.enabled ? style.iconColor
                                                         : style.disabledText,
                                   .horizontal =
                                       HorizontalTextAlignment::center,
                                   .vertical = VerticalTextAlignment::center,
                                   .zIndex = kContentZ +
                                             static_cast<float>(depth) * 0.01f,
                                   .pickingId = context.pickingId}});
                }
                const float rightReserve =
                    style.itemHorizontalPadding +
                    (item.isSubmenu() ? style.submenuIndicatorWidth : 0.f) +
                    (item.shortcut.empty()
                         ? 0.f
                         : estimatedTextSize(item.shortcut, style.text).x +
                               style.shortcutGap);
                const float labelLeft = left + style.iconColumnWidth;
                const float labelWidth = std::max(
                    0.f, row.bottomRight().x - rightReserve - labelLeft);
                context.painter.drawText(
                    item.name,
                    {.bounds = {.center = {labelLeft + labelWidth * 0.5f,
                                           row.center.y},
                                .size = {labelWidth, row.size.y}},
                     .fontSize = style.text.fontSize,
                     .color =
                         item.enabled ? style.text.color : style.disabledText,
                     .horizontal = HorizontalTextAlignment::start,
                     .vertical = VerticalTextAlignment::center,
                     .zIndex = kContentZ + static_cast<float>(depth) * 0.01f,
                     .letterSpacing = style.text.letterSpacing,
                     .pickingId = context.pickingId});
                if (!item.shortcut.empty()) {
                    WidgetBounds shortcutBounds{
                        .center = {row.bottomRight().x -
                                       style.itemHorizontalPadding -
                                       (item.isSubmenu()
                                            ? style.submenuIndicatorWidth
                                            : 0.f) -
                                       style.shortcutGap * 0.5f,
                                   row.center.y},
                        .size = {rightReserve, row.size.y},
                    };
                    context.painter.drawText(
                        item.shortcut,
                        {.bounds = shortcutBounds,
                         .fontSize = style.text.fontSize,
                         .color = item.enabled ? style.shortcutColor
                                               : style.disabledText,
                         .horizontal = HorizontalTextAlignment::end,
                         .vertical = VerticalTextAlignment::center,
                         .zIndex =
                             kContentZ + static_cast<float>(depth) * 0.01f,
                         .letterSpacing = style.text.letterSpacing,
                         .pickingId = context.pickingId});
                }
                if (item.isSubmenu()) {
                    WidgetBounds chevron{
                        .center = {row.bottomRight().x -
                                       style.itemHorizontalPadding -
                                       style.submenuIndicatorWidth * 0.5f,
                                   row.center.y},
                        .size = {style.submenuIndicatorWidth, row.size.y},
                    };
                    context.painter.drawIcon(
                        Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT,
                        {.glyph = {.bounds = chevron,
                                   .fontSize = style.submenuChevronSize,
                                   .color = item.enabled ? style.iconColor
                                                         : style.disabledText,
                                   .horizontal =
                                       HorizontalTextAlignment::center,
                                   .vertical = VerticalTextAlignment::center,
                                   .zIndex = kContentZ +
                                             static_cast<float>(depth) * 0.01f,
                                   .pickingId = context.pickingId}});
                }
            }

            void updateOpenPathForHot() {
                const auto *item = m_model->findItem(m_hot);
                if (item != nullptr && item->isSubmenu() && item->enabled) {
                    if (m_openPath.size() > m_hotDepth) {
                        m_openPath.resize(m_hotDepth);
                    }
                    m_openPath.push_back(item->id);
                } else if (m_openPath.size() > m_hotDepth) {
                    m_openPath.resize(m_hotDepth);
                }
            }

            void selectFirst(size_t depth) {
                const auto *items = itemsAtDepth(depth);
                if (items == nullptr) {
                    m_hot = {};
                    return;
                }
                const auto it = std::find_if(
                    items->begin(), items->end(), [](const MenuItem &item) {
                        return item.kind == MenuItemKind::command &&
                               item.enabled;
                    });
                m_hot = it != items->end() ? it->id : MenuItemId{};
                m_hotDepth = depth;
            }

            void moveSelection(int direction) {
                const auto *items = itemsAtDepth(m_hotDepth);
                if (items == nullptr || items->empty()) {
                    return;
                }
                size_t index = 0;
                const auto current = std::find_if(
                    items->begin(), items->end(), [this](const MenuItem &item) {
                        return item.id == m_hot;
                    });
                if (current != items->end()) {
                    index = static_cast<size_t>(current - items->begin());
                } else {
                    index = direction > 0 ? items->size() - 1 : 0;
                }
                for (size_t attempt = 0; attempt < items->size(); ++attempt) {
                    index = direction > 0
                                ? (index + 1) % items->size()
                                : (index + items->size() - 1) % items->size();
                    const auto &candidate = (*items)[index];
                    if (candidate.kind == MenuItemKind::command &&
                        candidate.enabled) {
                        m_hot = candidate.id;
                        updateOpenPathForHot();
                        return;
                    }
                }
            }

            void openSelectedSubmenu() {
                const auto *item = m_model->findItem(m_hot);
                if (item == nullptr || !item->enabled || !item->isSubmenu()) {
                    return;
                }
                if (m_openPath.size() > m_hotDepth) {
                    m_openPath.resize(m_hotDepth);
                }
                m_openPath.push_back(item->id);
                selectFirst(m_hotDepth + 1);
            }

            void closeSelectedSubmenu() {
                if (m_hotDepth == 0 || m_openPath.empty()) {
                    return;
                }
                m_hotDepth -= 1;
                m_hot = m_openPath[m_hotDepth];
                m_openPath.resize(m_hotDepth);
            }

            void ensureHotVisible() {
                if (!m_hot || m_hotDepth >= m_panels.size() ||
                    m_hotDepth >= m_panelScroll.size()) {
                    return;
                }
                const auto &panel = m_panels[m_hotDepth];
                const auto row =
                    std::find_if(panel.rows.begin(),
                                 panel.rows.end(),
                                 [this](const ContextRow &candidate) {
                                     return candidate.item == m_hot;
                                 });
                if (row == panel.rows.end())
                    return;
                const float top = panel.bounds.topLeft().y;
                const float bottom = panel.bounds.bottomRight().y;
                if (row->bounds.topLeft().y < top)
                    m_panelScroll[m_hotDepth] -= top - row->bounds.topLeft().y;
                else if (row->bounds.bottomRight().y > bottom)
                    m_panelScroll[m_hotDepth] +=
                        row->bounds.bottomRight().y - bottom;
                m_panelScroll[m_hotDepth] = std::clamp(
                    m_panelScroll[m_hotDepth],
                    0.f,
                    std::max(0.f, panel.contentHeight - panel.bounds.size.y));
            }

            void activate(MenuItemId item) {
                const auto *definition = m_model->findItem(item);
                if (definition == nullptr || !definition->enabled ||
                    definition->kind != MenuItemKind::command ||
                    definition->isSubmenu()) {
                    return;
                }
                if (m_model->activate(item) && m_dismissed) {
                    m_dismissed();
                }
            }

            void updateLayoutImpl(WidgetTree &state, LayoutNode &layout) {
                const auto *definition = m_model->findMenu(m_menu);
                const auto &style = resolvedStyle(state);
                const glm::vec2 size =
                    definition != nullptr
                        ? panelSize(definition->items, style)
                        : glm::vec2{style.popupMinimumWidth, style.itemHeight};
                layout.setWidth(size.x);
                layout.setHeight(size.y);
                m_layoutDirty = false;
            }

            void reconnect() {
                m_connection.disconnect();
                if (m_model == nullptr || m_state == nullptr) {
                    return;
                }
                m_connection = m_model->changed().connect([this](const auto &) {
                    m_layoutDirty = true;
                    if (m_state != nullptr && m_state->contains(m_id)) {
                        m_state->invalidate(m_id,
                                            WidgetInvalidation::layout |
                                                WidgetInvalidation::paint);
                    }
                });
            }

            std::shared_ptr<MenuModel> m_model;
            MenuId m_menu;
            std::optional<UIMenuStyle> m_style;
            std::function<void()> m_dismissed;
            float m_maximumHeight = 520.f;
            MenuModel::ChangedSignal::Connection m_connection;
            WidgetTree *m_state = nullptr;
            WidgetId m_id;
            MenuItemId m_hot;
            size_t m_hotDepth = 0;
            MenuItemId m_pressed;
            std::vector<MenuItemId> m_openPath;
            mutable std::vector<ContextPanel> m_panels;
            mutable std::vector<float> m_panelScroll;
            bool m_layoutDirty = true;
        };
    } // namespace

    Autocomplete::Autocomplete(std::shared_ptr<TextEditModel> model,
                               AutocompleteProvider provider,
                               Changed changed,
                               Submitted submitted,
                               Completed completed,
                               AutocompleteOptions options)
        : m_model(model != nullptr ? std::move(model)
                                   : std::make_shared<TextEditModel>()),
          m_provider(std::move(provider)),
          m_changed(std::move(changed)),
          m_submitted(std::move(submitted)),
          m_completed(std::move(completed)),
          m_options(std::move(options)),
          m_session(std::make_shared<Detail::AutocompleteSession>()) {
    }

    std::string_view Autocomplete::typeName() const noexcept {
        return "Autocomplete";
    }

    WidgetTraits Autocomplete::traits() const noexcept {
        return {
            .focusable = false, .hitTestVisible = false, .clipChildren = false};
    }

    void Autocomplete::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        const auto &style =
            m_options.textBox.style.value_or(context.state.theme().textBox);
        context.layout.setWidth(style.minimumSize.x);
        context.layout.setHeight(style.minimumSize.y);
        m_textBox = context.state.addWidget(
            std::make_unique<TextBox>(
                m_model, m_changed, m_submitted, m_options.textBox),
            m_id);
        if (auto *layout = context.state.getLayout(m_textBox)) {
            layout->setWidthPercent(1.f);
            layout->setHeightPercent(1.f);
        }
        m_connection =
            m_model->changed().connect([this](const TextEditChange &change) {
                if (!change.textChanged || m_suppressRefresh ||
                    m_state == nullptr || !m_state->contains(m_id))
                    return;
                m_dismissedQuery.clear();
                if (m_state->getFocusedWidget() == m_textBox)
                    refresh();
                else
                    static_cast<void>(close());
            });
    }

    void Autocomplete::onUnmount(WidgetTree &, WidgetId) {
        static_cast<void>(close());
        m_connection.disconnect();
        m_state = nullptr;
        m_id = {};
        m_textBox = {};
    }

    void Autocomplete::update(WidgetUpdateContext &context) {
        const bool openNow = isOpen();
        if (m_popupWasOpen && !openNow) {
            m_dismissedQuery = m_model->text();
            m_popup = {};
        }
        m_popupWasOpen = openNow;

        const bool focused = context.state.getFocusedWidget() == m_textBox;
        if (focused && !m_wasFocused) {
            m_dismissedQuery.clear();
            refresh();
        } else if (!focused && m_wasFocused) {
            static_cast<void>(close());
        }
        m_wasFocused = focused;
    }

    void Autocomplete::arrange(WidgetArrangeContext &context) {
        for (const auto child : context.children()) {
            static_cast<void>(context.setChildBounds(child, context.bounds));
            static_cast<void>(context.setChildVisible(child, true));
        }
    }

    UIEventReply Autocomplete::onEvent(WidgetEventContext &context,
                                       const UIEvent &event) {
        if (context.phase != UIEventPhase::capture ||
            context.target != m_textBox)
            return {};
        const auto *key = event.getIf<Input::KeyEvent>();
        if (key == nullptr ||
            (key->action != KeyAction::press && key->action != KeyAction::hold))
            return {};
        if (key->key == KeyCode::escape && isOpen()) {
            static_cast<void>(close());
            return {.handled = true, .stopPropagation = true};
        }
        if (!isOpen()) {
            if (key->key == KeyCode::arrowDown) {
                m_dismissedQuery.clear();
                refresh();
                if (isOpen())
                    return {.handled = true, .stopPropagation = true};
            }
            return {};
        }
        if (key->key == KeyCode::arrowDown) {
            moveSelection(1);
        } else if (key->key == KeyCode::arrowUp) {
            moveSelection(-1);
        } else if (key->key == KeyCode::enter &&
                   m_session->selected != Detail::AutocompleteSession::npos) {
            complete(m_session->selected);
        } else {
            return {};
        }
        context.state.invalidate(m_id, WidgetInvalidation::paint);
        return {.handled = true,
                .stopPropagation = true,
                .invalidate = WidgetInvalidation::paint};
    }

    std::shared_ptr<TextEditModel> Autocomplete::model() const noexcept {
        return m_model;
    }

    WidgetId Autocomplete::textBoxId() const noexcept {
        return m_textBox;
    }

    void Autocomplete::setProvider(AutocompleteProvider provider) {
        m_provider = std::move(provider);
        m_dismissedQuery.clear();
        refresh();
    }

    void Autocomplete::refresh() {
        if (m_state == nullptr || !m_textBox || !m_provider ||
            m_state->getFocusedWidget() != m_textBox ||
            graphemeCount(m_model->text()) < m_options.minimumCharacters ||
            m_model->text() == m_dismissedQuery) {
            static_cast<void>(close());
            return;
        }

        auto items = m_provider(m_model->text());
        if (items.size() > m_options.maximumItems)
            items.resize(m_options.maximumItems);
        std::vector<AutocompleteItemId> used;
        used.reserve(items.size());
        for (auto &item : items) {
            while (!item.id ||
                   std::find(used.begin(), used.end(), item.id) != used.end())
                item.id = AutocompleteItemId::generate();
            used.push_back(item.id);
        }
        if (items.empty()) {
            static_cast<void>(close());
            return;
        }

        const auto previousId = m_session->selected < m_session->items.size()
                                    ? m_session->items[m_session->selected].id
                                    : AutocompleteItemId{};
        m_session->items = std::move(items);
        m_session->selected = Detail::AutocompleteSession::npos;
        if (previousId) {
            const auto found = std::find_if(m_session->items.begin(),
                                            m_session->items.end(),
                                            [previousId](const auto &item) {
                                                return item.id == previousId;
                                            });
            if (found != m_session->items.end() && found->enabled)
                m_session->selected =
                    static_cast<size_t>(found - m_session->items.begin());
        }
        if (m_session->selected == Detail::AutocompleteSession::npos &&
            m_options.selectFirst) {
            const auto found =
                std::find_if(m_session->items.begin(),
                             m_session->items.end(),
                             [](const auto &item) { return item.enabled; });
            if (found != m_session->items.end())
                m_session->selected =
                    static_cast<size_t>(found - m_session->items.begin());
        }
        ++m_session->revision;
        if (isOpen()) {
            const auto content = m_popup.content();
            if (content)
                m_state->invalidate(content.id(),
                                    WidgetInvalidation::layout |
                                        WidgetInvalidation::paint);
        } else {
            open();
        }
    }

    bool Autocomplete::close() {
        const bool closed = m_popup.close();
        m_popup = {};
        m_popupWasOpen = false;
        m_dismissedQuery = m_model != nullptr ? m_model->text() : std::string{};
        return closed;
    }

    bool Autocomplete::isOpen() const noexcept {
        return static_cast<bool>(m_popup);
    }

    void Autocomplete::open() {
        if (isOpen() || m_state == nullptr || !m_textBox ||
            m_session->items.empty())
            return;
        auto *host = m_state->popupHost();
        if (host == nullptr)
            return;
        const auto &style =
            m_options.popupStyle.value_or(m_state->theme().dropdown);
        AnchoredPopupOptions popup{
            .anchor = PopupAnchor::forWidget(m_textBox),
            .preferredSide = PopupSide::bottom,
            .alignment = PopupAlignment::start,
            .gap = 3.f,
            .minimumSize = {m_state->getBounds(m_textBox).size.x,
                            style.itemHeight},
            .maximumSize = {0.f, m_options.popupMaximumHeight},
            .matchAnchorWidth = true,
            .passThroughAnchor = true,
            .focus = {.trapFocus = false,
                      .autoFocus = false,
                      .restoreFocus = true},
        };
        popup.dismissOnEscape = false;
        popup.content.padding =
            Core::Style::Padding{m_state->theme().menus.popupPadding};
        const auto session = m_session;
        const auto styleOverride = m_options.popupStyle;
        const float maximumHeight = m_options.popupMaximumHeight;
        m_popup = host->open(
            std::move(popup),
            [this, session, styleOverride, maximumHeight](UIComposer &content) {
                content.emplace<AutocompleteList>(
                    session,
                    [this](size_t index) { complete(index); },
                    styleOverride,
                    maximumHeight);
            });
        m_popupWasOpen = isOpen();
    }

    void Autocomplete::complete(size_t index) {
        if (index >= m_session->items.size() ||
            !m_session->items[index].enabled)
            return;
        const AutocompleteItem item = m_session->items[index];
        const std::string replacement =
            item.replacement.empty() ? item.label : item.replacement;
        const std::string previous = m_model->text();
        m_suppressRefresh = true;
        static_cast<void>(m_model->selectAll());
        static_cast<void>(m_model->insertText(replacement));
        m_suppressRefresh = false;
        static_cast<void>(close());
        if (m_state != nullptr && m_state->contains(m_textBox))
            static_cast<void>(m_state->setFocus(m_textBox));
        if (previous != m_model->text() && m_changed)
            m_changed(m_model->text());
        if (m_completed)
            m_completed(item);
    }

    void Autocomplete::moveSelection(int direction) {
        if (m_session->items.empty() || direction == 0)
            return;
        size_t index = m_session->selected;
        if (index == Detail::AutocompleteSession::npos)
            index = direction > 0 ? m_session->items.size() - 1 : 0;
        for (size_t attempt = 0; attempt < m_session->items.size(); ++attempt) {
            index = direction > 0 ? (index + 1) % m_session->items.size()
                                  : (index + m_session->items.size() - 1) %
                                        m_session->items.size();
            if (m_session->items[index].enabled) {
                m_session->selected = index;
                return;
            }
        }
    }

    size_t Autocomplete::graphemeCount(std::string_view text) const noexcept {
        size_t count = 0;
        for (size_t offset = 0; offset < text.size(); ++count)
            offset = TextEditModel::nextGraphemeBoundary(text, offset);
        return count;
    }

    Dropdown::Dropdown(std::shared_ptr<DropdownModel> model,
                       Changed changed,
                       DropdownOptions options)
        : m_model(model != nullptr ? std::move(model)
                                   : std::make_shared<DropdownModel>()),
          m_changed(std::move(changed)),
          m_options(std::move(options)) {
    }

    std::string_view Dropdown::typeName() const noexcept {
        return "Dropdown";
    }

    WidgetTraits Dropdown::traits() const noexcept {
        return {.focusable = true, .hitTestVisible = true};
    }

    void Dropdown::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        reconnectModel();
        WidgetLayoutContext layoutContext{.state = context.state,
                                          .id = context.id,
                                          .layout = context.layout,
                                          .themeChanged = true};
        updateLayout(layoutContext);
    }

    void Dropdown::onUnmount(WidgetTree &, WidgetId) {
        static_cast<void>(close());
        m_connection.disconnect();
        m_state = nullptr;
        m_id = {};
    }

    void Dropdown::updateLayout(WidgetLayoutContext &context) {
        if (!m_options.autoSize ||
            (!m_intrinsicSizeDirty && !context.themeChanged)) {
            return;
        }
        const auto &style =
            m_options.style.value_or(context.state.theme().dropdown);
        float textWidth =
            estimatedTextSize(m_options.placeholder, style.placeholder).x;
        for (const auto &item : m_model->items()) {
            textWidth = std::max(
                textWidth, estimatedTextSize(item.label, style.field.text).x);
        }
        const float width =
            std::max(style.minimumWidth,
                     textWidth + style.field.contentPadding.x * 2.f +
                         style.chevronSize * 2.f);
        context.layout.setWidth(width);
        context.layout.setHeight(style.field.minimumSize.y);
        m_intrinsicSizeDirty = false;
    }

    void Dropdown::paint(WidgetPaintContext &context) const {
        const auto &style =
            m_options.style.value_or(context.state.theme().dropdown);
        const UIBoxStyle *box = &style.field.normal;
        if (!context.enabled) {
            box = &style.field.disabled;
        } else if (m_pressable.isPressed()) {
            box = &style.field.pressed;
        } else if (m_pressable.isHovered() || context.hovered) {
            box = &style.field.hovered;
        } else if (context.focused || isOpen()) {
            box = &style.field.focused;
        }
        context.painter.drawBox(
            makeBox(context.bounds, *box, context.pickingId));

        const auto *selected = m_model->find(m_model->selection());
        const std::string_view text =
            selected != nullptr ? std::string_view{selected->label}
                                : std::string_view{m_options.placeholder};
        const auto &textStyle =
            selected != nullptr ? style.field.text : style.placeholder;
        const float rightReserve = style.field.contentPadding.x +
                                   std::max(16.f, style.chevronSize * 2.f);
        const float left =
            context.bounds.topLeft().x + style.field.contentPadding.x;
        const float width =
            std::max(0.f, context.bounds.bottomRight().x - rightReserve - left);
        context.painter.drawText(
            text,
            {.bounds = {.center = {left + width * 0.5f,
                                   context.bounds.center.y},
                        .size = {width, context.bounds.size.y}},
             .fontSize = textStyle.fontSize,
             .color =
                 context.enabled ? textStyle.color : style.field.disabledText,
             .horizontal = HorizontalTextAlignment::start,
             .vertical = VerticalTextAlignment::center,
             .zIndex = kContentZ,
             .letterSpacing = textStyle.letterSpacing,
             .pickingId = context.pickingId});
        const WidgetBounds chevron{
            .center = {context.bounds.bottomRight().x - rightReserve * 0.5f,
                       context.bounds.center.y},
            .size = {rightReserve, context.bounds.size.y},
        };
        context.painter.drawIcon(
            isOpen() ? Icons::FontAwesomeIcons::FA_CHEVRON_UP
                     : Icons::FontAwesomeIcons::FA_CHEVRON_DOWN,
            {.glyph = {.bounds = chevron,
                       .fontSize = style.chevronSize,
                       .color = context.enabled ? style.chevron
                                                : style.field.disabledText,
                       .horizontal = HorizontalTextAlignment::center,
                       .vertical = VerticalTextAlignment::center,
                       .zIndex = kContentZ,
                       .pickingId = context.pickingId}});
    }

    CursorIcon Dropdown::cursor(const WidgetCursorContext &) const noexcept {
        return CursorIcon::pointer;
    }

    UIEventReply Dropdown::onEvent(WidgetEventContext &context,
                                   const UIEvent &event) {
        auto result = m_pressable.handle(context, event);
        if (result.activated && context.enabled) {
            static_cast<void>(isOpen() ? close() : open());
            result.reply.invalidate |= WidgetInvalidation::paint;
        }
        if (const auto *key = event.getIf<Input::KeyEvent>();
            key != nullptr && context.phase == UIEventPhase::target &&
            context.focused && context.enabled &&
            key->action == KeyAction::press) {
            int direction = 0;
            if (key->key == KeyCode::arrowDown) {
                direction = 1;
            } else if (key->key == KeyCode::arrowUp) {
                direction = -1;
            }
            if (direction != 0 && !isOpen()) {
                const auto next =
                    m_model->nextEnabled(m_model->selection(), direction);
                if (next && select(next) && m_changed) {
                    m_changed(next);
                }
                result.reply.handled = true;
                result.reply.stopPropagation = true;
            } else if (key->key == KeyCode::escape && isOpen()) {
                static_cast<void>(close());
                result.reply.handled = true;
                result.reply.stopPropagation = true;
            }
        }
        return result.reply;
    }

    std::shared_ptr<DropdownModel> Dropdown::model() const noexcept {
        return m_model;
    }

    DropdownItemId Dropdown::selection() const noexcept {
        return m_model->selection();
    }

    bool Dropdown::select(DropdownItemId item) {
        return m_model->select(item);
    }

    bool Dropdown::isOpen() const noexcept {
        return static_cast<bool>(m_popup);
    }

    bool Dropdown::open() {
        if (isOpen() || m_state == nullptr || !m_id) {
            return isOpen();
        }
        auto *host = m_state->popupHost();
        if (host == nullptr) {
            return false;
        }
        const auto &style = m_options.style.value_or(m_state->theme().dropdown);
        AnchoredPopupOptions popup{
            .anchor = PopupAnchor::forWidget(m_id),
            .preferredSide = PopupSide::bottom,
            .alignment = PopupAlignment::start,
            .gap = 3.f,
            .minimumSize = {m_state->getBounds(m_id).size.x, style.itemHeight},
            .maximumSize = {0.f, style.popupMaximumHeight},
            .matchAnchorWidth = true,
            .passThroughAnchor = true,
            .focus = {.trapFocus = true,
                      .autoFocus = true,
                      .restoreFocus = true},
        };
        popup.content.padding =
            Core::Style::Padding{m_state->theme().menus.popupPadding};
        const auto model = m_model;
        const auto changed = m_changed;
        const auto styleOverride = m_options.style;
        auto closeHandle = std::make_shared<PopupHandle>();
        m_popup = host->open(
            std::move(popup),
            [model, changed, styleOverride, closeHandle](UIComposer &content) {
                content.emplace<DropdownList>(
                    model,
                    [model, changed, closeHandle](DropdownItemId item) {
                        if (model->select(item) && changed) {
                            changed(item);
                        }
                        static_cast<void>(closeHandle->close());
                    },
                    styleOverride);
            });
        *closeHandle = m_popup;
        return isOpen();
    }

    bool Dropdown::close() {
        const bool closed = m_popup.close();
        m_popup = {};
        return closed;
    }

    void Dropdown::reconnectModel() {
        m_connection.disconnect();
        if (m_model == nullptr || m_state == nullptr) {
            return;
        }
        m_connection = m_model->changed().connect([this](const auto &change) {
            if (change.kind != DropdownChangeKind::selection) {
                m_intrinsicSizeDirty = true;
            }
            if (m_state != nullptr && m_state->contains(m_id)) {
                m_state->invalidate(m_id,
                                    m_intrinsicSizeDirty
                                        ? WidgetInvalidation::layout |
                                              WidgetInvalidation::paint
                                        : WidgetInvalidation::paint);
            }
        });
    }

    PopupHandle ContextMenu::open(PopupHost &host,
                                  std::shared_ptr<MenuModel> model,
                                  MenuId menu,
                                  PopupAnchor anchor,
                                  ContextMenuOptions options) {
        if (model == nullptr || model->findMenu(menu) == nullptr) {
            return {};
        }
        options.popup.anchor = std::move(anchor);
        const auto originalPanel = options.popup.style.value_or(UIBoxStyle{});
        options.popup.style = transparentBox(originalPanel);
        const auto style = options.style;
        const float maximumHeight = options.maximumHeight;
        auto closeHandle = std::make_shared<PopupHandle>();
        auto opened = host.open(
            std::move(options.popup),
            [model, menu, style, maximumHeight, closeHandle](
                UIComposer &content) {
                content.emplace<ContextMenuWidget>(
                    model,
                    menu,
                    style,
                    [closeHandle] { static_cast<void>(closeHandle->close()); },
                    maximumHeight);
            });
        *closeHandle = opened;
        return opened;
    }

    PopupHandle ContextMenu::open(PopupHost &host,
                                  std::vector<MenuItem> items,
                                  glm::vec2 position,
                                  ContextMenuOptions options) {
        auto model = std::make_shared<MenuModel>();
        const MenuId menu =
            model->addMenu({.name = "Context", .items = std::move(items)});
        return open(host,
                    std::move(model),
                    menu,
                    PopupAnchor::forPoint(position),
                    std::move(options));
    }

    ContextMenuRegion::ContextMenuRegion(std::shared_ptr<MenuModel> model,
                                         MenuId menu,
                                         ContextMenuOptions options)
        : m_model(std::move(model)),
          m_menu(menu),
          m_options(std::move(options)) {
    }

    std::string_view ContextMenuRegion::typeName() const noexcept {
        return "ContextMenuRegion";
    }

    WidgetTraits ContextMenuRegion::traits() const noexcept {
        return {
            .focusable = false, .hitTestVisible = true, .clipChildren = false};
    }

    void ContextMenuRegion::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
    }

    void ContextMenuRegion::onUnmount(WidgetTree &, WidgetId) {
        static_cast<void>(close());
        m_state = nullptr;
        m_id = {};
    }

    void ContextMenuRegion::arrange(WidgetArrangeContext &context) {
        const auto children = context.children();
        for (size_t index = 0; index < children.size(); ++index) {
            static_cast<void>(
                context.setChildVisible(children[index], index == 0));
        }
    }

    UIEventReply ContextMenuRegion::onEvent(WidgetEventContext &context,
                                            const UIEvent &event) {
        const auto *button = event.getIf<Input::MouseButtonEvent>();
        if (button == nullptr || button->button != MouseButton::right ||
            button->action != MouseButtonAction::press ||
            !context.hasPointerPosition || !context.pointerInside() ||
            (context.phase != UIEventPhase::target &&
             context.phase != UIEventPhase::bubble)) {
            return {};
        }
        if (m_model == nullptr || m_state == nullptr ||
            m_model->findMenu(m_menu) == nullptr) {
            return {};
        }
        static_cast<void>(close());
        if (auto *host = m_state->popupHost(); host != nullptr) {
            m_popup = ContextMenu::open(
                *host,
                m_model,
                m_menu,
                PopupAnchor::forPoint(context.pointerPosition),
                m_options);
        }
        return {.handled = static_cast<bool>(m_popup),
                .stopPropagation = static_cast<bool>(m_popup)};
    }

    bool ContextMenuRegion::isOpen() const noexcept {
        return static_cast<bool>(m_popup);
    }

    bool ContextMenuRegion::close() {
        const bool closed = m_popup.close();
        m_popup = {};
        return closed;
    }

    Tooltip::Tooltip(std::string text, TooltipOptions options)
        : m_text(std::move(text)),
          m_options(std::move(options)) {
    }

    std::string_view Tooltip::typeName() const noexcept {
        return "Tooltip";
    }

    WidgetTraits Tooltip::traits() const noexcept {
        return {
            .focusable = false, .hitTestVisible = false, .clipChildren = false};
    }

    void Tooltip::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
    }

    void Tooltip::onUnmount(WidgetTree &, WidgetId) {
        static_cast<void>(hide());
        m_state = nullptr;
        m_id = {};
    }

    void Tooltip::update(WidgetUpdateContext &context) {
        const auto pointer = context.state.getPointerPosition();
        const bool inside =
            pointer.has_value() &&
            context.state.getBounds(context.id).contains(*pointer);
        if (!inside) {
            m_hoverElapsedMs = 0.0;
            m_wasInside = false;
            static_cast<void>(hide());
            return;
        }
        if (!m_wasInside) {
            m_hoverElapsedMs = 0.0;
            m_wasInside = true;
        }
        m_hoverElapsedMs += std::max(0.0, context.deltaTime.count());
        const auto &style =
            m_options.style.value_or(context.state.theme().tooltip);
        const float delay =
            std::max(0.f, m_options.delayMs.value_or(style.delayMs));
        if (!isOpen() && m_hoverElapsedMs >= delay) {
            static_cast<void>(showNow());
        }
    }

    void Tooltip::arrange(WidgetArrangeContext &context) {
        const auto children = context.children();
        for (size_t index = 0; index < children.size(); ++index) {
            static_cast<void>(
                context.setChildVisible(children[index], index == 0));
        }
    }

    const std::string &Tooltip::text() const noexcept {
        return m_text;
    }

    void Tooltip::setText(std::string text) {
        if (m_text != text) {
            m_text = std::move(text);
            if (isOpen()) {
                static_cast<void>(hide());
                static_cast<void>(showNow());
            }
        }
    }

    bool Tooltip::showNow() {
        if (isOpen() || m_state == nullptr || !m_id || m_text.empty()) {
            return isOpen();
        }
        auto *host = m_state->popupHost();
        if (host == nullptr) {
            return false;
        }
        const auto style = m_options.style.value_or(m_state->theme().tooltip);
        AnchoredPopupOptions popup{
            .anchor = PopupAnchor::forWidget(m_id),
            .preferredSide = m_options.side,
            .alignment = m_options.alignment,
            .gap = m_options.gap,
            .maximumSize = {style.maximumWidth, 0.f},
            .dismissOnOutsidePress = false,
            .dismissOnEscape = false,
            .closeWhenAnchorGone = true,
            .interactive = false,
            .focus = {.trapFocus = false,
                      .autoFocus = false,
                      .restoreFocus = false},
            .style = style.panel,
        };
        popup.content.padding = Core::Style::Padding::fromSymmetric(
            style.padding.x, style.padding.y);
        const std::string text = m_text;
        m_popup =
            host->open(std::move(popup), [text, style](UIComposer &content) {
                content.label(text,
                              {.style = style.text,
                               .horizontal = HorizontalTextAlignment::start,
                               .vertical = VerticalTextAlignment::center,
                               .autoSize = true});
            });
        return isOpen();
    }

    bool Tooltip::hide() {
        const bool closed = m_popup.close();
        m_popup = {};
        return closed;
    }

    bool Tooltip::isOpen() const noexcept {
        return static_cast<bool>(m_popup);
    }

} // namespace Bess::UI
