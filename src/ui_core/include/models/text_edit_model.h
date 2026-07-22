#pragma once

#include "common/bess_api.h"
#include "models/signal.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Bess::UI {

    struct TextSelection {
        size_t anchor = 0;
        size_t caret = 0;

        [[nodiscard]] size_t start() const noexcept {
            return std::min(anchor, caret);
        }

        [[nodiscard]] size_t end() const noexcept {
            return std::max(anchor, caret);
        }

        [[nodiscard]] bool collapsed() const noexcept {
            return anchor == caret;
        }

        bool operator==(const TextSelection &) const noexcept = default;
    };

    struct TextCompositionRange {
        size_t start = 0;
        size_t end = 0;

        bool operator==(const TextCompositionRange &) const noexcept = default;
    };

    enum class TextNavigationUnit : uint8_t { grapheme, word };
    enum class TextNavigationDirection : uint8_t { backward, forward };

    struct TextEditChange {
        TextSelection previousSelection;
        TextSelection selection;
        bool textChanged = false;
        bool selectionChanged = false;
        bool compositionChanged = false;
    };

    // Renderer-independent UTF-8 editing state. Every public byte offset is
    // normalized to an extended-grapheme boundary, so callers cannot split a
    // combining sequence, emoji ZWJ sequence, or regional-indicator pair.
    class BESS_API TextEditModel {
      public:
        using ChangedSignal = Signal<TextEditChange>;

        explicit TextEditModel(
            std::string text = {},
            size_t maximumBytes = std::numeric_limits<size_t>::max(),
            size_t historyLimit = 128);

        [[nodiscard]] const std::string &text() const noexcept;
        bool setText(std::string_view text, bool preserveSelection = false);

        [[nodiscard]] size_t maximumBytes() const noexcept;
        bool setMaximumBytes(size_t maximumBytes);
        [[nodiscard]] size_t historyLimit() const noexcept;
        void setHistoryLimit(size_t limit);

        [[nodiscard]] TextSelection selection() const noexcept;
        [[nodiscard]] bool hasSelection() const noexcept;
        [[nodiscard]] std::string selectedText() const;
        bool setSelection(size_t anchor, size_t caret);
        bool setCaret(size_t caret, bool extendSelection = false);
        bool selectAll();
        bool selectWordAt(size_t byteOffset);

        bool replaceSelection(std::string_view text);
        bool insertText(std::string_view text);
        bool insertCodepoint(char32_t codepoint);
        bool
        eraseBackward(TextNavigationUnit unit = TextNavigationUnit::grapheme);
        bool
        eraseForward(TextNavigationUnit unit = TextNavigationUnit::grapheme);
        bool moveCaret(TextNavigationDirection direction,
                       TextNavigationUnit unit = TextNavigationUnit::grapheme,
                       bool extendSelection = false);
        bool moveToStart(bool extendSelection = false);
        bool moveToEnd(bool extendSelection = false);

        [[nodiscard]] bool canUndo() const noexcept;
        [[nodiscard]] bool canRedo() const noexcept;
        bool undo();
        bool redo();
        void clearHistory() noexcept;

        [[nodiscard]] bool hasComposition() const noexcept;
        [[nodiscard]] std::optional<TextCompositionRange>
        compositionRange() const noexcept;
        bool beginComposition();
        bool updateComposition(std::string_view text,
                               size_t selectionStart,
                               size_t selectionLength);
        bool commitComposition();
        bool commitComposition(std::string_view text);
        bool cancelComposition();

        [[nodiscard]] ChangedSignal &changed() noexcept;

        [[nodiscard]] static bool isValidUtf8(std::string_view text) noexcept;
        [[nodiscard]] static size_t
        previousGraphemeBoundary(std::string_view text, size_t offset) noexcept;
        [[nodiscard]] static size_t
        nextGraphemeBoundary(std::string_view text, size_t offset) noexcept;
        [[nodiscard]] static size_t
        clampToGraphemeBoundary(std::string_view text, size_t offset) noexcept;

      private:
        struct Snapshot {
            std::string text;
            TextSelection selection;
        };

        [[nodiscard]] Snapshot snapshot() const;
        void restore(const Snapshot &snapshot);
        void emitChange(const Snapshot &before, bool compositionChanged);
        void recordUndo(Snapshot before);
        [[nodiscard]] std::string boundedInput(std::string_view text,
                                               size_t availableBytes) const;
        bool replaceSelectionInternal(std::string_view text);
        bool deleteSelectionInternal();
        [[nodiscard]] size_t previousWordBoundary(size_t offset) const noexcept;
        [[nodiscard]] size_t nextWordBoundary(size_t offset) const noexcept;
        void normalizeSelection() noexcept;
        bool
        commitCompositionInternal(std::optional<std::string_view> replacement);

        std::string m_text;
        TextSelection m_selection;
        size_t m_maximumBytes = std::numeric_limits<size_t>::max();
        size_t m_historyLimit = 128;
        std::vector<Snapshot> m_undo;
        std::vector<Snapshot> m_redo;
        std::optional<Snapshot> m_compositionBase;
        std::optional<TextCompositionRange> m_composition;
        ChangedSignal m_changed;
    };

} // namespace Bess::UI
