#include "controls/numeric_input.h"

#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

namespace Bess::UI {
    namespace {
        constexpr float kChromeZ = 0.001f;
        constexpr float kSelectionZ = 0.002f;
        constexpr float kTextZ = 0.003f;
        constexpr float kCaretZ = 0.004f;

        [[nodiscard]] bool isFinite(double value) noexcept {
            return std::isfinite(value);
        }

        [[nodiscard]] int clampPrecision(int precision) noexcept {
            return std::clamp(precision, 0, 9);
        }

        void trimTrailingZeros(std::string &text) {
            const auto dot = text.find('.');
            if (dot == std::string::npos) {
                return;
            }
            while (!text.empty() && text.back() == '0') {
                text.pop_back();
            }
            if (!text.empty() && text.back() == '.') {
                text.pop_back();
            }
            if (text == "-0") {
                text = "0";
            }
        }

        [[nodiscard]] BoxPaint box(WidgetBounds bounds,
                                   const UIBoxStyle &style,
                                   PickingId picking,
                                   float z = kChromeZ) {
            return {.bounds = bounds,
                    .color = style.background,
                    .borderColor = style.border,
                    .cornerRadius = style.cornerRadius,
                    .borderThickness = style.borderThickness,
                    .shadow = style.shadow,
                    .zIndex = z,
                    .pickingId = picking};
        }

        [[nodiscard]] WidgetBounds inset(WidgetBounds bounds, glm::vec2 padding) {
            padding = glm::max(padding, glm::vec2{0.f});
            const glm::vec2 size =
                glm::max(bounds.size - padding * 2.f, glm::vec2{0.f});
            return {.center = bounds.center, .size = size};
        }

        [[nodiscard]] bool pressedOrHeld(const Input::KeyEvent &event) noexcept {
            return event.action == KeyAction::press ||
                   event.action == KeyAction::hold;
        }

        [[nodiscard]] std::string singleLine(std::string_view text) {
            std::string result;
            result.reserve(text.size());
            for (const char ch : text) {
                if (ch == '\n' || ch == '\r') {
                    continue;
                }
                result.push_back(ch);
            }
            return result;
        }
    } // namespace

    NumericInput::NumericInput(NumericInputKind kind,
                               std::shared_ptr<NumericModel> model,
                               Changed changed,
                               Submitted submitted,
                               NumericInputOptions options)
        : m_kind(kind),
          m_model(model != nullptr ? std::move(model)
                                   : std::make_shared<NumericModel>()),
          m_textModel(std::make_shared<TextEditModel>(
              "", options.maximumBytes > 0 ? options.maximumBytes : 64)),
          m_changed(std::move(changed)),
          m_submitted(std::move(submitted)),
          m_options(std::move(options)) {
        m_options.precision = clampPrecision(m_options.precision);
        if (m_kind == NumericInputKind::integer) {
            m_options.precision = 0;
        }
        if (!std::isfinite(m_options.step) || m_options.step < 0.0) {
            m_options.step = 0.0;
        }
        if (m_options.minimum.has_value() || m_options.maximum.has_value()) {
            m_options.clampToRange = true;
        }
        syncTextFromValue();
    }

    std::string_view NumericInput::typeName() const noexcept {
        return m_kind == NumericInputKind::integer ? "IntInput" : "FloatInput";
    }

    WidgetTraits NumericInput::traits() const noexcept {
        return {.focusable = true, .hitTestVisible = true};
    }

    void NumericInput::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        reconnectModel();
        WidgetLayoutContext layout{.state = context.state,
                                   .id = context.id,
                                   .layout = context.layout,
                                   .themeChanged = true};
        updateLayout(layout);
    }

    void NumericInput::onUnmount(WidgetTree &state, WidgetId) {
        if (m_focused) {
            state.platformServices().endTextInput();
        }
        m_connection.disconnect();
        m_textConnection.disconnect();
        m_pointerSelecting = false;
        m_focused = false;
        m_editing = false;
        m_state = nullptr;
        m_id = {};
    }

    void NumericInput::updateLayout(WidgetLayoutContext &context) {
        if (!m_options.autoSize && !context.themeChanged) {
            return;
        }
        const auto &style =
            m_options.style.value_or(context.state.theme().textBox);
        if (m_options.autoSize) {
            context.layout.setMinSize(style.minimumSize);
            context.layout.setHeight(style.minimumSize.y);
        }
    }

    void NumericInput::update(WidgetUpdateContext &context) {
        if (!m_focused) {
            return;
        }
        const auto &style =
            m_options.style.value_or(context.state.theme().textBox);
        const double interval =
            std::max(100.0, static_cast<double>(style.blinkIntervalMs));
        m_blinkElapsedMs += std::max(0.0, context.deltaTime.count());
        if (m_blinkElapsedMs >= interval) {
            m_blinkElapsedMs = std::fmod(m_blinkElapsedMs, interval);
            m_caretVisible = !m_caretVisible;
            context.state.invalidate(context.id, WidgetInvalidation::paint);
        }
    }

    void NumericInput::paint(WidgetPaintContext &context) const {
        const auto &style =
            m_options.style.value_or(context.state.theme().textBox);
        const UIBoxStyle *chrome = &style.normal;
        if (!context.enabled) {
            chrome = &style.disabled;
        } else if (context.focused) {
            chrome = &style.focused;
        } else if (context.hovered) {
            chrome = &style.hovered;
        }
        context.painter.drawBox(
            box(context.bounds, *chrome, context.pickingId));

        const WidgetBounds content = inset(context.bounds, style.padding);
        if (content.empty()) {
            return;
        }
        const auto &text = m_textModel->text();
        const auto selection = m_textModel->selection();
        if (!m_caretMetricsValid || m_caretMetricsText != text ||
            m_caretMetricsFontSize != style.text.fontSize ||
            m_caretMetricsLetterSpacing != style.text.letterSpacing) {
            m_caretPositions.clear();
            m_caretPositions.emplace_back(0, 0.f);
            for (size_t offset = 0; offset < text.size();) {
                offset = TextEditModel::nextGraphemeBoundary(text, offset);
                const float x = context.painter
                                    .measureText(text.substr(0, offset),
                                                 style.text.fontSize,
                                                 style.text.letterSpacing)
                                    .x;
                m_caretPositions.emplace_back(offset, x);
            }
            m_caretMetricsText = text;
            m_caretMetricsFontSize = style.text.fontSize;
            m_caretMetricsLetterSpacing = style.text.letterSpacing;
            m_caretMetricsValid = true;
        }
        const float fullWidth = m_caretPositions.back().second;
        const auto caretIt =
            std::lower_bound(m_caretPositions.begin(),
                             m_caretPositions.end(),
                             selection.caret,
                             [](const auto &entry, size_t offset) {
                                 return entry.first < offset;
                             });
        const float caretX =
            caretIt != m_caretPositions.end() ? caretIt->second : fullWidth;
        const float visibleWidth = content.size.x;
        if (caretX - m_scrollX > visibleWidth) {
            m_scrollX = caretX - visibleWidth;
        } else if (caretX < m_scrollX) {
            m_scrollX = caretX;
        }
        m_scrollX =
            std::clamp(m_scrollX, 0.f, std::max(0.f, fullWidth - visibleWidth));
        m_contentLeft = content.topLeft().x;

        ScopedUIClip clip{context.painter, content};
        const float originX = content.topLeft().x - m_scrollX;
        const float lineHeight = std::min(
            content.size.y, std::max(1.f, style.text.fontSize * 1.25f));
        auto xFor = [&](size_t byte) {
            const auto it =
                std::lower_bound(m_caretPositions.begin(),
                                 m_caretPositions.end(),
                                 byte,
                                 [](const auto &entry, size_t offset) {
                                     return entry.first < offset;
                                 });
            return originX +
                   (it != m_caretPositions.end() ? it->second : fullWidth);
        };

        if (!selection.collapsed()) {
            const float start = xFor(selection.start());
            const float end = xFor(selection.end());
            context.painter.drawBox({
                .bounds = {.center = {(start + end) * 0.5f, content.center.y},
                           .size = {std::max(0.f, end - start), lineHeight}},
                .color = style.selection,
                .zIndex = kSelectionZ,
            });
        }

        if (text.empty() && !m_options.placeholder.empty() &&
            !m_textModel->hasComposition()) {
            context.painter.drawText(
                m_options.placeholder,
                {.bounds = content,
                 .fontSize = style.placeholder.fontSize,
                 .color = style.placeholder.color,
                 .horizontal = HorizontalTextAlignment::start,
                 .vertical = VerticalTextAlignment::center,
                 .zIndex = kTextZ,
                 .letterSpacing = style.placeholder.letterSpacing});
        } else {
            const WidgetBounds textBounds{
                .center = {originX + std::max(fullWidth, visibleWidth) * 0.5f,
                           content.center.y},
                .size = {std::max(fullWidth, visibleWidth), content.size.y},
            };
            context.painter.drawText(
                text,
                {.bounds = textBounds,
                 .fontSize = style.text.fontSize,
                 .color = context.enabled ? style.text.color
                                          : style.text.color.withAlpha(0.38f),
                 .horizontal = HorizontalTextAlignment::start,
                 .vertical = VerticalTextAlignment::center,
                 .zIndex = kTextZ,
                 .letterSpacing = style.text.letterSpacing});
        }

        const float caretWidth =
            std::min(content.size.x, std::max(1.f, style.caretWidth));
        const float halfCaretWidth = caretWidth * 0.5f;
        const float drawnCaretCenterX =
            std::clamp(xFor(selection.caret) + halfCaretWidth,
                       content.topLeft().x + halfCaretWidth,
                       content.bottomRight().x - halfCaretWidth);
        const WidgetBounds caretBounds{
            .center = {drawnCaretCenterX, content.center.y},
            .size = {caretWidth, lineHeight},
        };
        if (context.focused && selection.collapsed() && m_caretVisible) {
            context.painter.drawBox({.bounds = caretBounds,
                                     .color = style.caret,
                                     .zIndex = kCaretZ});
        }
        if (context.focused) {
            WidgetBounds platformCaret = caretBounds;
            platformCaret.center += context.state.getViewportSize() * 0.5f;
            context.state.platformServices().updateTextInputArea(platformCaret);
        }
    }

    CursorIcon NumericInput::cursor(const WidgetCursorContext &) const noexcept {
        return CursorIcon::text;
    }

    UIEventReply NumericInput::onEvent(WidgetEventContext &context,
                                       const UIEvent &event) {
        if (context.phase != UIEventPhase::target) {
            return {};
        }

        if (const auto *focus = event.getIf<UIFocusChangedEvent>()) {
            m_focused = focus->focused;
            m_pointerSelecting = false;
            resetCaretBlink();
            if (focus->focused) {
                m_editing = true;
                context.state.platformServices().beginTextInput();
                if (m_options.selectAllOnFocus) {
                    static_cast<void>(m_textModel->selectAll());
                }
            } else {
                commitText();
                m_editing = false;
                context.state.platformServices().endTextInput();
            }
            return {.invalidate = WidgetInvalidation::paint};
        }

        if (const auto *button = event.getIf<Input::MouseButtonEvent>();
            button != nullptr && button->button == MouseButton::left &&
            context.hasPointerPosition) {
            if ((button->action == MouseButtonAction::press ||
                 button->action == MouseButtonAction::doubleClick) &&
                context.pointerInside()) {
                const size_t offset = byteOffsetAt(context.pointerPosition.x);
                if (button->action == MouseButtonAction::doubleClick) {
                    static_cast<void>(m_textModel->selectWordAt(offset));
                } else {
                    static_cast<void>(
                        m_textModel->setCaret(offset, event.modifiers.shift));
                }
                m_pointerSelecting = true;
                m_editing = true;
                resetCaretBlink();
                return {.handled = true,
                        .stopPropagation = true,
                        .requestFocus = true,
                        .capturePointer = true,
                        .invalidate = WidgetInvalidation::paint};
            }
            if (button->action == MouseButtonAction::release &&
                m_pointerSelecting) {
                m_pointerSelecting = false;
                return {.handled = true,
                        .stopPropagation = true,
                        .releasePointer = true,
                        .invalidate = WidgetInvalidation::paint};
            }
        }
        if (event.is<Input::MouseMoveEvent>() && m_pointerSelecting &&
            context.hasPointerPosition) {
            static_cast<void>(m_textModel->setCaret(
                byteOffsetAt(context.pointerPosition.x), true));
            resetCaretBlink();
            return {.handled = true,
                    .stopPropagation = true,
                    .invalidate = WidgetInvalidation::paint};
        }

        if (!context.focused || !context.enabled) {
            return {};
        }

        if (const auto *text = event.getIf<Input::TextInputEvent>()) {
            if (m_options.readOnly || text->codepoint < 0x20 ||
                text->codepoint == 0x7F) {
                return {.handled = true, .stopPropagation = true};
            }
            const std::string previous = m_textModel->text();
            static_cast<void>(m_textModel->insertCodepoint(text->codepoint));
            if (!applyEditFilter(m_textModel->text())) {
                static_cast<void>(m_textModel->setText(previous, true));
            } else {
                m_editing = true;
            }
            resetCaretBlink();
            return {.handled = true,
                    .stopPropagation = true,
                    .invalidate = WidgetInvalidation::paint};
        }

        const auto *key = event.getIf<Input::KeyEvent>();
        if (key == nullptr || !pressedOrHeld(*key)) {
            return {};
        }

        if (key->key == KeyCode::arrowUp || key->key == KeyCode::arrowDown) {
            commitText();
            const double direction =
                key->key == KeyCode::arrowUp ? 1.0 : -1.0;
            if (changeByStep(direction, event.modifiers.shift)) {
                syncTextFromValue(m_options.selectAllOnFocus);
            }
            resetCaretBlink();
            return {.handled = true,
                    .stopPropagation = true,
                    .invalidate = WidgetInvalidation::paint};
        }

        const bool command = event.modifiers.control || event.modifiers.super;
        const bool selecting = event.modifiers.shift;
        const auto unit =
            command ? TextNavigationUnit::word : TextNavigationUnit::grapheme;
        const std::string previous = m_textModel->text();
        bool recognized = false;
        if (command) {
            switch (key->key) {
            case KeyCode::a:
                static_cast<void>(m_textModel->selectAll());
                recognized = true;
                break;
            case KeyCode::z:
                if (!m_options.readOnly) {
                    static_cast<void>(selecting ? m_textModel->redo()
                                                : m_textModel->undo());
                    if (!applyEditFilter(m_textModel->text())) {
                        static_cast<void>(m_textModel->setText(previous, true));
                    }
                }
                recognized = true;
                break;
            case KeyCode::y:
                if (!m_options.readOnly) {
                    static_cast<void>(m_textModel->redo());
                    if (!applyEditFilter(m_textModel->text())) {
                        static_cast<void>(m_textModel->setText(previous, true));
                    }
                }
                recognized = true;
                break;
            default:
                break;
            }
        }
        if (!recognized) {
            switch (key->key) {
            case KeyCode::arrowLeft:
                static_cast<void>(m_textModel->moveCaret(
                    TextNavigationDirection::backward, unit, selecting));
                recognized = true;
                break;
            case KeyCode::arrowRight:
                static_cast<void>(m_textModel->moveCaret(
                    TextNavigationDirection::forward, unit, selecting));
                recognized = true;
                break;
            case KeyCode::home:
                static_cast<void>(m_textModel->moveToStart(selecting));
                recognized = true;
                break;
            case KeyCode::end:
                static_cast<void>(m_textModel->moveToEnd(selecting));
                recognized = true;
                break;
            case KeyCode::backspace:
                if (!m_options.readOnly) {
                    static_cast<void>(m_textModel->eraseBackward(unit));
                    m_editing = true;
                }
                recognized = true;
                break;
            case KeyCode::del:
                if (!m_options.readOnly) {
                    static_cast<void>(m_textModel->eraseForward(unit));
                    m_editing = true;
                }
                recognized = true;
                break;
            case KeyCode::enter:
                commitText();
                if (m_submitted) {
                    m_submitted(m_model->value());
                }
                recognized = true;
                break;
            case KeyCode::escape:
                restoreValidText();
                static_cast<void>(m_textModel->cancelComposition());
                recognized = true;
                break;
            default:
                break;
            }
        }
        if (!recognized) {
            return {};
        }
        resetCaretBlink();
        return {.handled = true,
                .stopPropagation = key->key != KeyCode::escape,
                .invalidate = WidgetInvalidation::paint};
    }

    NumericInputKind NumericInput::kind() const noexcept {
        return m_kind;
    }

    std::shared_ptr<NumericModel> NumericInput::model() const noexcept {
        return m_model;
    }

    double NumericInput::value() const noexcept {
        return m_model->value();
    }

    bool NumericInput::setValue(double value) {
        const double next = normalize(value);
        if (!m_model->setValue(next)) {
            if (!m_editing) {
                syncTextFromValue();
            }
            return false;
        }
        if (!m_editing) {
            syncTextFromValue();
        }
        return true;
    }

    const std::string &NumericInput::text() const noexcept {
        return m_textModel->text();
    }

    bool NumericInput::readOnly() const noexcept {
        return m_options.readOnly;
    }

    void NumericInput::setReadOnly(bool readOnly) noexcept {
        m_options.readOnly = readOnly;
    }

    void NumericInput::setChanged(Changed changed) {
        m_changed = std::move(changed);
    }

    void NumericInput::setSubmitted(Submitted submitted) {
        m_submitted = std::move(submitted);
    }

    void NumericInput::setRange(std::optional<double> minimum,
                                std::optional<double> maximum,
                                bool clamp) {
        m_options.minimum = minimum;
        m_options.maximum = maximum;
        m_options.clampToRange = clamp && (minimum.has_value() || maximum.has_value());
        static_cast<void>(setValue(m_model->value()));
    }

    void NumericInput::setStep(double step) noexcept {
        m_options.step =
            std::isfinite(step) && step > 0.0 ? step : 0.0;
    }

    void NumericInput::setPrecision(int precision) {
        if (m_kind == NumericInputKind::integer) {
            m_options.precision = 0;
            return;
        }
        const int next = clampPrecision(precision);
        if (m_options.precision == next) {
            return;
        }
        m_options.precision = next;
        if (!m_editing) {
            syncTextFromValue();
        }
    }

    void NumericInput::reconnectModel() {
        m_connection.disconnect();
        m_textConnection.disconnect();
        if (m_model != nullptr) {
            m_connection = m_model->changed().connect([this](const auto &) {
                if (!m_editing) {
                    syncTextFromValue();
                }
                if (m_state != nullptr && m_state->contains(m_id)) {
                    m_state->invalidate(m_id, WidgetInvalidation::paint);
                }
            });
        }
        if (m_textModel != nullptr) {
            m_textConnection =
                m_textModel->changed().connect([this](const auto &) {
                    m_caretMetricsValid = false;
                    if (m_state != nullptr && m_state->contains(m_id)) {
                        m_state->invalidate(m_id, WidgetInvalidation::paint);
                    }
                });
        }
    }

    void NumericInput::resetCaretBlink() noexcept {
        m_blinkElapsedMs = 0.0;
        m_caretVisible = true;
    }

    size_t NumericInput::byteOffsetAt(float x) const noexcept {
        if (m_caretPositions.empty()) {
            return 0;
        }
        const float local = x - m_contentLeft + m_scrollX;
        if (local <= 0.f) {
            return 0;
        }
        for (size_t index = 1; index < m_caretPositions.size(); ++index) {
            const float midpoint = (m_caretPositions[index - 1].second +
                                    m_caretPositions[index].second) *
                                   0.5f;
            if (local < midpoint) {
                return m_caretPositions[index - 1].first;
            }
        }
        return m_caretPositions.back().first;
    }

    void NumericInput::notifyChangedIfNeeded(double previous) {
        if (previous == m_model->value() || !m_changed) {
            return;
        }
        m_changed(m_model->value());
    }

    void NumericInput::syncTextFromValue(bool selectAll) {
        const std::string text = formatValue(m_model->value());
        static_cast<void>(m_textModel->setText(text));
        if (selectAll) {
            static_cast<void>(m_textModel->selectAll());
        }
        m_caretMetricsValid = false;
    }

    void NumericInput::commitText() {
        double parsed = 0.0;
        if (parseValue(m_textModel->text(), parsed)) {
            const double previous = m_model->value();
            static_cast<void>(m_model->setValue(normalize(parsed)));
            syncTextFromValue();
            m_editing = false;
            notifyChangedIfNeeded(previous);
            return;
        }
        restoreValidText();
        m_editing = false;
    }

    void NumericInput::restoreValidText() {
        syncTextFromValue();
        m_editing = false;
    }

    bool NumericInput::applyEditFilter(std::string_view candidate) {
        return isValidPartialEdit(candidate);
    }

    bool NumericInput::changeByStep(double direction, bool coarse) {
        const double step = effectiveStep();
        if (step <= 0.0) {
            return false;
        }
        const double delta = direction * step * (coarse ? 10.0 : 1.0);
        const double previous = m_model->value();
        if (!m_model->setValue(normalize(previous + delta))) {
            return false;
        }
        notifyChangedIfNeeded(previous);
        return true;
    }

    std::string NumericInput::formatValue(double value) const {
        value = sanitize(value);
        if (m_kind == NumericInputKind::integer) {
            const long long rounded = static_cast<long long>(std::llround(value));
            return std::to_string(rounded);
        }

        std::array<char, 96> buffer{};
        const int digits = resolvedPrecision();
        auto result = std::to_chars(buffer.data(),
                                    buffer.data() + buffer.size(),
                                    value,
                                    std::chars_format::fixed,
                                    digits);
        if (result.ec != std::errc{}) {
            result = std::to_chars(buffer.data(),
                                   buffer.data() + buffer.size(),
                                   value,
                                   std::chars_format::general,
                                   std::max(1, digits + 1));
        }
        if (result.ec != std::errc{}) {
            return "0";
        }
        std::string text{buffer.data(), result.ptr};
        trimTrailingZeros(text);
        return text.empty() ? "0" : text;
    }

    bool NumericInput::parseValue(std::string_view text,
                                  double &out) const noexcept {
        if (text.empty()) {
            return false;
        }
        const std::string owned{singleLine(text)};
        char *end = nullptr;
        errno = 0;
        const double parsed = std::strtod(owned.c_str(), &end);
        if (end != owned.c_str() + owned.size() || errno == ERANGE ||
            !isFinite(parsed)) {
            return false;
        }
        if (m_kind == NumericInputKind::integer) {
            // Reject non-integral floats such as "1.5" for integer fields.
            if (std::floor(parsed) != parsed) {
                return false;
            }
        }
        out = parsed;
        return true;
    }

    bool NumericInput::isValidPartialEdit(std::string_view text) const noexcept {
        if (text.empty()) {
            return true;
        }
        bool sawDot = false;
        bool sawDigit = false;
        bool sawExponent = false;
        for (size_t index = 0; index < text.size(); ++index) {
            const char ch = text[index];
            if (ch >= '0' && ch <= '9') {
                sawDigit = true;
                continue;
            }
            if ((ch == '-' || ch == '+') &&
                (index == 0 ||
                 (sawExponent &&
                  (text[index - 1] == 'e' || text[index - 1] == 'E')))) {
                continue;
            }
            if (m_kind == NumericInputKind::floatingPoint && ch == '.' &&
                !sawDot && !sawExponent) {
                sawDot = true;
                continue;
            }
            if (m_kind == NumericInputKind::floatingPoint &&
                (ch == 'e' || ch == 'E') && sawDigit && !sawExponent) {
                sawExponent = true;
                continue;
            }
            return false;
        }
        return true;
    }

    double NumericInput::sanitize(double value) const noexcept {
        return isFinite(value) ? value : m_model->value();
    }

    double NumericInput::normalize(double value) const noexcept {
        value = sanitize(value);
        if (m_kind == NumericInputKind::integer) {
            value = static_cast<double>(std::llround(value));
        }
        if (!m_options.clampToRange) {
            return value;
        }
        if (m_options.minimum.has_value()) {
            value = std::max(value, *m_options.minimum);
        }
        if (m_options.maximum.has_value()) {
            value = std::min(value, *m_options.maximum);
        }
        return value;
    }

    double NumericInput::effectiveStep() const noexcept {
        if (std::isfinite(m_options.step) && m_options.step > 0.0) {
            return m_options.step;
        }
        return m_kind == NumericInputKind::integer ? 1.0 : 0.0;
    }

    int NumericInput::resolvedPrecision() const noexcept {
        return m_kind == NumericInputKind::integer
                   ? 0
                   : clampPrecision(m_options.precision);
    }

} // namespace Bess::UI
