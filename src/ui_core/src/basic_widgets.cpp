#include "controls/basic_widgets.h"

#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>

namespace Bess::UI {
    namespace {
        // Interactive chrome must sit above its containing surface. Leaving
        // both at the same depth makes overlapping opaque primitives compete
        // for the same depth value, so the container can obscure the control
        // depending on how the GPU schedules the batch.
        constexpr float kInteractiveChromeZ = 0.001f;
        constexpr float kInteractiveContentZ = 0.002f;
        // Reserve enough depth for each child's local chrome/content layers.
        // The offset is arranged state, so explicit LayoutNode Z values remain
        // untouched and are added on top of this deterministic sibling order.
        constexpr float kStackChildLayerStride = 0.01f;

        BoxPaint boxPaint(WidgetBounds bounds,
                          const UIBoxStyle &style,
                          PickingId pickingId,
                          float zIndex = 0.f) {
            return {
                .bounds = bounds,
                .color = style.background,
                .borderColor = style.border,
                .cornerRadius = style.cornerRadius,
                .borderThickness = style.borderThickness,
                .shadow = style.shadow,
                .zIndex = zIndex,
                .pickingId = pickingId,
            };
        }

        glm::vec2 estimatedTextSize(std::string_view text,
                                    const UITextStyle &style) {
            return {
                std::max(
                    1.f,
                    static_cast<float>(text.size()) * style.fontSize * 0.6f +
                        style.letterSpacing * static_cast<float>(text.size())),
                std::max(1.f, style.fontSize * 1.25f),
            };
        }

        UITextStyle resolvedLabelStyle(const LabelOptions &options,
                                       const UITheme &theme) {
            auto style = options.style.value_or(theme.label);
            if (options.fontSize.has_value()) {
                style.fontSize = std::max(1.f, *options.fontSize);
            }
            return style;
        }

        float nonNegativeFinite(float value) noexcept {
            return std::isfinite(value) ? std::max(0.f, value) : 0.f;
        }

        glm::vec2 nonNegativeFinite(glm::vec2 value) noexcept {
            return {
                nonNegativeFinite(value.x),
                nonNegativeFinite(value.y),
            };
        }

        bool isHorizontal(LayoutDirection direction) noexcept {
            return direction == LayoutDirection::horizontal ||
                   direction == LayoutDirection::horizontalReverse;
        }

        Core::Style::Padding
        normalizedInsets(const Core::Style::Padding &insets) noexcept {
            return {
                nonNegativeFinite(insets.top),
                nonNegativeFinite(insets.right),
                nonNegativeFinite(insets.bottom),
                nonNegativeFinite(insets.left),
            };
        }

        WidgetBounds insetBounds(WidgetBounds bounds,
                                 const Core::Style::Padding &rawInsets) {
            auto insets = normalizedInsets(rawInsets);
            const auto clampPair =
                [](float &leading, float &trailing, float available) {
                    const float total = leading + trailing;
                    if (total > available && total > 0.f) {
                        const float scale = available / total;
                        leading *= scale;
                        trailing *= scale;
                    }
                };
            clampPair(insets.left, insets.right, bounds.size.x);
            clampPair(insets.top, insets.bottom, bounds.size.y);

            const glm::vec2 topLeft =
                bounds.topLeft() + glm::vec2{insets.left, insets.top};
            const glm::vec2 size =
                glm::max(bounds.size - glm::vec2{insets.left + insets.right,
                                                 insets.top + insets.bottom},
                         glm::vec2{0.f});
            return {.center = topLeft + size * 0.5f, .size = size};
        }

        float alignedCenter(float slotStart,
                            float slotExtent,
                            float childExtent,
                            StackAlignment alignment) noexcept {
            switch (alignment) {
            case StackAlignment::start:
                return slotStart + childExtent * 0.5f;
            case StackAlignment::end:
                return slotStart + slotExtent - childExtent * 0.5f;
            case StackAlignment::center:
            case StackAlignment::stretch:
                return slotStart + slotExtent * 0.5f;
            }
            return slotStart + slotExtent * 0.5f;
        }
    } // namespace

    FlexContainer::FlexContainer(FlexContainerOptions options)
        : m_options(std::move(options)) {
    }

    std::string_view FlexContainer::typeName() const noexcept {
        return "FlexContainer";
    }

    WidgetTraits FlexContainer::traits() const noexcept {
        return {
            .focusable = false,
            .hitTestVisible = m_options.hitTestVisible,
            .clipChildren = m_options.clipChildren,
        };
    }

    void FlexContainer::onMount(WidgetMountContext &context) {
        context.layout.setDirection(m_options.direction);
        context.layout.setMainAxisAlignment(m_options.mainAxisAlignment);
        context.layout.setCrossAxisAlignment(m_options.crossAxisAlignment);
        context.layout.setPadding(m_options.padding);
        context.layout.setGap(m_options.gap);
        if (m_options.stretchWidth) {
            context.layout.setWidthPercent(1.f);
        }
        if (m_options.stretchHeight) {
            context.layout.setHeightPercent(1.f);
        }
    }

    StackContainer::StackContainer(StackContainerOptions options)
        : m_options(std::move(options)) {
    }

    std::string_view StackContainer::typeName() const noexcept {
        return "StackContainer";
    }

    WidgetTraits StackContainer::traits() const noexcept {
        return {
            .focusable = false,
            .hitTestVisible = m_options.hitTestVisible,
            .clipChildren = m_options.clipChildren,
        };
    }

    void StackContainer::onMount(WidgetMountContext &context) {
        context.layout.setPadding(normalizedInsets(m_options.padding));
        if (m_options.stretchWidth) {
            context.layout.setWidthPercent(1.f);
        }
        if (m_options.stretchHeight) {
            context.layout.setHeightPercent(1.f);
        }
    }

    void StackContainer::updateLayout(WidgetLayoutContext &context) {
        for (const WidgetId child : context.state.getChildren(context.id)) {
            auto *childLayout = context.state.getLayout(child);
            if (childLayout == nullptr) {
                continue;
            }
            childLayout->setPosMode(PosMode::absolute);
            childLayout->setPos({0.f, 0.f});
            childLayout->setPosUnit(Unit::pixel);
        }
    }

    void StackContainer::arrange(WidgetArrangeContext &context) {
        const auto *containerLayout = context.state.getLayout(context.id);
        const WidgetBounds content = insetBounds(
            context.bounds,
            containerLayout != nullptr ? containerLayout->getPadding()
                                       : Core::Style::Padding{});

        size_t layer = 0;
        for (const auto child : context.children()) {
            const auto *childLayout = context.state.getLayout(child);
            const WidgetBounds slot =
                insetBounds(content,
                            childLayout != nullptr ? childLayout->getMargin()
                                                   : Core::Style::Margin{});
            glm::vec2 desired =
                nonNegativeFinite(context.state.getBounds(child).size);
            glm::vec2 minimum{0.f};
            glm::vec2 maximum{-1.f};
            if (childLayout != nullptr) {
                if (childLayout->getWidthMode() == LayoutSizeMode::point) {
                    desired.x = nonNegativeFinite(childLayout->getWidthValue());
                }
                if (childLayout->getHeightMode() == LayoutSizeMode::point) {
                    desired.y =
                        nonNegativeFinite(childLayout->getHeightValue());
                }
                const glm::vec2 configuredMinimum = childLayout->getMinSize();
                if (std::isfinite(configuredMinimum.x) &&
                    configuredMinimum.x >= 0.f) {
                    minimum.x = configuredMinimum.x;
                }
                if (std::isfinite(configuredMinimum.y) &&
                    configuredMinimum.y >= 0.f) {
                    minimum.y = configuredMinimum.y;
                }
                const glm::vec2 configuredMaximum = childLayout->getMaxSize();
                if (std::isfinite(configuredMaximum.x) &&
                    configuredMaximum.x >= 0.f) {
                    maximum.x = configuredMaximum.x;
                }
                if (std::isfinite(configuredMaximum.y) &&
                    configuredMaximum.y >= 0.f) {
                    maximum.y = configuredMaximum.y;
                }
            }

            WidgetBounds childBounds;
            childBounds.size = desired;
            if (m_options.horizontalAlignment == StackAlignment::stretch) {
                childBounds.size.x = slot.size.x;
            }
            if (m_options.verticalAlignment == StackAlignment::stretch) {
                childBounds.size.y = slot.size.y;
            }
            childBounds.size = glm::max(childBounds.size, minimum);
            if (maximum.x >= 0.f) {
                childBounds.size.x = std::min(childBounds.size.x,
                                              std::max(maximum.x, minimum.x));
            }
            if (maximum.y >= 0.f) {
                childBounds.size.y = std::min(childBounds.size.y,
                                              std::max(maximum.y, minimum.y));
            }
            // The stack provides hard at-most constraints. This final clamp
            // keeps pathological child minima inside a finite parent slot.
            childBounds.size = glm::min(childBounds.size, slot.size);
            const glm::vec2 slotStart = slot.topLeft();
            childBounds.center = {
                alignedCenter(slotStart.x,
                              slot.size.x,
                              childBounds.size.x,
                              m_options.horizontalAlignment),
                alignedCenter(slotStart.y,
                              slot.size.y,
                              childBounds.size.y,
                              m_options.verticalAlignment),
            };
            static_cast<void>(context.setChildBounds(child, childBounds));
            static_cast<void>(
                context.setChildVisible(child, !childBounds.empty()));
            static_cast<void>(context.setChildZOffset(
                child, static_cast<float>(layer++) * kStackChildLayerStride));
        }
    }

    StackAlignment StackContainer::horizontalAlignment() const noexcept {
        return m_options.horizontalAlignment;
    }

    StackAlignment StackContainer::verticalAlignment() const noexcept {
        return m_options.verticalAlignment;
    }

    void
    StackContainer::setHorizontalAlignment(StackAlignment alignment) noexcept {
        m_options.horizontalAlignment = alignment;
    }

    void
    StackContainer::setVerticalAlignment(StackAlignment alignment) noexcept {
        m_options.verticalAlignment = alignment;
    }

    void StackContainer::setAlignment(StackAlignment horizontal,
                                      StackAlignment vertical) noexcept {
        m_options.horizontalAlignment = horizontal;
        m_options.verticalAlignment = vertical;
    }

    Surface::Surface(SurfaceOptions options) : m_options(std::move(options)) {
    }

    std::string_view Surface::typeName() const noexcept {
        return "Surface";
    }

    WidgetTraits Surface::traits() const noexcept {
        return {
            .focusable = false,
            .hitTestVisible = m_options.hitTestVisible,
            .clipChildren = m_options.clipChildren,
        };
    }

    void Surface::paint(WidgetPaintContext &context) const {
        const auto &style =
            m_options.style.value_or(context.state.theme().surface);
        context.painter.drawBox(
            boxPaint(context.bounds, style, context.pickingId));
    }

    Label::Label(std::string text, LabelOptions options)
        : m_text(std::move(text)),
          m_options(std::move(options)) {
    }

    std::string_view Label::typeName() const noexcept {
        return "Label";
    }

    WidgetTraits Label::traits() const noexcept {
        return {.focusable = false, .hitTestVisible = false};
    }

    void Label::onMount(WidgetMountContext &context) {
        if (!m_options.autoSize) {
            return;
        }
        const auto style = resolvedLabelStyle(m_options, context.state.theme());
        const auto size = estimatedTextSize(m_text, style);
        context.layout.setWidth(size.x);
        context.layout.setHeight(size.y);
        m_intrinsicSizeDirty = false;
    }

    void Label::updateLayout(WidgetLayoutContext &context) {
        if (!m_options.autoSize ||
            (!m_intrinsicSizeDirty && !context.themeChanged)) {
            return;
        }
        const auto style = resolvedLabelStyle(m_options, context.state.theme());
        const auto size = estimatedTextSize(m_text, style);
        context.layout.setWidth(size.x);
        context.layout.setHeight(size.y);
        m_intrinsicSizeDirty = false;
    }

    void Label::paint(WidgetPaintContext &context) const {
        const auto style = resolvedLabelStyle(m_options, context.state.theme());
        context.painter.drawText(m_text,
                                 {
                                     .bounds = context.bounds,
                                     .fontSize = style.fontSize,
                                     .color = style.color,
                                     .horizontal = m_options.horizontal,
                                     .vertical = m_options.vertical,
                                     .letterSpacing = style.letterSpacing,
                                     .pickingId = context.pickingId,
                                 });
    }

    const std::string &Label::text() const noexcept {
        return m_text;
    }

    void Label::setText(std::string text) {
        if (m_text == text) {
            return;
        }
        m_text = std::move(text);
        m_intrinsicSizeDirty = true;
    }

    Button::Button(std::string label,
                   Activated activated,
                   ButtonOptions options)
        : m_label(std::move(label)),
          m_activated(std::move(activated)),
          m_options(std::move(options)) {
    }

    std::string_view Button::typeName() const noexcept {
        return "Button";
    }

    WidgetTraits Button::traits() const noexcept {
        return {.focusable = true, .hitTestVisible = true};
    }

    void Button::onMount(WidgetMountContext &context) {
        if (!m_options.autoSize) {
            return;
        }
        const auto &style =
            m_options.style.value_or(context.state.theme().button);
        const auto textSize = estimatedTextSize(m_label, style.text);
        const glm::vec2 size =
            glm::max(style.minimumSize, textSize + style.contentPadding * 2.f);
        context.layout.setWidth(size.x);
        context.layout.setHeight(size.y);
        m_intrinsicSizeDirty = false;
    }

    void Button::updateLayout(WidgetLayoutContext &context) {
        if (!m_options.autoSize ||
            (!m_intrinsicSizeDirty && !context.themeChanged)) {
            return;
        }
        const auto &style =
            m_options.style.value_or(context.state.theme().button);
        const auto textSize = estimatedTextSize(m_label, style.text);
        const glm::vec2 size =
            glm::max(style.minimumSize, textSize + style.contentPadding * 2.f);
        context.layout.setWidth(size.x);
        context.layout.setHeight(size.y);
        m_intrinsicSizeDirty = false;
    }

    void Button::paint(WidgetPaintContext &context) const {
        const auto &style =
            m_options.style.value_or(context.state.theme().button);
        const UIBoxStyle *box = &style.normal;
        if (!context.enabled) {
            box = &style.disabled;
        } else if (m_pressable.isPressed()) {
            box = &style.pressed;
        } else if (m_pressable.isHovered() || context.hovered) {
            box = &style.hovered;
        } else if (context.focused) {
            box = &style.focused;
        }

        context.painter.drawBox(boxPaint(
            context.bounds, *box, context.pickingId, kInteractiveChromeZ));
        const auto textColor =
            context.enabled ? style.text.color : style.disabledText;
        context.painter.drawText(
            m_label,
            {
                .bounds = context.bounds,
                .fontSize = style.text.fontSize,
                .color = textColor,
                .horizontal = HorizontalTextAlignment::center,
                .vertical = VerticalTextAlignment::center,
                .zIndex = kInteractiveContentZ,
                .letterSpacing = style.text.letterSpacing,
                .pickingId = context.pickingId,
            });
    }

    UIEventReply Button::onEvent(WidgetEventContext &context,
                                 const UIEvent &event) {
        auto result = m_pressable.handle(context, event);
        if (result.activated) {
            if (m_activated) {
                m_activated();
            }
            if (m_activatedWithEvent) {
                m_activatedWithEvent(event);
            }
        }
        return result.reply;
    }

    const std::string &Button::label() const noexcept {
        return m_label;
    }

    void Button::setLabel(std::string label) {
        if (m_label == label) {
            return;
        }
        m_label = std::move(label);
        m_intrinsicSizeDirty = true;
    }

    void Button::setActivated(Activated activated) {
        m_activated = std::move(activated);
    }

    void Button::setActivatedWithEvent(ActivatedWithEvent activated) {
        m_activatedWithEvent = std::move(activated);
    }

    bool Button::isPressed() const noexcept {
        return m_pressable.isPressed();
    }

    Spacer::Spacer(SpacerOptions options) : m_options(std::move(options)) {
    }

    std::string_view Spacer::typeName() const noexcept {
        return "Spacer";
    }

    WidgetTraits Spacer::traits() const noexcept {
        return {.focusable = false, .hitTestVisible = false};
    }

    void Spacer::onMount(WidgetMountContext &context) {
        context.layout.setFlexGrow(nonNegativeFinite(m_options.flex));
        context.layout.setFlexShrink(1.f);
        context.layout.setFlexBasis(0.f);
        context.layout.setMinSize(nonNegativeFinite(m_options.minimumSize));
    }

    Gap::Gap(float extent) : m_extent(nonNegativeFinite(extent)) {
    }

    std::string_view Gap::typeName() const noexcept {
        return "Gap";
    }

    WidgetTraits Gap::traits() const noexcept {
        return {.focusable = false, .hitTestVisible = false};
    }

    void Gap::onMount(WidgetMountContext &context) {
        applyLayout(context.state, context.id, context.layout);
    }

    void Gap::updateLayout(WidgetLayoutContext &context) {
        applyLayout(context.state, context.id, context.layout);
    }

    void Gap::applyLayout(WidgetTree &state, WidgetId id, LayoutNode &layout) {
        const auto parent = state.getParent(id);
        const auto *parentLayout = state.getLayout(parent);
        const bool horizontal = parentLayout == nullptr ||
                                isHorizontal(parentLayout->getDirection());
        if (m_horizontal == horizontal) {
            return;
        }
        m_horizontal = horizontal;

        layout.setFlex(0.f, 0.f, m_extent);
        layout.setWidth(horizontal ? m_extent : 0.f);
        layout.setHeight(horizontal ? 0.f : m_extent);
    }
} // namespace Bess::UI
