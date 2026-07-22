#include "controls/text_box.h"

#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Bess::UI {
    namespace {
        constexpr float kChromeZ = 0.001f;
        constexpr float kSelectionZ = 0.002f;
        constexpr float kTextZ = 0.003f;
        constexpr float kCaretZ = 0.004f;

        BoxPaint box(WidgetBounds bounds,
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

        WidgetBounds inset(WidgetBounds bounds, glm::vec2 padding) {
            padding = glm::max(padding, glm::vec2{0.f});
            const glm::vec2 size =
                glm::max(bounds.size - padding * 2.f, glm::vec2{0.f});
            return {.center = bounds.center, .size = size};
        }

        bool pressedOrHeld(const Input::KeyEvent &event) noexcept {
            return event.action == KeyAction::press ||
                   event.action == KeyAction::hold;
        }
    } // namespace

    TextBox::TextBox(std::shared_ptr<TextEditModel> model,
                     Changed changed,
                     Submitted submitted,
                     TextBoxOptions options)
        : m_model(model != nullptr ? std::move(model)
                                   : std::make_shared<TextEditModel>()),
          m_changed(std::move(changed)),
          m_submitted(std::move(submitted)),
          m_options(std::move(options)) {
    }

    std::string_view TextBox::typeName() const noexcept {
        return "TextBox";
    }

    WidgetTraits TextBox::traits() const noexcept {
        return {.focusable = true, .hitTestVisible = true};
    }

    void TextBox::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        reconnectModel();
        WidgetLayoutContext layout{.state = context.state,
                                   .id = context.id,
                                   .layout = context.layout,
                                   .themeChanged = true};
        updateLayout(layout);
    }

    void TextBox::onUnmount(WidgetTree &state, WidgetId) {
        if (m_focused)
            state.platformServices().endTextInput();
        m_connection.disconnect();
        m_pointerSelecting = false;
        m_focused = false;
        m_state = nullptr;
        m_id = {};
    }

    void TextBox::updateLayout(WidgetLayoutContext &context) {
        if (!m_options.autoSize && !context.themeChanged)
            return;
        const auto &style =
            m_options.style.value_or(context.state.theme().textBox);
        if (m_options.autoSize) {
            context.layout.setMinSize(style.minimumSize);
            context.layout.setHeight(style.minimumSize.y);
        }
    }

    void TextBox::update(WidgetUpdateContext &context) {
        if (!m_focused)
            return;
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

    void TextBox::paint(WidgetPaintContext &context) const {
        const auto &style =
            m_options.style.value_or(context.state.theme().textBox);
        const UIBoxStyle *chrome = &style.normal;
        if (!context.enabled)
            chrome = &style.disabled;
        else if (context.focused)
            chrome = &style.focused;
        else if (context.hovered)
            chrome = &style.hovered;
        context.painter.drawBox(
            box(context.bounds, *chrome, context.pickingId));

        const WidgetBounds content = inset(context.bounds, style.padding);
        if (content.empty())
            return;
        const auto &text = m_model->text();
        const auto selection = m_model->selection();
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
            !m_model->hasComposition()) {
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

        if (const auto composition = m_model->compositionRange()) {
            const float start = xFor(composition->start);
            const float end = xFor(composition->end);
            context.painter.drawBox({
                .bounds = {.center = {(start + end) * 0.5f,
                                      content.bottomRight().y -
                                          style.compositionUnderlineThickness *
                                              0.5f},
                           .size = {std::max(style.caretWidth, end - start),
                                    style.compositionUnderlineThickness}},
                .color = style.compositionUnderline,
                .zIndex = kCaretZ,
            });
        }

        const float caretWidth =
            std::min(content.size.x, std::max(1.f, style.caretWidth));
        const float halfCaretWidth = caretWidth * 0.5f;
        // xFor() is the insertion edge. Treating that edge as the rectangle's
        // center puts half the caret outside the scissor at byte position zero
        // and can remove it entirely after rasterization. Keep the complete
        // caret inside the editable viewport instead.
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

    CursorIcon TextBox::cursor(const WidgetCursorContext &) const noexcept {
        return CursorIcon::text;
    }

    UIEventReply TextBox::onEvent(WidgetEventContext &context,
                                  const UIEvent &event) {
        if (context.phase != UIEventPhase::target)
            return {};

        if (const auto *focus = event.getIf<UIFocusChangedEvent>()) {
            m_focused = focus->focused;
            m_pointerSelecting = false;
            resetCaretBlink();
            if (focus->focused) {
                context.state.platformServices().beginTextInput();
                if (m_options.selectAllOnFocus) {
                    static_cast<void>(m_model->selectAll());
                }
            } else {
                const std::string previous = m_model->text();
                static_cast<void>(m_model->commitComposition());
                context.state.platformServices().endTextInput();
                notifyChangedIfNeeded(previous);
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
                    static_cast<void>(m_model->selectWordAt(offset));
                } else {
                    static_cast<void>(
                        m_model->setCaret(offset, event.modifiers.shift));
                }
                m_pointerSelecting = true;
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
            static_cast<void>(m_model->setCaret(
                byteOffsetAt(context.pointerPosition.x), true));
            resetCaretBlink();
            return {.handled = true,
                    .stopPropagation = true,
                    .invalidate = WidgetInvalidation::paint};
        }

        if (!context.focused || !context.enabled)
            return {};

        if (const auto *text = event.getIf<Input::TextInputEvent>()) {
            if (m_options.readOnly || text->codepoint < 0x20 ||
                text->codepoint == 0x7F)
                return {.handled = true, .stopPropagation = true};
            const std::string previous = m_model->text();
            static_cast<void>(m_model->insertCodepoint(text->codepoint));
            resetCaretBlink();
            notifyChangedIfNeeded(previous);
            return {.handled = true,
                    .stopPropagation = true,
                    .invalidate = WidgetInvalidation::paint};
        }

        if (const auto *composition =
                event.getIf<Input::TextCompositionEvent>()) {
            const std::string previous = m_model->text();
            if (m_options.readOnly) {
                static_cast<void>(m_model->cancelComposition());
                resetCaretBlink();
                return {.handled = true,
                        .stopPropagation = true,
                        .invalidate = WidgetInvalidation::paint};
            }
            switch (composition->phase) {
            case Input::TextCompositionPhase::begin:
                static_cast<void>(m_model->beginComposition());
                break;
            case Input::TextCompositionPhase::update:
                static_cast<void>(
                    m_model->updateComposition(singleLine(composition->text),
                                               composition->selectionStart,
                                               composition->selectionLength));
                break;
            case Input::TextCompositionPhase::commit:
                static_cast<void>(
                    m_model->commitComposition(singleLine(composition->text)));
                break;
            case Input::TextCompositionPhase::cancel:
                static_cast<void>(m_model->cancelComposition());
                break;
            }
            resetCaretBlink();
            notifyChangedIfNeeded(previous);
            return {.handled = true,
                    .stopPropagation = true,
                    .invalidate = WidgetInvalidation::paint};
        }

        const auto *key = event.getIf<Input::KeyEvent>();
        if (key == nullptr || !pressedOrHeld(*key))
            return {};
        const bool command = event.modifiers.control || event.modifiers.super;
        const bool selecting = event.modifiers.shift;
        const auto unit =
            command ? TextNavigationUnit::word : TextNavigationUnit::grapheme;
        const std::string previous = m_model->text();
        bool recognized = false;
        if (command) {
            switch (key->key) {
            case KeyCode::a:
                static_cast<void>(m_model->selectAll());
                recognized = true;
                break;
            case KeyCode::c:
                static_cast<void>(copySelection(context.state));
                recognized = true;
                break;
            case KeyCode::x:
                if (!m_options.readOnly)
                    static_cast<void>(cutSelection(context.state));
                recognized = true;
                break;
            case KeyCode::v:
                if (!m_options.readOnly)
                    static_cast<void>(insertClipboardText(context.state));
                recognized = true;
                break;
            case KeyCode::z:
                if (!m_options.readOnly)
                    static_cast<void>(selecting ? m_model->redo()
                                                : m_model->undo());
                recognized = true;
                break;
            case KeyCode::y:
                if (!m_options.readOnly)
                    static_cast<void>(m_model->redo());
                recognized = true;
                break;
            default:
                break;
            }
        }
        if (!recognized) {
            switch (key->key) {
            case KeyCode::arrowLeft:
                static_cast<void>(m_model->moveCaret(
                    TextNavigationDirection::backward, unit, selecting));
                recognized = true;
                break;
            case KeyCode::arrowRight:
                static_cast<void>(m_model->moveCaret(
                    TextNavigationDirection::forward, unit, selecting));
                recognized = true;
                break;
            case KeyCode::home:
                static_cast<void>(m_model->moveToStart(selecting));
                recognized = true;
                break;
            case KeyCode::end:
                static_cast<void>(m_model->moveToEnd(selecting));
                recognized = true;
                break;
            case KeyCode::backspace:
                if (!m_options.readOnly)
                    static_cast<void>(m_model->eraseBackward(unit));
                recognized = true;
                break;
            case KeyCode::del:
                if (!m_options.readOnly)
                    static_cast<void>(m_model->eraseForward(unit));
                recognized = true;
                break;
            case KeyCode::enter:
                if (m_submitted)
                    m_submitted(m_model->text());
                recognized = true;
                break;
            case KeyCode::escape:
                static_cast<void>(m_model->cancelComposition());
                recognized = true;
                break;
            default:
                break;
            }
        }
        if (!recognized)
            return {};
        resetCaretBlink();
        notifyChangedIfNeeded(previous);
        return {.handled = true,
                .stopPropagation = key->key != KeyCode::escape,
                .invalidate = WidgetInvalidation::paint};
    }

    std::shared_ptr<TextEditModel> TextBox::model() const noexcept {
        return m_model;
    }

    const std::string &TextBox::text() const noexcept {
        return m_model->text();
    }

    bool TextBox::setText(std::string_view text) {
        return m_model->setText(singleLine(text));
    }

    bool TextBox::readOnly() const noexcept {
        return m_options.readOnly;
    }

    void TextBox::setReadOnly(bool readOnly) noexcept {
        m_options.readOnly = readOnly;
    }

    void TextBox::setChanged(Changed changed) {
        m_changed = std::move(changed);
    }

    void TextBox::setSubmitted(Submitted submitted) {
        m_submitted = std::move(submitted);
    }

    void TextBox::reconnectModel() {
        m_connection.disconnect();
        if (m_model == nullptr || m_state == nullptr)
            return;
        m_connection = m_model->changed().connect([this](const auto &) {
            resetCaretBlink();
            if (m_state != nullptr && m_state->contains(m_id)) {
                m_state->invalidate(m_id, WidgetInvalidation::paint);
            }
        });
    }

    void TextBox::resetCaretBlink() noexcept {
        m_blinkElapsedMs = 0.0;
        m_caretVisible = true;
    }

    size_t TextBox::byteOffsetAt(float x) const noexcept {
        if (m_caretPositions.empty())
            return 0;
        const float local = x - m_contentLeft + m_scrollX;
        if (local <= 0.f)
            return 0;
        for (size_t index = 1; index < m_caretPositions.size(); ++index) {
            const float midpoint = (m_caretPositions[index - 1].second +
                                    m_caretPositions[index].second) *
                                   0.5f;
            if (local < midpoint)
                return m_caretPositions[index - 1].first;
        }
        return m_caretPositions.back().first;
    }

    bool TextBox::insertClipboardText(WidgetTree &tree) {
        const auto text = tree.platformServices().readClipboardText();
        return text.has_value() && m_model->insertText(singleLine(*text));
    }

    bool TextBox::copySelection(WidgetTree &tree) const {
        return m_model->hasSelection() &&
               tree.platformServices().writeClipboardText(
                   m_model->selectedText());
    }

    bool TextBox::cutSelection(WidgetTree &tree) {
        if (!copySelection(tree))
            return false;
        return m_model->replaceSelection({});
    }

    void TextBox::notifyChangedIfNeeded(std::string previousText) {
        if (previousText != m_model->text() && m_changed)
            m_changed(m_model->text());
    }

    std::string TextBox::singleLine(std::string_view text) const {
        std::string result;
        result.reserve(text.size());
        for (const char value : text) {
            if (value != '\r' && value != '\n')
                result.push_back(value);
        }
        return result;
    }

} // namespace Bess::UI
