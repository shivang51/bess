#include "controls/value_controls.h"

#include "bess_core/ui/icons/font_awesome_icons.h"
#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Bess::UI {
    namespace {
        constexpr float kChromeZ = 0.001f;
        constexpr float kContentZ = 0.002f;

        float safeNonNegative(float value) noexcept {
            return std::isfinite(value) ? std::max(0.f, value) : 0.f;
        }

        glm::vec2 estimatedTextSize(std::string_view text,
                                    const UITextStyle &style) noexcept {
            const float glyphCount = static_cast<float>(text.size());
            return {std::max(0.f,
                             glyphCount * style.fontSize * 0.6f +
                                 glyphCount * style.letterSpacing),
                    std::max(1.f, style.fontSize * 1.25f)};
        }

        bool
        measurementMatches(const Detail::ControlTextMeasurement &measurement,
                           const UITextStyle &style) noexcept {
            return measurement.valid &&
                   measurement.fontSize == style.fontSize &&
                   measurement.letterSpacing == style.letterSpacing;
        }

        glm::vec2 resolvedTextSize(
            std::string_view text,
            const UITextStyle &style,
            const Detail::ControlTextMeasurement &measurement) noexcept {
            return measurementMatches(measurement, style)
                       ? measurement.size
                       : estimatedTextSize(text, style);
        }

        bool
        updateTextMeasurement(UIPainter &painter,
                              std::string_view text,
                              const UITextStyle &style,
                              Detail::ControlTextMeasurement &measurement) {
            if (measurementMatches(measurement, style)) {
                return false;
            }
            glm::vec2 size =
                painter.measureText(text, style.fontSize, style.letterSpacing);
            if (!std::isfinite(size.x) || !std::isfinite(size.y) ||
                size.x < 0.f || size.y < 0.f) {
                size = estimatedTextSize(text, style);
            }
            measurement = {
                .size = size,
                .fontSize = style.fontSize,
                .letterSpacing = style.letterSpacing,
                .valid = true,
            };
            return true;
        }

        WidgetBounds
        leadingInteractionBounds(WidgetBounds bounds,
                                 glm::vec2 intrinsicSize) noexcept {
            const glm::vec2 size =
                glm::min(glm::max(intrinsicSize, glm::vec2{0.f}), bounds.size);
            return {
                .center = {bounds.topLeft().x + size.x * 0.5f, bounds.center.y},
                .size = size,
            };
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

        WidgetBounds leadingSquare(WidgetBounds bounds, float extent) {
            extent = std::min(safeNonNegative(extent), bounds.size.y);
            return {
                .center = {bounds.topLeft().x + extent * 0.5f, bounds.center.y},
                .size = {extent, extent}};
        }

        WidgetBounds
        trailingLabel(WidgetBounds bounds, float leadingExtent, float gap) {
            const float left = bounds.topLeft().x + leadingExtent + gap;
            const float width = std::max(0.f, bounds.bottomRight().x - left);
            return {.center = {left + width * 0.5f, bounds.center.y},
                    .size = {width, bounds.size.y}};
        }

        const UIBoxStyle &selectionBox(const UISelectionControlStyle &style,
                                       const Pressable &pressable,
                                       bool selected,
                                       bool focused,
                                       bool enabled) {
            if (!enabled) {
                return style.indicatorDisabled;
            }
            if (selected) {
                return style.indicatorSelected;
            }
            if (pressable.isPressed()) {
                return style.indicatorPressed;
            }
            if (pressable.isHovered()) {
                return style.indicatorHovered;
            }
            if (focused) {
                return style.indicatorFocused;
            }
            return style.indicator;
        }

        void paintSelectionLabel(WidgetPaintContext &context,
                                 std::string_view label,
                                 const UISelectionControlStyle &style,
                                 float leadingExtent) {
            if (label.empty()) {
                return;
            }
            context.painter.drawText(
                label,
                {.bounds =
                     trailingLabel(context.bounds, leadingExtent, style.gap),
                 .fontSize = style.text.fontSize,
                 .color =
                     context.enabled ? style.text.color : style.disabledText,
                 .horizontal = HorizontalTextAlignment::start,
                 .vertical = VerticalTextAlignment::center,
                 .zIndex = kContentZ,
                 .letterSpacing = style.text.letterSpacing,
                 .pickingId = context.pickingId});
        }

        WidgetId findRadio(WidgetTree &tree,
                           WidgetId root,
                           const RadioGroupModel *group,
                           RadioId radio) {
            if (const auto *candidate = tree.getWidget<RadioButton>(root);
                candidate != nullptr && candidate->group().get() == group &&
                candidate->radioId() == radio) {
                return root;
            }
            for (const auto child : tree.getChildren(root)) {
                if (const auto found = findRadio(tree, child, group, radio)) {
                    return found;
                }
            }
            return {};
        }

        WidgetId findRadio(WidgetTree &tree,
                           const RadioGroupModel *group,
                           RadioId radio) {
            for (const auto root : tree.getRoots()) {
                if (const auto found = findRadio(tree, root, group, radio)) {
                    return found;
                }
            }
            return {};
        }
    } // namespace

    CheckBox::CheckBox(std::string label,
                       std::shared_ptr<CheckStateModel> model,
                       Changed changed,
                       CheckBoxOptions options)
        : m_label(std::move(label)),
          m_model(model != nullptr ? std::move(model)
                                   : std::make_shared<CheckStateModel>()),
          m_changed(std::move(changed)),
          m_options(std::move(options)) {
    }

    std::string_view CheckBox::typeName() const noexcept {
        return "CheckBox";
    }

    WidgetTraits CheckBox::traits() const noexcept {
        return {.focusable = true, .hitTestVisible = true};
    }

    void CheckBox::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        reconnectModel();
        WidgetLayoutContext layoutContext{.state = context.state,
                                          .id = context.id,
                                          .layout = context.layout,
                                          .themeChanged = true};
        updateLayout(layoutContext);
    }

    void CheckBox::onUnmount(WidgetTree &, WidgetId) {
        m_connection.disconnect();
        m_state = nullptr;
        m_id = {};
    }

    void CheckBox::updateLayout(WidgetLayoutContext &context) {
        if (!m_options.autoSize ||
            (!m_intrinsicSizeDirty && !context.themeChanged)) {
            return;
        }
        const auto &style =
            m_options.style.value_or(context.state.theme().checkbox);
        const auto text =
            resolvedTextSize(m_label, style.text, m_labelMeasurement);
        context.layout.setWidth(style.indicatorSize +
                                (m_label.empty() ? 0.f : style.gap + text.x));
        context.layout.setHeight(
            std::max({style.minimumHeight, style.indicatorSize, text.y}));
        m_intrinsicSizeDirty = false;
    }

    void CheckBox::paint(WidgetPaintContext &context) const {
        const auto &style =
            m_options.style.value_or(context.state.theme().checkbox);
        if (updateTextMeasurement(
                context.painter, m_label, style.text, m_labelMeasurement) &&
            m_options.autoSize && m_state != nullptr &&
            m_state->contains(m_id)) {
            m_intrinsicSizeDirty = true;
            m_state->invalidate(m_id, WidgetInvalidation::layout);
        }
        const WidgetBounds indicator =
            leadingSquare(context.bounds, style.indicatorSize);
        const bool selected = value() != CheckState::unchecked;
        UIBoxStyle resolvedIndicator = selectionBox(
            style, m_pressable, selected, context.focused, context.enabled);
        if (selected && context.focused && context.enabled) {
            resolvedIndicator.border = style.indicatorFocused.border;
            resolvedIndicator.borderThickness =
                style.indicatorFocused.borderThickness;
        }
        context.painter.drawBox(
            makeBox(indicator, resolvedIndicator, context.pickingId));
        if (selected) {
            const auto icon = value() == CheckState::mixed
                                  ? Icons::FontAwesomeIcons::FA_MINUS
                                  : Icons::FontAwesomeIcons::FA_CHECK;
            context.painter.drawIcon(
                icon,
                {.glyph = {.bounds = indicator,
                           .fontSize = style.markSize,
                           .color = context.enabled ? style.mark
                                                    : style.disabledMark,
                           .horizontal = HorizontalTextAlignment::center,
                           .vertical = VerticalTextAlignment::center,
                           .zIndex = kContentZ,
                           .pickingId = context.pickingId}});
        }
        paintSelectionLabel(context, m_label, style, style.indicatorSize);
    }

    WidgetBounds
    CheckBox::interactionBounds(WidgetBounds bounds) const noexcept {
        const UISelectionControlStyle *style =
            m_options.style.has_value()
                ? &*m_options.style
                : (m_state != nullptr ? &m_state->theme().checkbox : nullptr);
        if (style == nullptr) {
            return bounds;
        }
        const auto text =
            resolvedTextSize(m_label, style->text, m_labelMeasurement);
        return leadingInteractionBounds(
            bounds,
            {style->indicatorSize +
                 (m_label.empty() ? 0.f : style->gap + text.x),
             std::max({style->minimumHeight, style->indicatorSize, text.y})});
    }

    bool CheckBox::hitTest(WidgetBounds bounds,
                           glm::vec2 position) const noexcept {
        return interactionBounds(bounds).contains(position);
    }

    CursorIcon CheckBox::cursor(const WidgetCursorContext &) const noexcept {
        return CursorIcon::pointer;
    }

    UIEventReply CheckBox::onEvent(WidgetEventContext &context,
                                   const UIEvent &event) {
        WidgetEventContext pressContext = context;
        pressContext.bounds = interactionBounds(context.bounds);
        auto result = m_pressable.handle(pressContext, event);
        if (result.activated && context.enabled) {
            const bool changed = m_model->toggle(m_options.cycleMixed);
            if (changed && m_changed) {
                m_changed(m_model->value());
            }
        }
        return result.reply;
    }

    CheckState CheckBox::value() const noexcept {
        return m_model->value();
    }

    bool CheckBox::setValue(CheckState value) {
        return m_model->setValue(value);
    }

    const std::string &CheckBox::label() const noexcept {
        return m_label;
    }

    void CheckBox::setLabel(std::string label) {
        if (m_label != label) {
            m_label = std::move(label);
            m_intrinsicSizeDirty = true;
            m_labelMeasurement.valid = false;
            if (m_state != nullptr && m_state->contains(m_id)) {
                m_state->invalidate(m_id,
                                    WidgetInvalidation::layout |
                                        WidgetInvalidation::paint);
            }
        }
    }

    std::shared_ptr<CheckStateModel> CheckBox::model() const noexcept {
        return m_model;
    }

    void CheckBox::reconnectModel() {
        m_connection.disconnect();
        if (m_model == nullptr || m_state == nullptr) {
            return;
        }
        m_connection = m_model->changed().connect([this](const auto &) {
            if (m_state != nullptr && m_state->contains(m_id)) {
                m_state->invalidate(m_id, WidgetInvalidation::paint);
            }
        });
    }

    ToggleSwitch::ToggleSwitch(std::string label,
                               std::shared_ptr<BoolModel> model,
                               Changed changed,
                               ToggleSwitchOptions options)
        : m_label(std::move(label)),
          m_model(model != nullptr ? std::move(model)
                                   : std::make_shared<BoolModel>()),
          m_changed(std::move(changed)),
          m_options(std::move(options)) {
    }

    std::string_view ToggleSwitch::typeName() const noexcept {
        return "ToggleSwitch";
    }

    WidgetTraits ToggleSwitch::traits() const noexcept {
        return {.focusable = true, .hitTestVisible = true};
    }

    void ToggleSwitch::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        reconnectModel();
        WidgetLayoutContext layoutContext{.state = context.state,
                                          .id = context.id,
                                          .layout = context.layout,
                                          .themeChanged = true};
        updateLayout(layoutContext);
    }

    void ToggleSwitch::onUnmount(WidgetTree &, WidgetId) {
        m_connection.disconnect();
        m_state = nullptr;
        m_id = {};
    }

    void ToggleSwitch::updateLayout(WidgetLayoutContext &context) {
        if (!m_options.autoSize ||
            (!m_intrinsicSizeDirty && !context.themeChanged)) {
            return;
        }
        const auto &style =
            m_options.style.value_or(context.state.theme().toggle);
        const auto &labelStyle = context.state.theme().checkbox;
        const auto text =
            resolvedTextSize(m_label, labelStyle.text, m_labelMeasurement);
        context.layout.setWidth(
            style.size.x + (m_label.empty() ? 0.f : labelStyle.gap + text.x));
        context.layout.setHeight(
            std::max({style.size.y, labelStyle.minimumHeight, text.y}));
        m_intrinsicSizeDirty = false;
    }

    void ToggleSwitch::paint(WidgetPaintContext &context) const {
        const auto &style =
            m_options.style.value_or(context.state.theme().toggle);
        const auto &labelStyle = context.state.theme().checkbox;
        if (updateTextMeasurement(context.painter,
                                  m_label,
                                  labelStyle.text,
                                  m_labelMeasurement) &&
            m_options.autoSize && m_state != nullptr &&
            m_state->contains(m_id)) {
            m_intrinsicSizeDirty = true;
            m_state->invalidate(m_id, WidgetInvalidation::layout);
        }
        WidgetBounds track{
            .center = {context.bounds.topLeft().x + style.size.x * 0.5f,
                       context.bounds.center.y},
            .size = glm::min(style.size, context.bounds.size),
        };
        const UIBoxStyle *trackStyle = &style.track;
        if (!context.enabled) {
            trackStyle = &style.trackDisabled;
        } else if (value()) {
            trackStyle = &style.trackSelected;
        } else if (m_pressable.isPressed()) {
            trackStyle = &style.trackPressed;
        } else if (m_pressable.isHovered()) {
            trackStyle = &style.trackHovered;
        }
        UIBoxStyle resolvedTrack = *trackStyle;
        if (context.focused && context.enabled) {
            resolvedTrack.border = labelStyle.indicatorFocused.border;
            resolvedTrack.borderThickness =
                labelStyle.indicatorFocused.borderThickness;
        }
        context.painter.drawBox(
            makeBox(track, resolvedTrack, context.pickingId));

        const float inset =
            std::clamp(safeNonNegative(style.inset), 0.f, track.size.y * 0.5f);
        const float thumbExtent = std::max(0.f, track.size.y - inset * 2.f);
        const float left = track.topLeft().x + inset + thumbExtent * 0.5f;
        const float right = track.bottomRight().x - inset - thumbExtent * 0.5f;
        WidgetBounds thumb{
            .center = {value() ? right : left, track.center.y},
            .size = {thumbExtent, thumbExtent},
        };
        context.painter.drawBox(
            makeBox(thumb,
                    value() ? style.thumbSelected : style.thumb,
                    context.pickingId,
                    kContentZ));
        paintSelectionLabel(context, m_label, labelStyle, style.size.x);
    }

    WidgetBounds
    ToggleSwitch::interactionBounds(WidgetBounds bounds) const noexcept {
        const UIToggleStyle *style =
            m_options.style.has_value()
                ? &*m_options.style
                : (m_state != nullptr ? &m_state->theme().toggle : nullptr);
        const UISelectionControlStyle *labelStyle =
            m_state != nullptr ? &m_state->theme().checkbox : nullptr;
        if (style == nullptr || labelStyle == nullptr) {
            return bounds;
        }
        const auto text =
            resolvedTextSize(m_label, labelStyle->text, m_labelMeasurement);
        return leadingInteractionBounds(
            bounds,
            {style->size.x + (m_label.empty() ? 0.f : labelStyle->gap + text.x),
             std::max({style->size.y, labelStyle->minimumHeight, text.y})});
    }

    bool ToggleSwitch::hitTest(WidgetBounds bounds,
                               glm::vec2 position) const noexcept {
        return interactionBounds(bounds).contains(position);
    }

    CursorIcon
    ToggleSwitch::cursor(const WidgetCursorContext &) const noexcept {
        return CursorIcon::pointer;
    }

    UIEventReply ToggleSwitch::onEvent(WidgetEventContext &context,
                                       const UIEvent &event) {
        WidgetEventContext pressContext = context;
        pressContext.bounds = interactionBounds(context.bounds);
        auto result = m_pressable.handle(pressContext, event);
        if (result.activated && context.enabled) {
            const bool changed = m_model->toggle();
            if (changed && m_changed) {
                m_changed(m_model->value());
            }
        }
        return result.reply;
    }

    bool ToggleSwitch::value() const noexcept {
        return m_model->value();
    }

    bool ToggleSwitch::setValue(bool value) {
        return m_model->setValue(value);
    }

    const std::string &ToggleSwitch::label() const noexcept {
        return m_label;
    }

    void ToggleSwitch::setLabel(std::string label) {
        if (m_label != label) {
            m_label = std::move(label);
            m_intrinsicSizeDirty = true;
            m_labelMeasurement.valid = false;
            if (m_state != nullptr && m_state->contains(m_id)) {
                m_state->invalidate(m_id,
                                    WidgetInvalidation::layout |
                                        WidgetInvalidation::paint);
            }
        }
    }

    std::shared_ptr<BoolModel> ToggleSwitch::model() const noexcept {
        return m_model;
    }

    void ToggleSwitch::reconnectModel() {
        m_connection.disconnect();
        if (m_model == nullptr || m_state == nullptr) {
            return;
        }
        m_connection = m_model->changed().connect([this](const auto &) {
            if (m_state != nullptr && m_state->contains(m_id)) {
                m_state->invalidate(m_id, WidgetInvalidation::paint);
            }
        });
    }

    RadioButton::RadioButton(std::string label,
                             std::shared_ptr<RadioGroupModel> group,
                             RadioId id,
                             Selected selected,
                             RadioButtonOptions options)
        : m_label(std::move(label)),
          m_group(group != nullptr ? std::move(group)
                                   : std::make_shared<RadioGroupModel>()),
          m_radioId(id ? id : RadioId::generate()),
          m_selected(std::move(selected)),
          m_options(std::move(options)) {
    }

    std::string_view RadioButton::typeName() const noexcept {
        return "RadioButton";
    }

    WidgetTraits RadioButton::traits() const noexcept {
        return {.focusable = true, .hitTestVisible = true};
    }

    void RadioButton::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        while (!m_group->registerOption(m_radioId)) {
            m_radioId = RadioId::generate();
        }
        reconnectGroup();
        WidgetLayoutContext layoutContext{.state = context.state,
                                          .id = context.id,
                                          .layout = context.layout,
                                          .themeChanged = true};
        updateLayout(layoutContext);
    }

    void RadioButton::onUnmount(WidgetTree &, WidgetId) {
        m_connection.disconnect();
        static_cast<void>(m_group->unregisterOption(m_radioId));
        m_state = nullptr;
        m_id = {};
    }

    void RadioButton::updateLayout(WidgetLayoutContext &context) {
        if (!m_options.autoSize ||
            (!m_intrinsicSizeDirty && !context.themeChanged)) {
            return;
        }
        const auto &style =
            m_options.style.value_or(context.state.theme().radio);
        const auto text =
            resolvedTextSize(m_label, style.text, m_labelMeasurement);
        context.layout.setWidth(style.indicatorSize +
                                (m_label.empty() ? 0.f : style.gap + text.x));
        context.layout.setHeight(
            std::max({style.minimumHeight, style.indicatorSize, text.y}));
        m_intrinsicSizeDirty = false;
    }

    void RadioButton::paint(WidgetPaintContext &context) const {
        const auto &style =
            m_options.style.value_or(context.state.theme().radio);
        if (updateTextMeasurement(
                context.painter, m_label, style.text, m_labelMeasurement) &&
            m_options.autoSize && m_state != nullptr &&
            m_state->contains(m_id)) {
            m_intrinsicSizeDirty = true;
            m_state->invalidate(m_id, WidgetInvalidation::layout);
        }
        const WidgetBounds indicator =
            leadingSquare(context.bounds, style.indicatorSize);
        UIBoxStyle resolvedIndicator = selectionBox(
            style, m_pressable, selected(), context.focused, context.enabled);
        if (selected() && context.focused && context.enabled) {
            resolvedIndicator.border = style.indicatorFocused.border;
            resolvedIndicator.borderThickness =
                style.indicatorFocused.borderThickness;
        }
        context.painter.drawBox(
            makeBox(indicator, resolvedIndicator, context.pickingId));
        if (selected()) {
            const float markExtent =
                std::min(style.markSize, style.indicatorSize);
            UIBoxStyle mark{
                .background = context.enabled ? style.mark : style.disabledMark,
                .cornerRadius = glm::vec4{markExtent * 0.5f},
            };
            context.painter.drawBox(makeBox(
                {.center = indicator.center, .size = {markExtent, markExtent}},
                mark,
                context.pickingId,
                kContentZ));
        }
        paintSelectionLabel(context, m_label, style, style.indicatorSize);
    }

    WidgetBounds
    RadioButton::interactionBounds(WidgetBounds bounds) const noexcept {
        const UISelectionControlStyle *style =
            m_options.style.has_value()
                ? &*m_options.style
                : (m_state != nullptr ? &m_state->theme().radio : nullptr);
        if (style == nullptr) {
            return bounds;
        }
        const auto text =
            resolvedTextSize(m_label, style->text, m_labelMeasurement);
        return leadingInteractionBounds(
            bounds,
            {style->indicatorSize +
                 (m_label.empty() ? 0.f : style->gap + text.x),
             std::max({style->minimumHeight, style->indicatorSize, text.y})});
    }

    bool RadioButton::hitTest(WidgetBounds bounds,
                              glm::vec2 position) const noexcept {
        return interactionBounds(bounds).contains(position);
    }

    CursorIcon RadioButton::cursor(const WidgetCursorContext &) const noexcept {
        return CursorIcon::pointer;
    }

    UIEventReply RadioButton::onEvent(WidgetEventContext &context,
                                      const UIEvent &event) {
        WidgetEventContext pressContext = context;
        pressContext.bounds = interactionBounds(context.bounds);
        auto result = m_pressable.handle(pressContext, event);
        if (result.activated && context.enabled && select() && m_selected) {
            m_selected(m_radioId);
        }
        if (const auto *key = event.getIf<Input::KeyEvent>();
            key != nullptr && context.phase == UIEventPhase::target &&
            context.focused && key->action == KeyAction::press &&
            (key->key == KeyCode::arrowLeft || key->key == KeyCode::arrowUp ||
             key->key == KeyCode::arrowRight ||
             key->key == KeyCode::arrowDown)) {
            const int direction =
                key->key == KeyCode::arrowLeft || key->key == KeyCode::arrowUp
                    ? -1
                    : 1;
            const RadioId next = m_group->next(m_radioId, direction);
            if (next && m_group->select(next)) {
                if (const auto nextWidget =
                        findRadio(context.state, m_group.get(), next)) {
                    static_cast<void>(context.state.setFocus(nextWidget));
                }
                if (m_selected) {
                    m_selected(next);
                }
            }
            result.reply.handled = true;
            result.reply.stopPropagation = true;
            result.reply.invalidate |= WidgetInvalidation::paint;
        }
        return result.reply;
    }

    RadioId RadioButton::radioId() const noexcept {
        return m_radioId;
    }

    bool RadioButton::selected() const noexcept {
        return m_group->value() == m_radioId;
    }

    bool RadioButton::select() {
        return m_group->select(m_radioId);
    }

    const std::string &RadioButton::label() const noexcept {
        return m_label;
    }

    void RadioButton::setLabel(std::string label) {
        if (m_label != label) {
            m_label = std::move(label);
            m_intrinsicSizeDirty = true;
            m_labelMeasurement.valid = false;
            if (m_state != nullptr && m_state->contains(m_id)) {
                m_state->invalidate(m_id,
                                    WidgetInvalidation::layout |
                                        WidgetInvalidation::paint);
            }
        }
    }

    std::shared_ptr<RadioGroupModel> RadioButton::group() const noexcept {
        return m_group;
    }

    void RadioButton::reconnectGroup() {
        m_connection.disconnect();
        if (m_group == nullptr || m_state == nullptr) {
            return;
        }
        m_connection = m_group->changed().connect([this](const auto &) {
            if (m_state != nullptr && m_state->contains(m_id)) {
                m_state->invalidate(m_id, WidgetInvalidation::paint);
            }
        });
    }

    Slider::Slider(std::shared_ptr<RangeModel> model,
                   Changed changed,
                   SliderOptions options)
        : m_model(model != nullptr ? std::move(model)
                                   : std::make_shared<RangeModel>()),
          m_changed(std::move(changed)),
          m_options(std::move(options)) {
    }

    std::string_view Slider::typeName() const noexcept {
        return "Slider";
    }

    WidgetTraits Slider::traits() const noexcept {
        return {.focusable = true, .hitTestVisible = true};
    }

    void Slider::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        reconnectModel();
        WidgetLayoutContext layoutContext{.state = context.state,
                                          .id = context.id,
                                          .layout = context.layout,
                                          .themeChanged = true};
        updateLayout(layoutContext);
    }

    void Slider::onUnmount(WidgetTree &, WidgetId) {
        m_connection.disconnect();
        m_state = nullptr;
        m_id = {};
    }

    void Slider::updateLayout(WidgetLayoutContext &context) {
        if (!m_options.autoSize && !context.themeChanged) {
            return;
        }
        if (!m_options.autoSize) {
            return;
        }
        const auto &style =
            m_options.style.value_or(context.state.theme().slider);
        if (m_options.orientation == SliderOrientation::horizontal) {
            context.layout.setWidth(style.minimumLength);
            context.layout.setHeight(style.crossAxisSize);
        } else {
            context.layout.setWidth(style.crossAxisSize);
            context.layout.setHeight(style.minimumLength);
        }
    }

    void Slider::paint(WidgetPaintContext &context) const {
        const auto &style =
            m_options.style.value_or(context.state.theme().slider);
        const float thumbExtent =
            std::min(safeNonNegative(style.thumbSize),
                     m_options.orientation == SliderOrientation::horizontal
                         ? context.bounds.size.y
                         : context.bounds.size.x);
        const double normalized =
            std::clamp(m_model->normalizedValue(), 0.0, 1.0);

        WidgetBounds track;
        WidgetBounds fill;
        WidgetBounds thumb;
        if (m_options.orientation == SliderOrientation::horizontal) {
            const float length =
                std::max(0.f, context.bounds.size.x - thumbExtent);
            const float start = context.bounds.topLeft().x + thumbExtent * 0.5f;
            const float x = start + length * static_cast<float>(normalized);
            track = {.center = context.bounds.center,
                     .size = {length,
                              std::min(style.trackThickness,
                                       context.bounds.size.y)}};
            fill = {.center = {(start + x) * 0.5f, context.bounds.center.y},
                    .size = {std::max(0.f, x - start), track.size.y}};
            thumb = {.center = {x, context.bounds.center.y},
                     .size = {thumbExtent, thumbExtent}};
        } else {
            const float length =
                std::max(0.f, context.bounds.size.y - thumbExtent);
            const float top = context.bounds.topLeft().y + thumbExtent * 0.5f;
            const float bottom = top + length;
            const float y = bottom - length * static_cast<float>(normalized);
            track = {
                .center = context.bounds.center,
                .size = {std::min(style.trackThickness, context.bounds.size.x),
                         length}};
            fill = {.center = {context.bounds.center.x, (y + bottom) * 0.5f},
                    .size = {track.size.x, std::max(0.f, bottom - y)}};
            thumb = {.center = {context.bounds.center.x, y},
                     .size = {thumbExtent, thumbExtent}};
        }

        context.painter.drawBox(
            makeBox(track,
                    context.enabled ? style.track : style.disabledTrack,
                    context.pickingId));
        if (context.enabled && !fill.empty()) {
            context.painter.drawBox(
                makeBox(fill, style.fill, context.pickingId, kContentZ));
        }
        const UIBoxStyle *thumbStyle = &style.thumb;
        if (!context.enabled) {
            thumbStyle = &style.disabledThumb;
        } else if (m_pressable.isPressed()) {
            thumbStyle = &style.thumbPressed;
        } else if (m_pressable.isHovered()) {
            thumbStyle = &style.thumbHovered;
        } else if (context.focused) {
            thumbStyle = &style.thumbFocused;
        }
        context.painter.drawBox(
            makeBox(thumb, *thumbStyle, context.pickingId, kContentZ + 0.001f));
    }

    CursorIcon Slider::cursor(const WidgetCursorContext &) const noexcept {
        return CursorIcon::pointer;
    }

    UIEventReply Slider::onEvent(WidgetEventContext &context,
                                 const UIEvent &event) {
        const bool wasPressed = m_pressable.isPressed();
        auto result = m_pressable.handle(context, event);
        if (context.enabled && context.hasPointerPosition) {
            bool pointerChange = false;
            if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                button != nullptr && button->button == MouseButton::left) {
                pointerChange = (button->action == MouseButtonAction::press &&
                                 context.pointerInside()) ||
                                (button->action == MouseButtonAction::release &&
                                 wasPressed);
            } else if (event.is<Input::MouseMoveEvent>()) {
                pointerChange = wasPressed || m_pressable.isPressed();
            }
            if (pointerChange &&
                setFromPointer(context.bounds, context.pointerPosition) &&
                m_changed) {
                m_changed(m_model->value());
            }
        }

        if (const auto *key = event.getIf<Input::KeyEvent>();
            key != nullptr && context.phase == UIEventPhase::target &&
            context.focused && context.enabled &&
            (key->action == KeyAction::press ||
             key->action == KeyAction::hold)) {
            const double extent = m_model->maximum() - m_model->minimum();
            const double step = m_model->step() > 0.0
                                    ? m_model->step()
                                    : std::max(extent / 100.0, 0.01);
            const auto &resolvedStyle =
                m_options.style.value_or(context.state.theme().slider);
            bool changed = false;
            switch (key->key) {
            case KeyCode::arrowRight:
            case KeyCode::arrowUp:
                changed = changeBy(step);
                break;
            case KeyCode::arrowLeft:
            case KeyCode::arrowDown:
                changed = changeBy(-step);
                break;
            case KeyCode::pageUp:
                changed = changeBy(step * resolvedStyle.pageStepFactor);
                break;
            case KeyCode::pageDown:
                changed = changeBy(-step * resolvedStyle.pageStepFactor);
                break;
            case KeyCode::home:
                changed = m_model->setValue(m_model->minimum());
                break;
            case KeyCode::end:
                changed = m_model->setValue(m_model->maximum());
                break;
            default:
                break;
            }
            if (changed) {
                if (m_changed) {
                    m_changed(m_model->value());
                }
                result.reply.invalidate |= WidgetInvalidation::paint;
            }
            if (key->key == KeyCode::arrowRight ||
                key->key == KeyCode::arrowUp ||
                key->key == KeyCode::arrowLeft ||
                key->key == KeyCode::arrowDown || key->key == KeyCode::pageUp ||
                key->key == KeyCode::pageDown || key->key == KeyCode::home ||
                key->key == KeyCode::end) {
                result.reply.handled = true;
                result.reply.stopPropagation = true;
            }
        }
        return result.reply;
    }

    double Slider::value() const noexcept {
        return m_model->value();
    }

    bool Slider::setValue(double value) {
        return m_model->setValue(value);
    }

    std::shared_ptr<RangeModel> Slider::model() const noexcept {
        return m_model;
    }

    void Slider::reconnectModel() {
        m_connection.disconnect();
        if (m_model == nullptr || m_state == nullptr) {
            return;
        }
        m_connection = m_model->changed().connect([this](const auto &) {
            if (m_state != nullptr && m_state->contains(m_id)) {
                m_state->invalidate(m_id, WidgetInvalidation::paint);
            }
        });
    }

    bool Slider::setFromPointer(WidgetBounds bounds, glm::vec2 pointer) {
        if (m_state == nullptr) {
            return false;
        }
        const auto &resolved =
            m_options.style.value_or(m_state->theme().slider);
        const float crossExtent =
            m_options.orientation == SliderOrientation::horizontal
                ? bounds.size.y
                : bounds.size.x;
        const float thumb = std::min(resolved.thumbSize, crossExtent);
        double normalized = 0.0;
        if (m_options.orientation == SliderOrientation::horizontal) {
            const float start = bounds.topLeft().x + thumb * 0.5f;
            const float length = std::max(0.f, bounds.size.x - thumb);
            normalized = length > 0.f ? (pointer.x - start) / length : 0.0;
        } else {
            const float top = bounds.topLeft().y + thumb * 0.5f;
            const float length = std::max(0.f, bounds.size.y - thumb);
            normalized = length > 0.f ? 1.0 - (pointer.y - top) / length : 0.0;
        }
        normalized = std::clamp(normalized, 0.0, 1.0);
        return m_model->setValue(m_model->minimum() +
                                 normalized *
                                     (m_model->maximum() - m_model->minimum()));
    }

    bool Slider::changeBy(double delta) {
        return m_model->setValue(m_model->value() + delta);
    }

} // namespace Bess::UI
