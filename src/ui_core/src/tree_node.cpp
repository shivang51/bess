#include "controls/tree_node.h"

#include "bess_core/ui/icons/font_awesome_icons.h"
#include "layout.h"
#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace Bess::UI {
    namespace {
        constexpr float kHeaderChromeZ = 0.001f;
        constexpr float kHeaderContentZ = 0.002f;

        [[nodiscard]] float finiteNonNegative(float value) noexcept {
            return std::isfinite(value) ? std::max(0.f, value) : 0.f;
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
                .zIndex = kHeaderChromeZ,
                .pickingId = id,
            };
        }

        [[nodiscard]] WidgetBounds
        fromEdges(float left, float right, WidgetBounds row) noexcept {
            const float width = std::max(0.f, right - left);
            return {
                .center = {left + width * 0.5f, row.center.y},
                .size = {width, row.size.y},
            };
        }
    } // namespace

    TreeNode::TreeNode(std::string label,
                       TreeNodeOptions options,
                       ExpandedChanged expandedChanged)
        : m_label(std::move(label)),
          m_options(std::move(options)),
          m_expandedChanged(std::move(expandedChanged)) {
        m_options.headerHeight = finiteNonNegative(m_options.headerHeight);
        m_options.indentation = finiteNonNegative(m_options.indentation);
        m_options.horizontalPadding =
            finiteNonNegative(m_options.horizontalPadding);
        m_options.disclosureSlotWidth =
            finiteNonNegative(m_options.disclosureSlotWidth);
        m_options.iconSlotWidth = finiteNonNegative(m_options.iconSlotWidth);
        m_options.contentGap = finiteNonNegative(m_options.contentGap);
    }

    std::string_view TreeNode::typeName() const noexcept {
        return "TreeNode";
    }

    WidgetTraits TreeNode::traits() const noexcept {
        return {
            .focusable = m_options.collapsible,
            .hitTestVisible = m_options.collapsible,
            .clipChildren = false,
        };
    }

    void TreeNode::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;

        context.layout.setDirection(LayoutDirection::vertical);
        context.layout.setMainAxisAlignment(LayoutAlignment::start);
        context.layout.setCrossAxisAlignment(LayoutAlignment::start);
        context.layout.setPadding(contentPadding());
        if (m_options.stretchWidth) {
            context.layout.setWidthPercent(1.f);
        }

        FlexContainerOptions content{
            .direction = LayoutDirection::vertical,
            .mainAxisAlignment = LayoutAlignment::start,
            .crossAxisAlignment = LayoutAlignment::start,
            .stretchWidth = true,
            .stretchHeight = false,
            .clipChildren = false,
            .hitTestVisible = false,
        };
        m_contentRoot = context.state.addWidget(
            std::make_unique<FlexContainer>(std::move(content)), context.id);
        if (!m_contentRoot) {
            m_state = nullptr;
            m_id = {};
            throw std::runtime_error("TreeNode failed to create content host");
        }
        syncContentVisibility();
    }

    void TreeNode::onUnmount(WidgetTree &, WidgetId) {
        m_pressable.reset();
        m_contentRoot = {};
        m_id = {};
        m_state = nullptr;
    }

    void TreeNode::updateLayout(WidgetLayoutContext &context) {
        context.layout.setPadding(contentPadding());
        syncContentVisibility();
    }

    void TreeNode::paint(WidgetPaintContext &context) const {
        const auto bounds = headerBounds(context.bounds);
        if (bounds.empty()) {
            return;
        }
        const ScopedUIClip headerClip{context.painter, bounds};

        const auto &theme = context.state.theme();
        const UIBoxStyle *background = &theme.menus.barItem;
        if (context.enabled && m_pressable.isPressed()) {
            background = &theme.menus.itemPressed;
        } else if (context.enabled &&
                   (m_pressable.isHovered() || context.hovered)) {
            background = &theme.menus.itemHovered;
        } else if (context.enabled && context.focused) {
            background = &theme.button.focused;
        }
        context.painter.drawBox(
            boxPaint(bounds, *background, context.pickingId));

        const float left = bounds.topLeft().x + m_options.horizontalPadding;
        const float right = std::max(
            left, bounds.bottomRight().x - m_options.horizontalPadding);
        float cursor = left;
        const auto textColor =
            context.enabled ? theme.tabs.text.color : theme.button.disabledText;
        const auto iconColor =
            context.enabled ? theme.menus.iconColor : theme.button.disabledText;

        if (m_options.collapsible && m_options.disclosureSlotWidth > 0.f) {
            const float disclosureRight =
                std::min(right, cursor + m_options.disclosureSlotWidth);
            context.painter.drawIcon(
                m_options.expanded ? Icons::FontAwesomeIcons::FA_CHEVRON_DOWN
                                   : Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT,
                {.glyph = {
                     .bounds = fromEdges(cursor, disclosureRight, bounds),
                     .fontSize = std::max(8.f, theme.tabs.text.fontSize - 2.f),
                     .color = iconColor,
                     .horizontal = HorizontalTextAlignment::center,
                     .vertical = VerticalTextAlignment::center,
                     .zIndex = kHeaderContentZ,
                     .pickingId = context.pickingId,
                 }});
            cursor = disclosureRight;
        }

        if (!m_options.icon.empty() && m_options.iconSlotWidth > 0.f) {
            const float iconRight =
                std::min(right, cursor + m_options.iconSlotWidth);
            context.painter.drawIcon(
                m_options.icon,
                {.glyph = {
                     .bounds = fromEdges(cursor, iconRight, bounds),
                     .fontSize = theme.tabs.text.fontSize,
                     .color = iconColor,
                     .horizontal = HorizontalTextAlignment::center,
                     .vertical = VerticalTextAlignment::center,
                     .zIndex = kHeaderContentZ,
                     .pickingId = context.pickingId,
                 }});
            cursor = iconRight;
        }

        context.painter.drawText(
            m_label,
            {
                .bounds = fromEdges(cursor, right, bounds),
                .fontSize = theme.tabs.text.fontSize,
                .color = textColor,
                .horizontal = HorizontalTextAlignment::start,
                .vertical = VerticalTextAlignment::center,
                .zIndex = kHeaderContentZ,
                .letterSpacing = theme.tabs.text.letterSpacing,
                .pickingId = context.pickingId,
            });
    }

    bool TreeNode::hitTest(WidgetBounds bounds,
                           glm::vec2 position) const noexcept {
        return headerBounds(bounds).contains(position);
    }

    CursorIcon TreeNode::cursor(const WidgetCursorContext &) const noexcept {
        return m_options.collapsible ? CursorIcon::pointer : CursorIcon::arrow;
    }

    UIEventReply TreeNode::onEvent(WidgetEventContext &context,
                                   const UIEvent &event) {
        if (context.phase != UIEventPhase::target || !m_options.collapsible) {
            return {};
        }

        if (const auto *key = event.getIf<Input::KeyEvent>();
            key != nullptr && context.focused && context.enabled &&
            key->action == KeyAction::press) {
            if (key->key == KeyCode::arrowRight && !m_options.expanded) {
                static_cast<void>(changeExpanded(true, true));
                return {.handled = true,
                        .stopPropagation = true,
                        .invalidate = WidgetInvalidation::layout |
                                      WidgetInvalidation::paint};
            }
            if (key->key == KeyCode::arrowLeft && m_options.expanded) {
                static_cast<void>(changeExpanded(false, true));
                return {.handled = true,
                        .stopPropagation = true,
                        .invalidate = WidgetInvalidation::layout |
                                      WidgetInvalidation::paint};
            }
        }

        auto headerContext = context;
        headerContext.bounds = headerBounds(context.bounds);
        auto result = m_pressable.handle(headerContext, event);
        if (result.activated && m_options.collapsible) {
            static_cast<void>(changeExpanded(!m_options.expanded, true));
            result.reply.invalidate |=
                WidgetInvalidation::layout | WidgetInvalidation::paint;
        }
        return result.reply;
    }

    const std::string &TreeNode::label() const noexcept {
        return m_label;
    }

    void TreeNode::setLabel(std::string label) {
        m_label = std::move(label);
    }

    const std::string &TreeNode::icon() const noexcept {
        return m_options.icon;
    }

    void TreeNode::setIcon(std::string icon) {
        m_options.icon = std::move(icon);
    }

    bool TreeNode::isExpanded() const noexcept {
        return m_options.expanded;
    }

    bool TreeNode::setExpanded(bool expanded) {
        return changeExpanded(expanded, true);
    }

    bool TreeNode::toggle() {
        return changeExpanded(!m_options.expanded, true);
    }

    void TreeNode::setExpandedChanged(ExpandedChanged changed) {
        m_expandedChanged = std::move(changed);
    }

    WidgetId TreeNode::contentRoot() const noexcept {
        return m_contentRoot;
    }

    WidgetBounds TreeNode::headerBounds(WidgetBounds bounds) const noexcept {
        const float height = std::min(finiteNonNegative(m_options.headerHeight),
                                      finiteNonNegative(bounds.size.y));
        return {
            .center = {bounds.center.x, bounds.topLeft().y + height * 0.5f},
            .size = {finiteNonNegative(bounds.size.x), height},
        };
    }

    float TreeNode::labelTextStart() const noexcept {
        float cursor = finiteNonNegative(m_options.horizontalPadding);
        if (m_options.collapsible && m_options.disclosureSlotWidth > 0.f) {
            cursor += finiteNonNegative(m_options.disclosureSlotWidth);
        }
        if (!m_options.icon.empty() && m_options.iconSlotWidth > 0.f) {
            cursor += finiteNonNegative(m_options.iconSlotWidth);
        }
        return cursor;
    }

    Core::Style::Padding TreeNode::contentPadding() const noexcept {
        // Children start under the label text so nested disclosure glyphs and
        // titles form a clean vertical column (common tree/file-browser UX).
        const float left = labelTextStart() +
                           finiteNonNegative(m_options.indentation);
        return {
            finiteNonNegative(m_options.headerHeight) +
                finiteNonNegative(m_options.contentGap),
            0.f,
            0.f,
            left,
        };
    }

    bool TreeNode::changeExpanded(bool expanded, bool notify) {
        if (m_options.expanded == expanded) {
            return false;
        }
        m_options.expanded = expanded;
        // Visibility/focus notifications may synchronously remove this node
        // when the setter is used directly rather than through WidgetRef.
        // Snapshot everything needed before crossing that callback boundary.
        WidgetTree *const state = m_state;
        const WidgetId id = m_id;
        auto changed = notify ? m_expandedChanged : ExpandedChanged{};
        syncContentVisibility();
        if (state != nullptr && id && state->contains(id)) {
            state->invalidate(
                id, WidgetInvalidation::layout | WidgetInvalidation::paint);
        }
        if (changed) {
            changed(expanded);
        }
        return true;
    }

    void TreeNode::syncContentVisibility() {
        if (m_state == nullptr || !m_contentRoot ||
            !m_state->contains(m_contentRoot)) {
            return;
        }
        static_cast<void>(m_state->setVisibility(
            m_contentRoot,
            m_options.expanded ? WidgetVisibility::visible
                               : WidgetVisibility::collapsed));
    }

} // namespace Bess::UI
