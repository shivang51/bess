#include "bess_core/scene/scene_ui/text_box_context.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_draw_helpers.h"
#include <algorithm>

namespace Bess::Canvas::UI {
    namespace {
        std::string g_textClipboard;

        bool isUtf8Continuation(char ch) {
            return (static_cast<unsigned char>(ch) & 0xC0) == 0x80;
        }

        size_t previousCharBoundary(std::string_view text, size_t cursor) {
            cursor = std::min(cursor, text.size());
            if (cursor == 0) {
                return 0;
            }

            --cursor;
            while (cursor > 0 && isUtf8Continuation(text[cursor])) {
                --cursor;
            }
            return cursor;
        }

        size_t nextCharBoundary(std::string_view text, size_t cursor) {
            cursor = std::min(cursor, text.size());
            if (cursor >= text.size()) {
                return text.size();
            }

            ++cursor;
            while (cursor < text.size() && isUtf8Continuation(text[cursor])) {
                ++cursor;
            }
            return cursor;
        }

        size_t nextStartBoundary(std::string_view text, size_t cursor) {
            cursor = std::min(cursor, text.size());
            while (cursor < text.size() && isUtf8Continuation(text[cursor])) {
                ++cursor;
            }
            return cursor;
        }

        size_t utf8PrefixBoundary(std::string_view text, size_t maxBytes) {
            maxBytes = std::min(maxBytes, text.size());
            while (maxBytes > 0 && maxBytes < text.size() &&
                   isUtf8Continuation(text[maxBytes])) {
                --maxBytes;
            }
            return maxBytes;
        }

        std::string boundedText(std::string_view text, size_t maxBytes) {
            const size_t size = text.size() <= maxBytes
                                    ? text.size()
                                    : utf8PrefixBoundary(text, maxBytes);
            return std::string(text.substr(0, size));
        }

        bool appendUtf8(char32_t codepoint, std::string &out) {
            if (codepoint == 0 || codepoint > 0x10FFFF ||
                (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
                return false;
            }

            if (codepoint <= 0x7F) {
                out.push_back(static_cast<char>(codepoint));
            } else if (codepoint <= 0x7FF) {
                out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            } else if (codepoint <= 0xFFFF) {
                out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                out.push_back(
                    static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            } else {
                out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
                out.push_back(
                    static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                out.push_back(
                    static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            return true;
        }

        bool isAsciiSpace(char ch) {
            return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
        }

        bool isAsciiWord(char ch) {
            const auto c = static_cast<unsigned char>(ch);
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_';
        }

        enum class CharClass : uint8_t { space, word, punctuation };

        CharClass charClassAt(std::string_view text, size_t cursor) {
            if (cursor >= text.size()) {
                return CharClass::space;
            }

            const char ch = text[cursor];
            if (isAsciiSpace(ch)) {
                return CharClass::space;
            }

            if (static_cast<unsigned char>(ch) >= 0x80 || isAsciiWord(ch)) {
                return CharClass::word;
            }

            return CharClass::punctuation;
        }

        size_t previousWordBoundary(std::string_view text, size_t cursor) {
            cursor = std::min(cursor, text.size());
            while (cursor > 0 &&
                   charClassAt(text, previousCharBoundary(text, cursor)) ==
                       CharClass::space) {
                cursor = previousCharBoundary(text, cursor);
            }

            if (cursor == 0) {
                return 0;
            }

            const auto targetClass =
                charClassAt(text, previousCharBoundary(text, cursor));
            while (cursor > 0) {
                const size_t prev = previousCharBoundary(text, cursor);
                if (charClassAt(text, prev) != targetClass) {
                    break;
                }
                cursor = prev;
            }
            return cursor;
        }

        size_t nextWordBoundary(std::string_view text, size_t cursor) {
            cursor = std::min(cursor, text.size());
            while (cursor < text.size() &&
                   charClassAt(text, cursor) == CharClass::space) {
                cursor = nextCharBoundary(text, cursor);
            }

            if (cursor >= text.size()) {
                return text.size();
            }

            const auto targetClass = charClassAt(text, cursor);
            while (cursor < text.size() &&
                   charClassAt(text, cursor) == targetClass) {
                cursor = nextCharBoundary(text, cursor);
            }
            return cursor;
        }

        size_t findVisibleStart(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            std::string_view text,
            size_t cursor,
            float maxWidth,
            const Core::Renderer::FontProps &fontProps) {
            cursor = std::min(cursor, text.size());
            if (cursor == 0 ||
                renderer->measureText(text.substr(0, cursor), fontProps).x <=
                    maxWidth) {
                return 0;
            }

            size_t low = 0;
            size_t high = cursor;
            while (low < high) {
                const size_t mid = (low + high) / 2;
                if (renderer
                        ->measureText(text.substr(mid, cursor - mid), fontProps)
                        .x <= maxWidth) {
                    high = mid;
                } else {
                    low = mid + 1;
                }
            }
            return nextStartBoundary(text, low);
        }

        size_t findVisibleEnd(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            std::string_view text,
            size_t start,
            size_t cursor,
            float maxWidth,
            const Core::Renderer::FontProps &fontProps) {
            size_t low = std::min(cursor, text.size());
            size_t high = text.size();
            while (low < high) {
                const size_t mid = (low + high + 1) / 2;
                if (renderer
                        ->measureText(text.substr(start, mid - start),
                                      fontProps)
                        .x <= maxWidth) {
                    low = mid;
                } else {
                    high = mid - 1;
                }
            }
            return low == text.size() ? low
                                      : previousCharBoundary(text, low + 1);
        }

        size_t cursorIndexForPointer(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            std::string_view text,
            size_t visibleStart,
            size_t visibleEnd,
            float pointerX,
            float left,
            const Core::Renderer::FontProps &fontProps) {
            visibleStart = std::min(visibleStart, text.size());
            visibleEnd = std::min(visibleEnd, text.size());
            if (visibleStart > visibleEnd) {
                std::swap(visibleStart, visibleEnd);
            }

            if (pointerX <= left || visibleStart == visibleEnd) {
                return visibleStart;
            }

            float x = left;
            size_t cursor = visibleStart;
            while (cursor < visibleEnd) {
                const size_t next = nextCharBoundary(text, cursor);
                const float charWidth =
                    renderer
                        ->measureText(text.substr(cursor, next - cursor),
                                      fontProps)
                        .x;
                if (pointerX < x + (charWidth * 0.5f)) {
                    return cursor;
                }

                x += charWidth;
                cursor = next;
            }
            return visibleEnd;
        }

        struct TextBoxFallbackPalette {
            Core::Renderer::Color surface;
            Core::Renderer::Color surfaceHover;
            Core::Renderer::Color surfaceActive;
            Core::Renderer::Color border;
            Core::Renderer::Color borderFocus;
            Core::Renderer::Color text;
            Core::Renderer::Color textMuted;
            Core::Renderer::Color accent;
        };

        constexpr TextBoxFallbackPalette kTextBoxFallbackPalette{
            .surface = Core::Renderer::Color::fromRGBA8(31, 34, 38),
            .surfaceHover = Core::Renderer::Color::fromRGBA8(39, 43, 48),
            .surfaceActive = Core::Renderer::Color::fromRGBA8(31, 34, 38),
            .border = Core::Renderer::Color::fromRGBA8(78, 84, 92),
            .borderFocus = Core::Renderer::Color::fromRGBA8(99, 151, 236),
            .text = Core::Renderer::Color::fromRGBA8(236, 239, 243),
            .textMuted = Core::Renderer::Color::fromRGBA8(154, 162, 173),
            .accent = Core::Renderer::Color::fromRGBA8(99, 151, 236),
        };

        Core::Renderer::Color
        colorOr(const std::optional<Core::Renderer::Color> &overrideColor,
                const Core::Renderer::Color &fallback) {
            return overrideColor.value_or(fallback);
        }
    } // namespace

    void TextBoxContext::syncExternalValue(std::string_view value,
                                           size_t maxLength) {
        m_maxLength = maxLength;
        if (m_focused) {
            return;
        }

        const auto next = boundedText(value, maxLength);
        if (next == m_text) {
            return;
        }

        m_text = next;
        m_cursorPos = m_text.size();
        clearSelection();
    }

    void TextBoxContext::replaceText(std::string_view value,
                                     size_t maxLength,
                                     bool preserveCursor) {
        m_maxLength = maxLength;
        const auto next = boundedText(value, maxLength);
        const size_t previousCursor = m_cursorPos;

        m_text = next;
        m_cursorPos = preserveCursor ? std::min(previousCursor, m_text.size())
                                     : m_text.size();
        clearSelection();
        clampCursor();
    }

    void TextBoxContext::focus(std::string_view value,
                               size_t maxLength,
                               bool selectAllOnFocus) {
        m_maxLength = maxLength;
        m_text = boundedText(value, maxLength);
        m_focusStartText = m_text;
        m_cursorPos = m_text.size();
        m_selectionAnchorPos = selectAllOnFocus ? 0 : m_cursorPos;
        m_focused = true;
        m_pointerSelecting = false;
        m_pointerInputQueued = false;
        m_pointerSelectionStarted = false;
        m_pointerExtendSelection = false;
    }

    void TextBoxContext::blur() {
        m_focused = false;
        m_pointerSelecting = false;
        m_pointerInputQueued = false;
        m_pointerSelectionStarted = false;
        m_pointerExtendSelection = false;
    }

    TextBoxContextResult TextBoxContext::handleEvent(const SceneEvent &evt) {
        TextBoxContextResult result;
        if (!m_focused) {
            return result;
        }

        result.handled = true;
        if (evt.type == SceneEvent::Type::textInput) {
            std::string utf8;
            const auto codepoint = evt.data.textInput.codepoint;
            if (codepoint < 0x20 || codepoint == 0x7F ||
                !appendUtf8(codepoint, utf8)) {
                return result;
            }

            clampCursor();
            if (textSizeAfterSelectionDelete() + utf8.size() > m_maxLength) {
                return result;
            }

            deleteSelection(result);
            m_text.insert(m_cursorPos, utf8);
            m_cursorPos += utf8.size();
            clearSelection();
            markChanged(result);
            return result;
        }

        if (evt.type != SceneEvent::Type::key) {
            result.handled = false;
            return result;
        }

        const auto &data = evt.data.keyPress;
        if (data.action != KeyAction::press && data.action != KeyAction::hold) {
            return result;
        }

        clampCursor();
        const bool commandPressed = evt.isCtrlPressed;
        const bool selecting = evt.isShiftPressed;

        if (commandPressed) {
            switch (data.keycode) {
            case KeyCode::a:
                m_selectionAnchorPos = 0;
                m_cursorPos = m_text.size();
                return result;
            case KeyCode::c:
                if (hasSelection()) {
                    const auto [start, end] = selectionRange();
                    g_textClipboard = m_text.substr(start, end - start);
                }
                return result;
            case KeyCode::x:
                if (hasSelection()) {
                    const auto [start, end] = selectionRange();
                    g_textClipboard = m_text.substr(start, end - start);
                    deleteSelection(result);
                }
                return result;
            case KeyCode::v:
                if (g_textClipboard.empty() ||
                    textSizeAfterSelectionDelete() >= m_maxLength) {
                    return result;
                }
                {
                    const size_t remaining =
                        m_maxLength - textSizeAfterSelectionDelete();
                    const size_t pasteSize =
                        g_textClipboard.size() <= remaining
                            ? g_textClipboard.size()
                            : utf8PrefixBoundary(g_textClipboard, remaining);
                    if (pasteSize == 0) {
                        return result;
                    }
                    deleteSelection(result);
                    m_text.insert(m_cursorPos,
                                  g_textClipboard.substr(0, pasteSize));
                    m_cursorPos += pasteSize;
                    clearSelection();
                    markChanged(result);
                }
                return result;
            default:
                break;
            }
        }

        switch (data.keycode) {
        case KeyCode::backspace: {
            if (deleteSelection(result) || m_cursorPos == 0) {
                return result;
            }

            const size_t eraseBegin =
                evt.isCtrlPressed ? previousWordBoundary(m_text, m_cursorPos)
                                  : previousCharBoundary(m_text, m_cursorPos);
            m_text.erase(eraseBegin, m_cursorPos - eraseBegin);
            m_cursorPos = eraseBegin;
            markChanged(result);
            return result;
        }
        case KeyCode::del: {
            if (deleteSelection(result) || m_cursorPos >= m_text.size()) {
                return result;
            }

            const size_t eraseEnd = evt.isCtrlPressed
                                        ? nextWordBoundary(m_text, m_cursorPos)
                                        : nextCharBoundary(m_text, m_cursorPos);
            m_text.erase(m_cursorPos, eraseEnd - m_cursorPos);
            markChanged(result);
            return result;
        }
        case KeyCode::arrowLeft:
            if (!selecting && hasSelection()) {
                moveCursor(selectionRange().first, false, result);
                return result;
            }
            moveCursor(commandPressed
                           ? previousWordBoundary(m_text, m_cursorPos)
                           : previousCharBoundary(m_text, m_cursorPos),
                       selecting,
                       result);
            return result;
        case KeyCode::arrowRight:
            if (!selecting && hasSelection()) {
                moveCursor(selectionRange().second, false, result);
                return result;
            }
            moveCursor(commandPressed ? nextWordBoundary(m_text, m_cursorPos)
                                      : nextCharBoundary(m_text, m_cursorPos),
                       selecting,
                       result);
            return result;
        case KeyCode::home:
            moveCursor(0, selecting, result);
            return result;
        case KeyCode::end:
            moveCursor(m_text.size(), selecting, result);
            return result;
        case KeyCode::enter:
            result.submitted = true;
            return result;
        case KeyCode::escape:
            if (m_text != m_focusStartText) {
                m_text = m_focusStartText;
                m_cursorPos = m_text.size();
                clearSelection();
                markChanged(result);
            }
            result.canceled = true;
            return result;
        case KeyCode::tab:
            return result;
        default:
            return result;
        }
    }

    void TextBoxContext::queuePointerPress(const glm::vec2 &pos,
                                           bool extendSelection) {
        m_pointerPos = pos;
        m_pointerInputQueued = true;
        m_pointerSelecting = true;
        m_pointerSelectionStarted = true;
        m_pointerExtendSelection = extendSelection;
    }

    void TextBoxContext::queuePointerMove(const glm::vec2 &pos) {
        if (!m_pointerSelecting) {
            return;
        }

        m_pointerPos = pos;
        m_pointerInputQueued = true;
    }

    void TextBoxContext::queuePointerRelease(const glm::vec2 &pos) {
        m_pointerPos = pos;
        m_pointerInputQueued = true;
        m_pointerSelecting = false;
    }

    bool TextBoxContext::hasPointerCapture() const {
        return m_pointerSelecting;
    }

    void TextBoxContext::updatePointerSelection(
        const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
        float left,
        float contentWidth,
        const Core::Renderer::FontProps &fontProps) {
        if (!m_pointerInputQueued || renderer == nullptr) {
            return;
        }

        const auto [visibleStart, visibleEnd] =
            visibleRangeForCursor(renderer, contentWidth, fontProps);
        const size_t nextCursor = cursorIndexForPointer(renderer,
                                                        m_text,
                                                        visibleStart,
                                                        visibleEnd,
                                                        m_pointerPos.x,
                                                        left,
                                                        fontProps);

        if (m_pointerSelectionStarted && !m_pointerExtendSelection) {
            m_selectionAnchorPos = nextCursor;
        }

        m_cursorPos = nextCursor;
        m_pointerSelectionStarted = false;
        m_pointerExtendSelection = false;
        m_pointerInputQueued = false;
        clampCursor();
    }

    std::pair<size_t, size_t> TextBoxContext::visibleRangeForCursor(
        const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
        float maxWidth,
        const Core::Renderer::FontProps &fontProps) const {
        if (renderer == nullptr || m_text.empty() || maxWidth <= 1.f) {
            return {0, 0};
        }

        if (renderer->measureText(m_text, fontProps).x <= maxWidth) {
            return {0, m_text.size()};
        }

        const size_t visibleStart = findVisibleStart(
            renderer, m_text, m_cursorPos, maxWidth, fontProps);
        const size_t visibleEnd = findVisibleEnd(
            renderer, m_text, visibleStart, m_cursorPos, maxWidth, fontProps);
        return {visibleStart, visibleEnd};
    }

    bool TextBoxContext::hasSelection() const {
        return m_cursorPos != m_selectionAnchorPos;
    }

    std::pair<size_t, size_t> TextBoxContext::selectionRange() const {
        return {
            std::min(m_cursorPos, m_selectionAnchorPos),
            std::max(m_cursorPos, m_selectionAnchorPos),
        };
    }

    void TextBoxContext::clampCursor() {
        m_cursorPos = std::min(m_cursorPos, m_text.size());
        m_selectionAnchorPos = std::min(m_selectionAnchorPos, m_text.size());
    }

    void TextBoxContext::clearSelection() {
        m_selectionAnchorPos = m_cursorPos;
    }

    void TextBoxContext::markChanged(TextBoxContextResult &result) {
        result.changed = true;
    }

    bool TextBoxContext::deleteSelection(TextBoxContextResult &result) {
        if (!hasSelection()) {
            return false;
        }

        const auto [start, end] = selectionRange();
        m_text.erase(start, end - start);
        m_cursorPos = start;
        clearSelection();
        markChanged(result);
        return true;
    }

    size_t TextBoxContext::textSizeAfterSelectionDelete() const {
        if (!hasSelection()) {
            return m_text.size();
        }

        const auto [start, end] = selectionRange();
        return m_text.size() - (end - start);
    }

    void TextBoxContext::moveCursor(size_t nextCursor,
                                    bool selecting,
                                    TextBoxContextResult &result) {
        (void)result;
        m_cursorPos = std::min(nextCursor, m_text.size());
        if (!selecting) {
            clearSelection();
        }
        clampCursor();
    }

    void drawTextBoxContext(const PickingId &id,
                            TextBoxContext &input,
                            const glm::vec3 &boxPos,
                            const glm::vec2 &boxSize,
                            SceneDrawContext &context,
                            const TextBoxContextDrawOptions &options) {
        if (context.renderer == nullptr) {
            return;
        }

        auto size = boxSize;
        const auto referenceTextSize =
            context.renderer->measureText("M", {.fontSize = options.fontSize});

        if (size.y == 0.f) {
            size.y = referenceTextSize.y + (options.padding.y * 2.f);
        }

        if (size.x == 0.f) {
            const auto displayText = input.text().empty()
                                         ? options.placeholder
                                         : std::string_view(input.text());
            const auto measuredText = context.renderer->measureText(
                displayText, {.fontSize = options.fontSize});
            size.x = std::max(48.f, measuredText.x + (options.padding.x * 2.f));
        }

        const auto &palette = kTextBoxFallbackPalette;
        const bool focused = input.isFocused();
        auto bgColor =
            focused
                ? colorOr(options.focusedBackgroundColor, palette.surfaceActive)
                : colorOr(options.backgroundColor, palette.surface);
        if (!focused && options.hovered) {
            bgColor =
                colorOr(options.hoverBackgroundColor, palette.surfaceHover);
        }

        const SceneDraw::QuadStyle style{
            .borderColor =
                focused
                    ? colorOr(options.focusedBorderColor, palette.borderFocus)
                    : colorOr(options.borderColor, palette.border),
            .borderRadius = glm::vec4(2.f),
            .borderSize = glm::vec4(focused ? 0.8f : 0.5f),
        };

        SceneDraw::drawQuad(context, boxPos, size, bgColor, id, style);

        const float contentWidth =
            std::max(1.f, size.x - (options.padding.x * 2.f));
        const float left = boxPos.x - (size.x * 0.5f) + options.padding.x;
        const Core::Renderer::FontProps fontProps{
            .fontSize = options.fontSize,
        };

        if (focused) {
            input.updatePointerSelection(
                context.renderer, left, contentWidth, fontProps);
        }

        const auto [visibleStart, visibleEnd] = input.visibleRangeForCursor(
            context.renderer, contentWidth, fontProps);
        std::string_view visibleText =
            std::string_view(input.text())
                .substr(visibleStart, visibleEnd - visibleStart);
        Core::Renderer::Color textColor =
            colorOr(options.textColor, palette.text);

        if (input.text().empty() && !options.placeholder.empty()) {
            visibleText = options.placeholder;
            textColor = colorOr(options.placeholderColor, palette.textMuted);
        }

        const float maxCursorHeight = std::max(4.f, size.y - 4.f);
        const float defaultCursorHeight =
            std::max(referenceTextSize.y, size.y - (options.padding.y * 2.f));
        const float cursorHeight =
            std::clamp(options.cursorHeight.value_or(defaultCursorHeight),
                       4.f,
                       maxCursorHeight);
        const float cursorWidth =
            std::clamp(options.cursorWidth.value_or(1.5f), 1.f, 4.f);
        if (focused && input.hasSelection()) {
            const auto [selStart, selEnd] = input.selectionRange();
            const size_t visibleSelStart = std::max(selStart, visibleStart);
            const size_t visibleSelEnd = std::min(selEnd, visibleEnd);

            if (visibleSelStart < visibleSelEnd) {
                const auto text = std::string_view(input.text());
                const auto prefix =
                    text.substr(visibleStart, visibleSelStart - visibleStart);
                const auto selected = text.substr(
                    visibleSelStart, visibleSelEnd - visibleSelStart);
                const float selectionX =
                    left + context.renderer->measureText(prefix, fontProps).x;
                const float selectionWidth = std::max(
                    1.f, context.renderer->measureText(selected, fontProps).x);
                SceneDraw::drawQuad(context,
                                    {selectionX + (selectionWidth * 0.5f),
                                     boxPos.y,
                                     boxPos.z + 0.00015f},
                                    {selectionWidth, cursorHeight},
                                    colorOr(options.selectionColor,
                                            palette.accent.withAlpha(0.45f)),
                                    id);
            }
        }

        const float textOffY = context.renderer->textCenterOffsetY(
            visibleText.empty() ? std::string_view("M") : visibleText,
            {.fontSize = options.fontSize});
        const glm::vec3 textPos{
            left,
            boxPos.y + textOffY,
            boxPos.z + 0.0001f,
        };
        SceneDraw::drawText(context,
                            visibleText,
                            textPos,
                            static_cast<size_t>(options.fontSize),
                            textColor,
                            id);

        if (focused) {
            const size_t visibleCursor =
                std::clamp(input.cursorPos(), visibleStart, visibleEnd);
            const auto cursorText =
                std::string_view(input.text())
                    .substr(visibleStart, visibleCursor - visibleStart);
            const float cursorX =
                left + context.renderer->measureText(cursorText, fontProps).x;
            SceneDraw::drawQuad(context,
                                {cursorX + (cursorWidth * 0.5f),
                                 boxPos.y,
                                 boxPos.z + 0.0002f},
                                {cursorWidth, cursorHeight},
                                colorOr(options.cursorColor, palette.text),
                                id);
        }
    }
} // namespace Bess::Canvas::UI
