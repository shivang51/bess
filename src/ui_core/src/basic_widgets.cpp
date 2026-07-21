#include "controls/basic_widgets.h"

#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>

namespace Bess::UI {
    namespace {
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
        if (m_options.stretchWidth) {
            context.layout.setWidthStretch();
        }
        if (m_options.stretchHeight) {
            context.layout.setHeightStretch();
        }
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
            m_options.style.value_or(context.state.theme().panel);
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
        const auto &style =
            m_options.style.value_or(context.state.theme().label);
        const auto size = estimatedTextSize(m_text, style);
        context.layout.setWidth(size.x);
        context.layout.setHeight(size.y);
    }

    void Label::paint(WidgetPaintContext &context) const {
        const auto &style =
            m_options.style.value_or(context.state.theme().label);
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
        m_text = std::move(text);
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

        context.painter.drawBox(
            boxPaint(context.bounds, *box, context.pickingId));
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
                .zIndex = 0.001f,
                .letterSpacing = style.text.letterSpacing,
                .pickingId = context.pickingId,
            });
    }

    UIEventReply Button::onEvent(WidgetEventContext &context,
                                 const UIEvent &event) {
        auto result = m_pressable.handle(context, event);
        if (result.activated && m_activated) {
            m_activated();
        }
        return result.reply;
    }

    const std::string &Button::label() const noexcept {
        return m_label;
    }

    void Button::setLabel(std::string label) {
        m_label = std::move(label);
    }

    void Button::setActivated(Activated activated) {
        m_activated = std::move(activated);
    }

    bool Button::isPressed() const noexcept {
        return m_pressable.isPressed();
    }

    std::string_view Spacer::typeName() const noexcept {
        return "Spacer";
    }

    WidgetTraits Spacer::traits() const noexcept {
        return {.focusable = false, .hitTestVisible = false};
    }

    void Spacer::onMount(WidgetMountContext &context) {
        context.layout.setFlexGrow(1.f);
        context.layout.setFlexShrink(1.f);
        context.layout.setFlexBasis(0.f);
    }
} // namespace Bess::UI
