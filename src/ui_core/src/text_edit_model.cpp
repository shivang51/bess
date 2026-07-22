#include "models/text_edit_model.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace Bess::UI {
    namespace {
        struct Codepoint {
            char32_t value = 0;
            size_t begin = 0;
            size_t end = 0;
        };

        bool continuation(unsigned char byte) noexcept {
            return (byte & 0xC0u) == 0x80u;
        }

        bool decodeOne(std::string_view text,
                       size_t offset,
                       Codepoint &result) noexcept {
            if (offset >= text.size()) {
                return false;
            }
            const auto first = static_cast<unsigned char>(text[offset]);
            result.begin = offset;
            if (first <= 0x7Fu) {
                result.value = first;
                result.end = offset + 1;
                return true;
            }

            size_t count = 0;
            char32_t value = 0;
            char32_t minimum = 0;
            if (first >= 0xC2u && first <= 0xDFu) {
                count = 2;
                value = first & 0x1Fu;
                minimum = 0x80;
            } else if (first >= 0xE0u && first <= 0xEFu) {
                count = 3;
                value = first & 0x0Fu;
                minimum = 0x800;
            } else if (first >= 0xF0u && first <= 0xF4u) {
                count = 4;
                value = first & 0x07u;
                minimum = 0x10000;
            } else {
                return false;
            }
            if (offset + count > text.size()) {
                return false;
            }
            for (size_t index = 1; index < count; ++index) {
                const auto byte =
                    static_cast<unsigned char>(text[offset + index]);
                if (!continuation(byte)) {
                    return false;
                }
                value = (value << 6u) | (byte & 0x3Fu);
            }
            if (value < minimum || value > 0x10FFFF ||
                (value >= 0xD800 && value <= 0xDFFF)) {
                return false;
            }
            result.value = value;
            result.end = offset + count;
            return true;
        }

        bool appendUtf8(char32_t value, std::string &result) {
            if (value == 0 || value > 0x10FFFF ||
                (value >= 0xD800 && value <= 0xDFFF)) {
                return false;
            }
            if (value <= 0x7F) {
                result.push_back(static_cast<char>(value));
            } else if (value <= 0x7FF) {
                result.push_back(static_cast<char>(0xC0 | (value >> 6)));
                result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
            } else if (value <= 0xFFFF) {
                result.push_back(static_cast<char>(0xE0 | (value >> 12)));
                result.push_back(
                    static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
            } else {
                result.push_back(static_cast<char>(0xF0 | (value >> 18)));
                result.push_back(
                    static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
                result.push_back(
                    static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (value & 0x3F)));
            }
            return true;
        }

        std::string sanitizedUtf8(std::string_view text) {
            std::string result;
            result.reserve(text.size());
            size_t offset = 0;
            while (offset < text.size()) {
                Codepoint codepoint;
                if (decodeOne(text, offset, codepoint) &&
                    codepoint.value != 0) {
                    result.append(
                        text.substr(offset, codepoint.end - codepoint.begin));
                    offset = codepoint.end;
                } else {
                    static_cast<void>(appendUtf8(0xFFFD, result));
                    ++offset;
                }
            }
            return result;
        }

        std::vector<Codepoint> decode(std::string_view text) {
            std::vector<Codepoint> result;
            result.reserve(text.size());
            for (size_t offset = 0; offset < text.size();) {
                Codepoint codepoint;
                if (!decodeOne(text, offset, codepoint)) {
                    codepoint = {
                        .value = 0xFFFD, .begin = offset, .end = offset + 1};
                }
                result.push_back(codepoint);
                offset = codepoint.end;
            }
            return result;
        }

        bool in(char32_t value, char32_t first, char32_t last) noexcept {
            return value >= first && value <= last;
        }

        enum class GraphemeClass : uint8_t {
            other,
            cr,
            lf,
            control,
            extend,
            zwj,
            regionalIndicator,
            prepend,
            spacingMark,
            l,
            v,
            t,
            lv,
            lvt,
            extendedPictographic,
        };

        bool isExtend(char32_t c) noexcept {
            // Unicode combining marks, variation selectors, emoji modifiers,
            // and the combining half marks used by current desktop fonts.
            return in(c, 0x0300, 0x036F) || in(c, 0x0483, 0x0489) ||
                   in(c, 0x0591, 0x05BD) || c == 0x05BF ||
                   in(c, 0x05C1, 0x05C2) || in(c, 0x05C4, 0x05C5) ||
                   c == 0x05C7 || in(c, 0x0610, 0x061A) ||
                   in(c, 0x064B, 0x065F) || c == 0x0670 ||
                   in(c, 0x06D6, 0x06ED) || in(c, 0x0711, 0x0711) ||
                   in(c, 0x0730, 0x074A) || in(c, 0x07A6, 0x07B0) ||
                   in(c, 0x07EB, 0x07F3) || in(c, 0x0816, 0x082D) ||
                   in(c, 0x0859, 0x085B) || in(c, 0x08D3, 0x0902) ||
                   c == 0x093A || c == 0x093C || in(c, 0x0941, 0x0948) ||
                   c == 0x094D || in(c, 0x0951, 0x0957) ||
                   in(c, 0x0962, 0x0963) || in(c, 0x0981, 0x0981) ||
                   c == 0x09BC || in(c, 0x09C1, 0x09C4) || c == 0x09CD ||
                   in(c, 0x0A01, 0x0A02) || c == 0x0A3C ||
                   in(c, 0x0A41, 0x0A42) || in(c, 0x0A47, 0x0A48) ||
                   in(c, 0x0A4B, 0x0A4D) || in(c, 0x0A70, 0x0A71) ||
                   in(c, 0x0ABC, 0x0ABC) || in(c, 0x0AC1, 0x0AC8) ||
                   c == 0x0ACD || in(c, 0x0B01, 0x0B01) || c == 0x0B3C ||
                   c == 0x0B3F || in(c, 0x0B41, 0x0B44) || c == 0x0B4D ||
                   c == 0x0BCD || in(c, 0x0C00, 0x0C04) ||
                   in(c, 0x0C3E, 0x0C40) || in(c, 0x0C46, 0x0C48) ||
                   in(c, 0x0C4A, 0x0C4D) || in(c, 0x0C55, 0x0C56) ||
                   in(c, 0x0D00, 0x0D01) || in(c, 0x0D3B, 0x0D3C) ||
                   c == 0x0D4D || in(c, 0x0E31, 0x0E31) ||
                   in(c, 0x0E34, 0x0E3A) || in(c, 0x0E47, 0x0E4E) ||
                   in(c, 0x0EB1, 0x0EB1) || in(c, 0x0EB4, 0x0EBC) ||
                   in(c, 0x0EC8, 0x0ECE) || in(c, 0x0F18, 0x0F19) ||
                   in(c, 0x0F35, 0x0F35) || in(c, 0x0F37, 0x0F37) ||
                   in(c, 0x0F71, 0x0F84) || in(c, 0x0F86, 0x0F87) ||
                   in(c, 0x0F8D, 0x0FBC) || in(c, 0x102D, 0x1030) ||
                   in(c, 0x1032, 0x1037) || in(c, 0x1039, 0x103A) ||
                   in(c, 0x1058, 0x1059) || in(c, 0x135D, 0x135F) ||
                   in(c, 0x1712, 0x1714) || in(c, 0x1732, 0x1734) ||
                   in(c, 0x1752, 0x1753) || in(c, 0x1772, 0x1773) ||
                   in(c, 0x17B4, 0x17D3) || in(c, 0x180B, 0x180D) ||
                   in(c, 0x1AB0, 0x1AFF) || in(c, 0x1DC0, 0x1DFF) ||
                   in(c, 0x20D0, 0x20FF) || in(c, 0xFE00, 0xFE0F) ||
                   in(c, 0xFE20, 0xFE2F) || in(c, 0x1F3FB, 0x1F3FF) ||
                   in(c, 0xE0100, 0xE01EF);
        }

        bool isSpacingMark(char32_t c) noexcept {
            return c == 0x0903 || in(c, 0x093B, 0x0940) ||
                   in(c, 0x0949, 0x094C) || in(c, 0x0982, 0x0983) ||
                   in(c, 0x09BE, 0x09C0) || in(c, 0x09C7, 0x09C8) ||
                   in(c, 0x09CB, 0x09CC) || in(c, 0x0A3E, 0x0A40) ||
                   in(c, 0x0ABE, 0x0AC0) || in(c, 0x0AC9, 0x0AC9) ||
                   in(c, 0x0ACB, 0x0ACC) || in(c, 0x0B02, 0x0B03) ||
                   in(c, 0x0B3E, 0x0B40) || in(c, 0x0B47, 0x0B48) ||
                   in(c, 0x0B4B, 0x0B4C) || in(c, 0x0BBE, 0x0BC2) ||
                   in(c, 0x0BC6, 0x0BC8) || in(c, 0x0BCA, 0x0BCC) ||
                   in(c, 0x0C01, 0x0C03) || in(c, 0x0C41, 0x0C44) ||
                   in(c, 0x0C82, 0x0C83) || in(c, 0x0CBE, 0x0CC4) ||
                   in(c, 0x0CC7, 0x0CC8) || in(c, 0x0CCA, 0x0CCB) ||
                   in(c, 0x0D02, 0x0D03) || in(c, 0x0D3E, 0x0D40) ||
                   in(c, 0x0D46, 0x0D48) || in(c, 0x0D4A, 0x0D4C) ||
                   in(c, 0x0F3E, 0x0F3F) || in(c, 0x102B, 0x102C) ||
                   c == 0x1031 || in(c, 0x1038, 0x1038) ||
                   in(c, 0x17B6, 0x17C8) || in(c, 0x1923, 0x1926) ||
                   in(c, 0x1A19, 0x1A1B) || in(c, 0xA823, 0xA827);
        }

        GraphemeClass graphemeClass(char32_t c) noexcept {
            if (c == 0x0D)
                return GraphemeClass::cr;
            if (c == 0x0A)
                return GraphemeClass::lf;
            if (c == 0x200D)
                return GraphemeClass::zwj;
            if (in(c, 0x1F1E6, 0x1F1FF))
                return GraphemeClass::regionalIndicator;
            if (in(c, 0x1100, 0x115F) || in(c, 0xA960, 0xA97C))
                return GraphemeClass::l;
            if (in(c, 0x1160, 0x11A7) || in(c, 0xD7B0, 0xD7C6))
                return GraphemeClass::v;
            if (in(c, 0x11A8, 0x11FF) || in(c, 0xD7CB, 0xD7FB))
                return GraphemeClass::t;
            if (in(c, 0xAC00, 0xD7A3)) {
                return ((c - 0xAC00) % 28) == 0 ? GraphemeClass::lv
                                                : GraphemeClass::lvt;
            }
            if (isExtend(c))
                return GraphemeClass::extend;
            if (isSpacingMark(c))
                return GraphemeClass::spacingMark;
            if (in(c, 0x0600, 0x0605) || c == 0x06DD || c == 0x070F ||
                c == 0x08E2 || c == 0x110BD || c == 0x110CD)
                return GraphemeClass::prepend;
            if (c <= 0x001F || in(c, 0x007F, 0x009F) || in(c, 0x2028, 0x2029))
                return GraphemeClass::control;
            if (in(c, 0x1F000, 0x1FAFF) || in(c, 0x2300, 0x23FF) ||
                in(c, 0x2600, 0x27BF))
                return GraphemeClass::extendedPictographic;
            return GraphemeClass::other;
        }

        bool shouldBreak(const std::vector<Codepoint> &points,
                         size_t boundary) noexcept {
            const auto left = graphemeClass(points[boundary - 1].value);
            const auto right = graphemeClass(points[boundary].value);
            if (left == GraphemeClass::cr && right == GraphemeClass::lf)
                return false;
            const auto control = [](GraphemeClass value) {
                return value == GraphemeClass::cr ||
                       value == GraphemeClass::lf ||
                       value == GraphemeClass::control;
            };
            if (control(left) || control(right))
                return true;
            if (left == GraphemeClass::l &&
                (right == GraphemeClass::l || right == GraphemeClass::v ||
                 right == GraphemeClass::lv || right == GraphemeClass::lvt))
                return false;
            if ((left == GraphemeClass::lv || left == GraphemeClass::v) &&
                (right == GraphemeClass::v || right == GraphemeClass::t))
                return false;
            if ((left == GraphemeClass::lvt || left == GraphemeClass::t) &&
                right == GraphemeClass::t)
                return false;
            if (right == GraphemeClass::extend || right == GraphemeClass::zwj ||
                right == GraphemeClass::spacingMark)
                return false;
            if (left == GraphemeClass::prepend)
                return false;

            if (left == GraphemeClass::zwj &&
                right == GraphemeClass::extendedPictographic) {
                size_t index = boundary - 1;
                while (index > 0 && graphemeClass(points[index - 1].value) ==
                                        GraphemeClass::extend) {
                    --index;
                }
                if (index > 0 && graphemeClass(points[index - 1].value) ==
                                     GraphemeClass::extendedPictographic) {
                    return false;
                }
            }

            if (left == GraphemeClass::regionalIndicator &&
                right == GraphemeClass::regionalIndicator) {
                size_t preceding = 0;
                for (size_t index = boundary; index > 0; --index) {
                    if (graphemeClass(points[index - 1].value) !=
                        GraphemeClass::regionalIndicator)
                        break;
                    ++preceding;
                }
                return (preceding % 2) == 0;
            }
            return true;
        }

        std::vector<size_t> boundaries(std::string_view text) {
            const auto points = decode(text);
            std::vector<size_t> result{0};
            for (size_t index = 1; index < points.size(); ++index) {
                if (shouldBreak(points, index)) {
                    result.push_back(points[index].begin);
                }
            }
            if (result.back() != text.size()) {
                result.push_back(text.size());
            }
            return result;
        }

        enum class WordClass : uint8_t { whitespace, word, punctuation };

        WordClass wordClass(char32_t value) noexcept {
            if (value == ' ' || value == '\t' || value == '\r' ||
                value == '\n' || value == 0x00A0 || value == 0x1680 ||
                in(value, 0x2000, 0x200A) || value == 0x2028 ||
                value == 0x2029 || value == 0x202F || value == 0x205F ||
                value == 0x3000) {
                return WordClass::whitespace;
            }
            if (value <= 0x7F) {
                const auto ascii = static_cast<unsigned char>(value);
                return std::isalnum(ascii) != 0 || value == '_'
                           ? WordClass::word
                           : WordClass::punctuation;
            }
            // Non-ASCII scripts are treated as word text. Emoji and symbols
            // still remain individually navigable by grapheme commands.
            return WordClass::word;
        }

        char32_t codepointAt(std::string_view text, size_t offset) noexcept {
            Codepoint codepoint;
            return decodeOne(text, offset, codepoint) ? codepoint.value
                                                      : char32_t{0xFFFD};
        }
    } // namespace

    TextEditModel::TextEditModel(std::string text,
                                 size_t maximumBytes,
                                 size_t historyLimit)
        : m_maximumBytes(maximumBytes),
          m_historyLimit(historyLimit) {
        m_text = boundedInput(text, m_maximumBytes);
        m_selection = {m_text.size(), m_text.size()};
    }

    const std::string &TextEditModel::text() const noexcept {
        return m_text;
    }

    bool TextEditModel::setText(std::string_view text, bool preserveSelection) {
        const auto before = snapshot();
        m_composition.reset();
        m_compositionBase.reset();
        m_text = boundedInput(text, m_maximumBytes);
        if (preserveSelection) {
            normalizeSelection();
        } else {
            m_selection = {m_text.size(), m_text.size()};
        }
        clearHistory();
        emitChange(before, false);
        return before.text != m_text || before.selection != m_selection;
    }

    size_t TextEditModel::maximumBytes() const noexcept {
        return m_maximumBytes;
    }

    bool TextEditModel::setMaximumBytes(size_t maximumBytes) {
        if (m_maximumBytes == maximumBytes)
            return false;
        const auto before = snapshot();
        m_maximumBytes = maximumBytes;
        m_text = boundedInput(m_text, m_maximumBytes);
        m_composition.reset();
        m_compositionBase.reset();
        normalizeSelection();
        clearHistory();
        emitChange(before, false);
        return true;
    }

    size_t TextEditModel::historyLimit() const noexcept {
        return m_historyLimit;
    }

    void TextEditModel::setHistoryLimit(size_t limit) {
        m_historyLimit = limit;
        if (m_undo.size() > limit)
            m_undo.erase(m_undo.begin(), m_undo.end() - limit);
        if (m_redo.size() > limit)
            m_redo.erase(m_redo.begin(), m_redo.end() - limit);
    }

    TextSelection TextEditModel::selection() const noexcept {
        return m_selection;
    }

    bool TextEditModel::hasSelection() const noexcept {
        return !m_selection.collapsed();
    }

    std::string TextEditModel::selectedText() const {
        return m_text.substr(m_selection.start(),
                             m_selection.end() - m_selection.start());
    }

    bool TextEditModel::setSelection(size_t anchor, size_t caret) {
        if (hasComposition())
            static_cast<void>(commitComposition());
        const auto before = snapshot();
        m_selection = {
            clampToGraphemeBoundary(m_text, anchor),
            clampToGraphemeBoundary(m_text, caret),
        };
        emitChange(before, false);
        return before.selection != m_selection;
    }

    bool TextEditModel::setCaret(size_t caret, bool extendSelection) {
        return setSelection(extendSelection ? m_selection.anchor : caret,
                            caret);
    }

    bool TextEditModel::selectAll() {
        return setSelection(0, m_text.size());
    }

    bool TextEditModel::selectWordAt(size_t byteOffset) {
        if (m_text.empty())
            return setSelection(0, 0);
        size_t cursor = clampToGraphemeBoundary(m_text, byteOffset);
        if (cursor == m_text.size())
            cursor = previousGraphemeBoundary(m_text, cursor);
        const auto target = wordClass(codepointAt(m_text, cursor));
        size_t begin = cursor;
        while (begin > 0) {
            const size_t previous = previousGraphemeBoundary(m_text, begin);
            if (wordClass(codepointAt(m_text, previous)) != target)
                break;
            begin = previous;
        }
        size_t end = nextGraphemeBoundary(m_text, cursor);
        while (end < m_text.size() &&
               wordClass(codepointAt(m_text, end)) == target) {
            end = nextGraphemeBoundary(m_text, end);
        }
        return setSelection(begin, end);
    }

    bool TextEditModel::replaceSelection(std::string_view text) {
        if (hasComposition())
            static_cast<void>(commitComposition());
        const auto before = snapshot();
        if (!replaceSelectionInternal(text))
            return false;
        recordUndo(before);
        emitChange(before, false);
        return true;
    }

    bool TextEditModel::insertText(std::string_view text) {
        return replaceSelection(text);
    }

    bool TextEditModel::insertCodepoint(char32_t codepoint) {
        std::string encoded;
        return appendUtf8(codepoint, encoded) && insertText(encoded);
    }

    bool TextEditModel::eraseBackward(TextNavigationUnit unit) {
        if (hasComposition())
            static_cast<void>(commitComposition());
        const auto before = snapshot();
        if (!deleteSelectionInternal()) {
            if (m_selection.caret == 0)
                return false;
            const size_t begin =
                unit == TextNavigationUnit::word
                    ? previousWordBoundary(m_selection.caret)
                    : previousGraphemeBoundary(m_text, m_selection.caret);
            m_text.erase(begin, m_selection.caret - begin);
            m_selection = {begin, begin};
        }
        recordUndo(before);
        emitChange(before, false);
        return true;
    }

    bool TextEditModel::eraseForward(TextNavigationUnit unit) {
        if (hasComposition())
            static_cast<void>(commitComposition());
        const auto before = snapshot();
        if (!deleteSelectionInternal()) {
            if (m_selection.caret >= m_text.size())
                return false;
            const size_t end =
                unit == TextNavigationUnit::word
                    ? nextWordBoundary(m_selection.caret)
                    : nextGraphemeBoundary(m_text, m_selection.caret);
            m_text.erase(m_selection.caret, end - m_selection.caret);
        }
        recordUndo(before);
        emitChange(before, false);
        return true;
    }

    bool TextEditModel::moveCaret(TextNavigationDirection direction,
                                  TextNavigationUnit unit,
                                  bool extendSelection) {
        if (hasComposition())
            static_cast<void>(commitComposition());
        if (!extendSelection && hasSelection()) {
            return setSelection(direction == TextNavigationDirection::backward
                                    ? m_selection.start()
                                    : m_selection.end(),
                                direction == TextNavigationDirection::backward
                                    ? m_selection.start()
                                    : m_selection.end());
        }
        const size_t next =
            direction == TextNavigationDirection::backward
                ? (unit == TextNavigationUnit::word
                       ? previousWordBoundary(m_selection.caret)
                       : previousGraphemeBoundary(m_text, m_selection.caret))
                : (unit == TextNavigationUnit::word
                       ? nextWordBoundary(m_selection.caret)
                       : nextGraphemeBoundary(m_text, m_selection.caret));
        return setCaret(next, extendSelection);
    }

    bool TextEditModel::moveToStart(bool extendSelection) {
        return setCaret(0, extendSelection);
    }

    bool TextEditModel::moveToEnd(bool extendSelection) {
        return setCaret(m_text.size(), extendSelection);
    }

    bool TextEditModel::canUndo() const noexcept {
        return !m_undo.empty() || hasComposition();
    }

    bool TextEditModel::canRedo() const noexcept {
        return !m_redo.empty();
    }

    bool TextEditModel::undo() {
        if (hasComposition())
            return cancelComposition();
        if (m_undo.empty())
            return false;
        const auto before = snapshot();
        m_redo.push_back(before);
        if (m_redo.size() > m_historyLimit)
            m_redo.erase(m_redo.begin());
        const auto value = std::move(m_undo.back());
        m_undo.pop_back();
        restore(value);
        emitChange(before, false);
        return true;
    }

    bool TextEditModel::redo() {
        if (hasComposition() || m_redo.empty())
            return false;
        const auto before = snapshot();
        m_undo.push_back(before);
        if (m_undo.size() > m_historyLimit)
            m_undo.erase(m_undo.begin());
        const auto value = std::move(m_redo.back());
        m_redo.pop_back();
        restore(value);
        emitChange(before, false);
        return true;
    }

    void TextEditModel::clearHistory() noexcept {
        m_undo.clear();
        m_redo.clear();
    }

    bool TextEditModel::hasComposition() const noexcept {
        return m_composition.has_value();
    }

    std::optional<TextCompositionRange>
    TextEditModel::compositionRange() const noexcept {
        return m_composition;
    }

    bool TextEditModel::beginComposition() {
        if (hasComposition())
            return false;
        const auto before = snapshot();
        m_compositionBase = before;
        m_composition = {
            .start = m_selection.start(),
            .end = m_selection.end(),
        };
        emitChange(before, true);
        return true;
    }

    bool TextEditModel::updateComposition(std::string_view text,
                                          size_t selectionStart,
                                          size_t selectionLength) {
        if (!hasComposition())
            static_cast<void>(beginComposition());
        const auto before = snapshot();
        const size_t oldLength = m_composition->end - m_composition->start;
        const size_t available =
            m_maximumBytes -
            std::min(m_maximumBytes, m_text.size() - oldLength);
        const auto replacement = boundedInput(text, available);
        if (replacement.empty() && !text.empty())
            return false;
        m_text.replace(m_composition->start, oldLength, replacement);
        m_composition->end = m_composition->start + replacement.size();

        const size_t relativeAnchor =
            clampToGraphemeBoundary(replacement, selectionStart);
        const size_t relativeCaret = clampToGraphemeBoundary(
            replacement,
            std::min(replacement.size(), selectionStart + selectionLength));
        m_selection = {m_composition->start + relativeAnchor,
                       m_composition->start + relativeCaret};
        emitChange(before, true);
        return before.text != m_text || before.selection != m_selection;
    }

    bool TextEditModel::commitComposition() {
        return commitCompositionInternal(std::nullopt);
    }

    bool TextEditModel::commitComposition(std::string_view text) {
        return commitCompositionInternal(text);
    }

    bool TextEditModel::cancelComposition() {
        if (!m_compositionBase.has_value())
            return false;
        const auto before = snapshot();
        const auto base = std::move(*m_compositionBase);
        m_compositionBase.reset();
        m_composition.reset();
        restore(base);
        emitChange(before, true);
        return true;
    }

    TextEditModel::ChangedSignal &TextEditModel::changed() noexcept {
        return m_changed;
    }

    bool TextEditModel::isValidUtf8(std::string_view text) noexcept {
        size_t offset = 0;
        while (offset < text.size()) {
            Codepoint codepoint;
            if (!decodeOne(text, offset, codepoint) || codepoint.value == 0)
                return false;
            offset = codepoint.end;
        }
        return true;
    }

    size_t TextEditModel::previousGraphemeBoundary(std::string_view text,
                                                   size_t offset) noexcept {
        const auto values = boundaries(text);
        const auto it = std::lower_bound(
            values.begin(), values.end(), std::min(offset, text.size()));
        return it == values.begin() ? 0 : *std::prev(it);
    }

    size_t TextEditModel::nextGraphemeBoundary(std::string_view text,
                                               size_t offset) noexcept {
        const auto values = boundaries(text);
        const auto it = std::upper_bound(
            values.begin(), values.end(), std::min(offset, text.size()));
        return it == values.end() ? text.size() : *it;
    }

    size_t TextEditModel::clampToGraphemeBoundary(std::string_view text,
                                                  size_t offset) noexcept {
        const auto values = boundaries(text);
        const size_t clamped = std::min(offset, text.size());
        const auto it = std::upper_bound(values.begin(), values.end(), clamped);
        return it == values.begin() ? 0 : *std::prev(it);
    }

    TextEditModel::Snapshot TextEditModel::snapshot() const {
        return {.text = m_text, .selection = m_selection};
    }

    void TextEditModel::restore(const Snapshot &value) {
        m_text = value.text;
        m_selection = value.selection;
        normalizeSelection();
    }

    void TextEditModel::emitChange(const Snapshot &before,
                                   bool compositionChanged) {
        const bool textChanged = before.text != m_text;
        const bool selectionChanged = before.selection != m_selection;
        if (!textChanged && !selectionChanged && !compositionChanged)
            return;
        m_changed.emit({.previousSelection = before.selection,
                        .selection = m_selection,
                        .textChanged = textChanged,
                        .selectionChanged = selectionChanged,
                        .compositionChanged = compositionChanged});
    }

    void TextEditModel::recordUndo(Snapshot before) {
        if (before.text == m_text || m_historyLimit == 0)
            return;
        m_undo.push_back(std::move(before));
        if (m_undo.size() > m_historyLimit)
            m_undo.erase(m_undo.begin());
        m_redo.clear();
    }

    std::string TextEditModel::boundedInput(std::string_view text,
                                            size_t availableBytes) const {
        auto result = sanitizedUtf8(text);
        if (result.size() <= availableBytes)
            return result;
        result.resize(clampToGraphemeBoundary(result, availableBytes));
        return result;
    }

    bool TextEditModel::replaceSelectionInternal(std::string_view text) {
        const size_t removed = m_selection.end() - m_selection.start();
        const size_t used = m_text.size() - removed;
        const size_t available =
            used >= m_maximumBytes ? 0 : m_maximumBytes - used;
        const auto replacement = boundedInput(text, available);
        if (replacement.empty() && !text.empty())
            return false;
        const size_t begin = m_selection.start();
        m_text.replace(begin, removed, replacement);
        const size_t caret = begin + replacement.size();
        m_selection = {caret, caret};
        return removed > 0 || !replacement.empty();
    }

    bool TextEditModel::deleteSelectionInternal() {
        if (!hasSelection())
            return false;
        const size_t begin = m_selection.start();
        m_text.erase(begin, m_selection.end() - begin);
        m_selection = {begin, begin};
        return true;
    }

    size_t TextEditModel::previousWordBoundary(size_t offset) const noexcept {
        size_t cursor = clampToGraphemeBoundary(m_text, offset);
        while (cursor > 0) {
            const size_t previous = previousGraphemeBoundary(m_text, cursor);
            if (wordClass(codepointAt(m_text, previous)) !=
                WordClass::whitespace)
                break;
            cursor = previous;
        }
        if (cursor == 0)
            return 0;
        const auto target = wordClass(
            codepointAt(m_text, previousGraphemeBoundary(m_text, cursor)));
        while (cursor > 0) {
            const size_t previous = previousGraphemeBoundary(m_text, cursor);
            if (wordClass(codepointAt(m_text, previous)) != target)
                break;
            cursor = previous;
        }
        return cursor;
    }

    size_t TextEditModel::nextWordBoundary(size_t offset) const noexcept {
        size_t cursor = clampToGraphemeBoundary(m_text, offset);
        while (cursor < m_text.size() &&
               wordClass(codepointAt(m_text, cursor)) ==
                   WordClass::whitespace) {
            cursor = nextGraphemeBoundary(m_text, cursor);
        }
        if (cursor >= m_text.size())
            return m_text.size();
        const auto target = wordClass(codepointAt(m_text, cursor));
        while (cursor < m_text.size() &&
               wordClass(codepointAt(m_text, cursor)) == target) {
            cursor = nextGraphemeBoundary(m_text, cursor);
        }
        return cursor;
    }

    void TextEditModel::normalizeSelection() noexcept {
        m_selection.anchor =
            clampToGraphemeBoundary(m_text, m_selection.anchor);
        m_selection.caret = clampToGraphemeBoundary(m_text, m_selection.caret);
    }

    bool TextEditModel::commitCompositionInternal(
        std::optional<std::string_view> replacement) {
        if (!m_compositionBase.has_value() || !m_composition.has_value())
            return false;
        const auto before = snapshot();
        if (replacement.has_value()) {
            const size_t oldLength = m_composition->end - m_composition->start;
            const size_t used = m_text.size() - oldLength;
            const size_t available =
                used >= m_maximumBytes ? 0 : m_maximumBytes - used;
            const auto value = boundedInput(*replacement, available);
            if (value.empty() && !replacement->empty()) {
                const auto base = std::move(*m_compositionBase);
                m_compositionBase.reset();
                m_composition.reset();
                restore(base);
                emitChange(before, true);
                return true;
            }
            m_text.replace(m_composition->start, oldLength, value);
            const size_t caret = m_composition->start + value.size();
            m_selection = {caret, caret};
        } else {
            m_selection = {m_composition->end, m_composition->end};
        }
        const auto base = std::move(*m_compositionBase);
        m_compositionBase.reset();
        m_composition.reset();
        recordUndo(base);
        emitChange(before, true);
        return true;
    }

} // namespace Bess::UI
