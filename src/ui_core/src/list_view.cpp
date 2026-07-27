#include "controls/list_view.h"

#include "ui_painter.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Bess::UI {
    namespace {
        constexpr float kListChromeZ = 0.f;

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
                .zIndex = kListChromeZ,
                .pickingId = id,
            };
        }
    } // namespace

    ListView::ListView(ListViewOptions options)
        : m_options(std::move(options)) {
        m_options.padding = normalizedPadding(m_options.padding);
        m_options.gap = nonNegative(m_options.gap);
    }

    std::string_view ListView::typeName() const noexcept {
        return "ListView";
    }

    WidgetTraits ListView::traits() const noexcept {
        return {
            .focusable = false,
            .hitTestVisible = m_options.hitTestVisible,
            .clipChildren = m_options.clipChildren,
        };
    }

    void ListView::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        if (m_options.stretchWidth) {
            context.layout.setWidthPercent(1.f);
        }
        if (m_options.stretchHeight) {
            context.layout.setHeightPercent(1.f);
        }

        ScrollViewOptions scroll{
            .horizontal = m_options.horizontalScroll,
            .vertical = m_options.verticalScroll,
            .clipContent = m_options.clipChildren,
            .style = m_options.scrollStyle,
        };
        m_scrollRoot = context.state.addWidget(
            std::make_unique<ScrollView>(std::move(scroll)), context.id);
        if (!m_scrollRoot) {
            m_state = nullptr;
            m_id = {};
            throw std::runtime_error("ListView failed to create scroll host");
        }

        FlexContainerOptions content{
            .direction = m_options.direction,
            .mainAxisAlignment = m_options.mainAxisAlignment,
            .crossAxisAlignment = m_options.crossAxisAlignment,
            .padding = m_options.padding,
            .gap = m_options.gap,
            .stretchWidth = true,
            .stretchHeight = false,
            .clipChildren = false,
            .hitTestVisible = false,
        };
        m_contentRoot = context.state.addWidget(
            std::make_unique<FlexContainer>(std::move(content)), m_scrollRoot);
        if (!m_contentRoot) {
            static_cast<void>(context.state.removeWidget(m_scrollRoot));
            m_scrollRoot = {};
            m_state = nullptr;
            m_id = {};
            throw std::runtime_error("ListView failed to create content host");
        }
        applyContentLayout();
    }

    void ListView::onUnmount(WidgetTree &, WidgetId) {
        m_contentRoot = {};
        m_scrollRoot = {};
        m_id = {};
        m_state = nullptr;
    }

    void ListView::updateLayout(WidgetLayoutContext &context) {
        if (m_options.stretchWidth) {
            context.layout.setWidthPercent(1.f);
        }
        if (m_options.stretchHeight) {
            context.layout.setHeightPercent(1.f);
        }
        applyContentLayout();
    }

    void ListView::paint(WidgetPaintContext &context) const {
        if (!m_options.style.has_value() &&
            context.state.theme().listView.background.a <= 0.f &&
            context.state.theme().listView.border.a <= 0.f) {
            return;
        }
        const auto &style =
            m_options.style.value_or(context.state.theme().listView);
        context.painter.drawBox(
            boxPaint(context.bounds, style, context.pickingId));
    }

    WidgetId ListView::contentRoot() const noexcept {
        return m_contentRoot;
    }

    WidgetId ListView::scrollRoot() const noexcept {
        return m_scrollRoot;
    }

    std::span<const WidgetId> ListView::items() const noexcept {
        if (m_state == nullptr || !m_contentRoot ||
            !m_state->contains(m_contentRoot)) {
            return {};
        }
        return m_state->getChildren(m_contentRoot);
    }

    size_t ListView::itemCount() const noexcept {
        return items().size();
    }

    WidgetId ListView::addItem(std::unique_ptr<Widget> widget, size_t index) {
        if (m_state == nullptr || !m_contentRoot || widget == nullptr ||
            !m_state->contains(m_contentRoot)) {
            return {};
        }
        return m_state->addWidget(std::move(widget), m_contentRoot, index);
    }

    bool ListView::removeItem(WidgetId id) {
        if (m_state == nullptr || !id || !m_contentRoot) {
            return false;
        }
        if (m_state->getParent(id) != m_contentRoot) {
            return false;
        }
        return m_state->removeWidget(id);
    }

    bool ListView::clearItems() {
        if (m_state == nullptr || !m_contentRoot ||
            !m_state->contains(m_contentRoot)) {
            return false;
        }
        const auto children = m_state->getChildren(m_contentRoot);
        if (children.empty()) {
            return false;
        }
        // Snapshot IDs: removal mutates the child span.
        const std::vector<WidgetId> snapshot(children.begin(), children.end());
        bool removed = false;
        for (const auto child : snapshot) {
            removed = m_state->removeWidget(child) || removed;
        }
        return removed;
    }

    bool ListView::moveItem(WidgetId id, size_t index) {
        if (m_state == nullptr || !id || !m_contentRoot) {
            return false;
        }
        if (m_state->getParent(id) != m_contentRoot) {
            return false;
        }
        return m_state->reparentWidget(id, m_contentRoot, index);
    }

    const ListViewOptions &ListView::options() const noexcept {
        return m_options;
    }

    void ListView::setStyle(std::optional<UIBoxStyle> style) {
        m_options.style = std::move(style);
    }

    void ListView::setGap(float gap) noexcept {
        m_options.gap = nonNegative(gap);
    }

    void ListView::setPadding(Core::Style::Padding padding) {
        m_options.padding = normalizedPadding(padding);
    }

    void ListView::applyContentLayout() const {
        if (m_state == nullptr || !m_contentRoot ||
            !m_state->contains(m_contentRoot)) {
            return;
        }
        auto *layout = m_state->getLayout(m_contentRoot);
        if (layout == nullptr) {
            return;
        }
        layout->setDirection(m_options.direction);
        layout->setMainAxisAlignment(m_options.mainAxisAlignment);
        layout->setCrossAxisAlignment(m_options.crossAxisAlignment);
        layout->setPadding(m_options.padding);
        layout->setGap(m_options.gap);
        // Content sizes to its items so ScrollView can measure overflow.
        layout->setWidthPercent(1.f);
        layout->setHeightFitContent();
    }

} // namespace Bess::UI
