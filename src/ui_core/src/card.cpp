#include "controls/card.h"

#include "controls/basic_widgets.h"
#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace Bess::UI {
    namespace {
        constexpr float kCardChromeZ = 0.f;

        [[nodiscard]] float nonNegative(float value) noexcept {
            return std::isfinite(value) ? std::max(0.f, value) : 0.f;
        }

        [[nodiscard]] Core::Style::Padding
        normalizedPadding(Core::Style::Padding padding) noexcept {
            return {
                nonNegative(padding.top),
                nonNegative(padding.right),
                nonNegative(padding.bottom),
                nonNegative(padding.left),
            };
        }

        [[nodiscard]] BoxPaint
        boxPaint(WidgetBounds bounds, const UIBoxStyle &style, PickingId id) {
            return {
                .bounds = bounds,
                .color = style.background,
                .borderColor = style.border,
                .cornerRadius = style.cornerRadius,
                .borderThickness = style.borderThickness,
                .shadow = style.shadow,
                .zIndex = kCardChromeZ,
                .pickingId = id,
            };
        }
    } // namespace

    Card::Card(CardOptions options) : m_options(std::move(options)) {
        m_options.padding = normalizedPadding(m_options.padding);
        m_options.gap = nonNegative(m_options.gap);
    }

    std::string_view Card::typeName() const noexcept {
        return "Card";
    }

    WidgetTraits Card::traits() const noexcept {
        return {
            .focusable = false,
            .hitTestVisible = m_options.hitTestVisible,
            .clipChildren = m_options.clipChildren,
        };
    }

    void Card::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        if (m_options.stretchWidth) {
            context.layout.setWidthPercent(1.f);
        }
        if (m_options.stretchHeight) {
            context.layout.setHeightPercent(1.f);
        }

        FlexContainerOptions content{
            .direction = m_options.direction,
            .mainAxisAlignment = m_options.mainAxisAlignment,
            .crossAxisAlignment = m_options.crossAxisAlignment,
            .padding = m_options.padding,
            .gap = m_options.gap,
            .stretchWidth = true,
            .stretchHeight = m_options.stretchHeight,
            .clipChildren = false,
            .hitTestVisible = false,
        };
        m_contentRoot = context.state.addWidget(
            std::make_unique<FlexContainer>(std::move(content)), context.id);
        if (!m_contentRoot) {
            m_state = nullptr;
            m_id = {};
            throw std::runtime_error("Card failed to create content host");
        }
        applyContentLayout(context.layout);
    }

    void Card::onUnmount(WidgetTree &, WidgetId) {
        m_contentRoot = {};
        m_id = {};
        m_state = nullptr;
    }

    void Card::updateLayout(WidgetLayoutContext &context) {
        applyContentLayout(context.layout);
        if (m_state != nullptr && m_contentRoot &&
            m_state->contains(m_contentRoot)) {
            if (auto *layout = m_state->getLayout(m_contentRoot);
                layout != nullptr) {
                layout->setDirection(m_options.direction);
                layout->setMainAxisAlignment(m_options.mainAxisAlignment);
                layout->setCrossAxisAlignment(m_options.crossAxisAlignment);
                layout->setPadding(m_options.padding);
                layout->setGap(m_options.gap);
            }
        }
    }

    void Card::paint(WidgetPaintContext &context) const {
        const auto &style =
            m_options.style.value_or(context.state.theme().card);
        context.painter.drawBox(
            boxPaint(context.bounds, style, context.pickingId));
    }

    WidgetId Card::contentRoot() const noexcept {
        return m_contentRoot;
    }

    const CardOptions &Card::options() const noexcept {
        return m_options;
    }

    void Card::setStyle(std::optional<UIBoxStyle> style) {
        m_options.style = std::move(style);
    }

    void Card::setPadding(Core::Style::Padding padding) {
        m_options.padding = normalizedPadding(padding);
    }

    void Card::applyContentLayout(LayoutNode &layout) const {
        if (m_options.stretchWidth) {
            layout.setWidthPercent(1.f);
        }
        if (m_options.stretchHeight) {
            layout.setHeightPercent(1.f);
        }
    }

} // namespace Bess::UI
