#include "controls/tab_bar.h"

#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>

namespace Bess::UI {
    namespace {
        BoxPaint makeBox(WidgetBounds bounds,
                         const UIBoxStyle &style,
                         PickingId id,
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
    } // namespace

    std::vector<TabStripRegion>
    TabStripLayout::calculate(WidgetBounds bounds,
                              size_t tabCount,
                              const TabStripMetrics &metrics,
                              float scrollOffset) {
        std::vector<TabStripRegion> result;
        if (tabCount == 0 || bounds.empty()) {
            return result;
        }

        const glm::vec2 stripPadding = glm::min(
            glm::max(metrics.stripPadding, glm::vec2{0.f}), bounds.size * 0.5f);
        const float innerWidth =
            std::max(0.f, bounds.size.x - stripPadding.x * 2.f);
        const float maximumGap =
            tabCount > 1 ? innerWidth / (static_cast<float>(tabCount - 1) * 2.f)
                         : 0.f;
        const float gap = std::min(std::max(0.f, metrics.gap), maximumGap);
        const float totalGap = gap * static_cast<float>(tabCount - 1);
        const float available = std::max(0.f, innerWidth - totalGap);
        const float equalWidth = available / static_cast<float>(tabCount);
        const float minWidth = std::max(0.f, metrics.minimumWidth);
        const float maxWidth = std::max(minWidth, metrics.maximumWidth);
        // Tabs compress below their preferred minimum before overflowing. A
        // future scrolling policy can pass a non-zero offset without changing
        // hit-testing or the model.
        const float width =
            equalWidth < minWidth ? equalWidth : std::min(equalWidth, maxWidth);
        const float height =
            std::min(std::max(0.f, bounds.size.y - stripPadding.y * 2.f),
                     std::max(0.f, metrics.height - stripPadding.y * 2.f));
        const float left =
            bounds.topLeft().x + stripPadding.x - std::max(0.f, scrollOffset);
        const float top = bounds.topLeft().y + stripPadding.y;

        result.reserve(tabCount);
        for (size_t i = 0; i < tabCount; ++i) {
            WidgetBounds tab{
                .center = {left + width * (static_cast<float>(i) + 0.5f) +
                               gap * static_cast<float>(i),
                           top + height * 0.5f},
                .size = {width, height},
            };
            const float padding = std::min(
                std::max(0.f, metrics.horizontalPadding), width * 0.5f);
            result.push_back({
                .index = i,
                .bounds = tab,
                .labelBounds =
                    {
                        .center = tab.center,
                        .size = {std::max(0.f, width - padding * 2.f), height},
                    },
            });
        }
        return result;
    }

    std::optional<size_t>
    TabStripLayout::hitTest(std::span<const TabStripRegion> regions,
                            glm::vec2 position) noexcept {
        for (const auto &region : regions) {
            if (region.bounds.contains(position)) {
                return region.index;
            }
        }
        return std::nullopt;
    }

    TabStripRegion
    TabStripLayout::withTrailingAction(TabStripRegion region,
                                       float actionSize,
                                       float gap,
                                       float trailingPadding) noexcept {
        const auto nonNegativeFinite = [](float value) {
            return std::isfinite(value) ? std::max(0.f, value) : 0.f;
        };
        const float size = std::min({nonNegativeFinite(actionSize),
                                     region.bounds.size.x,
                                     region.bounds.size.y});
        if (size <= 0.f) {
            return region;
        }

        const auto tabTopLeft = region.bounds.topLeft();
        const auto tabBottomRight = region.bounds.bottomRight();
        const float padding =
            std::min(nonNegativeFinite(trailingPadding),
                     std::max(0.f, region.bounds.size.x - size));
        const float right = tabBottomRight.x - padding;
        region.trailingActionBounds = {
            .center = {right - size * 0.5f, region.bounds.center.y},
            .size = {size, size},
        };

        const float labelLeft =
            std::max(tabTopLeft.x, region.labelBounds.topLeft().x);
        const float labelRight =
            std::max(labelLeft,
                     std::min(region.labelBounds.bottomRight().x,
                              region.trailingActionBounds.topLeft().x -
                                  nonNegativeFinite(gap)));
        region.labelBounds.center.x = (labelLeft + labelRight) * 0.5f;
        region.labelBounds.size.x = labelRight - labelLeft;
        return region;
    }

    TabBar::TabBar(std::shared_ptr<TabModel> model, TabBarOptions options)
        : m_model(model != nullptr ? std::move(model)
                                   : std::make_shared<TabModel>()),
          m_options(std::move(options)) {
    }

    std::string_view TabBar::typeName() const noexcept {
        return "TabBar";
    }

    WidgetTraits TabBar::traits() const noexcept {
        return {
            .focusable = true, .hitTestVisible = true, .clipChildren = true};
    }

    void TabBar::onMount(WidgetMountContext &context) {
        const auto &resolved = style(context.state);
        context.layout.setWidthPercent(1.f);
        context.layout.setHeight(resolved.height);
        m_modelConnection = m_model->changed().connect(
            [state = &context.state, id = context.id](const TabChange &) {
                state->invalidate(id, WidgetInvalidation::paint);
            });
    }

    void TabBar::onUnmount(WidgetTree &, WidgetId) {
        m_modelConnection.disconnect();
    }

    void TabBar::updateLayout(WidgetLayoutContext &context) {
        if (!context.themeChanged || m_options.style.has_value()) {
            return;
        }
        context.layout.setHeight(context.state.theme().tabs.height);
    }

    void TabBar::paint(WidgetPaintContext &context) const {
        const auto &resolved = style(context.state);
        context.painter.drawBox(
            makeBox(context.bounds, resolved.strip, context.pickingId));

        const auto tabRegions = regions(context.bounds, context.state);
        const auto items = m_model->items();
        for (size_t i = 0; i < tabRegions.size() && i < items.size(); ++i) {
            const auto &item = items[i];
            const UIBoxStyle *box = &resolved.normal;
            if (m_pressedTab == item.id && m_pressable.isPressed()) {
                box = &resolved.pressed;
            } else if (m_model->active() == item.id) {
                box = &resolved.active;
            } else if (m_hoveredTab == item.id) {
                box = &resolved.hovered;
            }

            PickingId pickingId = context.pickingId;
            pickingId.info = static_cast<uint32_t>(i + 1);
            context.painter.drawBox(
                makeBox(tabRegions[i].bounds, *box, pickingId, 0.001f));

            auto color = m_model->active() == item.id ? resolved.text.color
                                                      : resolved.inactiveText;
            if (!item.enabled) {
                color.a *= 0.45f;
            }
            context.painter.drawText(
                item.title,
                {
                    .bounds = tabRegions[i].labelBounds,
                    .fontSize = resolved.text.fontSize,
                    .color = color,
                    .horizontal = HorizontalTextAlignment::start,
                    .vertical = VerticalTextAlignment::center,
                    .zIndex = 0.002f,
                    .letterSpacing = resolved.text.letterSpacing,
                    .pickingId = pickingId,
                });
        }
    }

    UIEventReply TabBar::onEvent(WidgetEventContext &context,
                                 const UIEvent &event) {
        if (context.phase != UIEventPhase::target) {
            return {};
        }

        if (const auto *key = event.getIf<Input::KeyEvent>();
            key != nullptr && context.focused &&
            key->action == KeyAction::press) {
            TabId next;
            if (key->key == KeyCode::arrowRight) {
                next = m_model->nextEnabled(m_model->active(), 1);
            } else if (key->key == KeyCode::arrowLeft) {
                next = m_model->nextEnabled(m_model->active(), -1);
            } else if (key->key == KeyCode::home) {
                for (const auto &item : m_model->items()) {
                    if (item.enabled) {
                        next = item.id;
                        break;
                    }
                }
            } else if (key->key == KeyCode::end) {
                const auto items = m_model->items();
                for (auto it = items.rbegin(); it != items.rend(); ++it) {
                    if (it->enabled) {
                        next = it->id;
                        break;
                    }
                }
            }
            if (next) {
                m_model->activate(next);
                return {.handled = true,
                        .stopPropagation = true,
                        .invalidate = WidgetInvalidation::paint};
            }
        }

        const bool pointerEvent = event.is<Input::MouseMoveEvent>() ||
                                  event.is<Input::MouseButtonEvent>() ||
                                  event.is<UIPointerCrossingEvent>();
        if (pointerEvent) {
            const TabId hit = context.hasPointerPosition
                                  ? tabAt(context.bounds,
                                          context.state,
                                          context.pointerPosition)
                                  : TabId{};
            if (event.is<Input::MouseMoveEvent>() ||
                event.is<UIPointerCrossingEvent>()) {
                m_hoveredTab = hit;
            }
            if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                button != nullptr && button->button == MouseButton::left &&
                (button->action == MouseButtonAction::press ||
                 button->action == MouseButtonAction::doubleClick)) {
                if (!hit) {
                    return {};
                }
                m_pressedTab = hit;
            }

            auto result = m_pressable.handle(context, event);
            if (result.activated) {
                const TabId activate =
                    m_pressedTab ? m_pressedTab : m_model->active();
                if (activate &&
                    (hit == activate || !event.is<Input::MouseButtonEvent>())) {
                    m_model->activate(activate);
                }
            }
            if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                button != nullptr &&
                button->action == MouseButtonAction::release) {
                m_pressedTab = {};
            }
            return result.reply;
        }

        auto result = m_pressable.handle(context, event);
        if (result.activated && m_model->active()) {
            m_model->activate(m_model->active());
        }
        return result.reply;
    }

    std::shared_ptr<TabModel> TabBar::model() const noexcept {
        return m_model;
    }

    TabId TabBar::hoveredTab() const noexcept {
        return m_hoveredTab;
    }

    const UITabStyle &TabBar::style(const WidgetTree &state) const {
        return m_options.style ? *m_options.style : state.theme().tabs;
    }

    std::vector<TabStripRegion> TabBar::regions(WidgetBounds bounds,
                                                const WidgetTree &state) const {
        const auto &resolved = style(state);
        return TabStripLayout::calculate(
            bounds, m_model->size(), metrics(resolved));
    }

    TabId TabBar::tabAt(WidgetBounds bounds,
                        const WidgetTree &state,
                        glm::vec2 position) const {
        const auto tabRegions = regions(bounds, state);
        const auto index = TabStripLayout::hitTest(tabRegions, position);
        const auto items = m_model->items();
        if (!index.has_value() || *index >= items.size() ||
            !items[*index].enabled) {
            return {};
        }
        return items[*index].id;
    }
} // namespace Bess::UI
