#include "bess_core/scene/widgets/scene_widgets_internal.h"
#include "common/logger.h"
#include <algorithm>
#include <string_view>

namespace Bess::Canvas::SceneWidgets::Detail {
    namespace {
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

        size_t utf8PrefixBoundary(std::string_view text, size_t maxBytes) {
            maxBytes = std::min(maxBytes, text.size());
            while (maxBytes > 0 && maxBytes < text.size() &&
                   isUtf8Continuation(text[maxBytes])) {
                --maxBytes;
            }
            return maxBytes;
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

        bool isCommandPressed(const SceneEvent &evt) {
            return evt.isCtrlPressed;
        }

        bool deleteSelection(WidgetState &state) {
            if (!hasTextSelection(state)) {
                return false;
            }

            const auto [selStart, selEnd] = textSelectionRange(state);
            state.text.erase(selStart, selEnd - selStart);
            state.cursorPos = selStart;
            clearTextSelection(state);
            markTextChanged(state);
            return true;
        }

        size_t textSizeAfterSelectionDelete(const WidgetState &state) {
            if (!hasTextSelection(state)) {
                return state.text.size();
            }

            const auto [selStart, selEnd] = textSelectionRange(state);
            return state.text.size() - (selEnd - selStart);
        }

        void moveCursor(WidgetState &state, size_t nextCursor, bool selecting) {
            state.cursorPos = std::min(nextCursor, state.text.size());
            if (!selecting) {
                clearTextSelection(state);
            }
            clampCursor(state);
        }

        std::string selectedText(const WidgetState &state) {
            if (!hasTextSelection(state)) {
                return {};
            }

            const auto [selStart, selEnd] = textSelectionRange(state);
            return state.text.substr(selStart, selEnd - selStart);
        }

    } // namespace

    bool handleTextInputKey(SceneWidgetsState &widgetsState,
                            WidgetState &state,
                            const SceneEvent &evt) {
        const auto &data = evt.data.keyPress;
        if (data.action != KeyAction::press && data.action != KeyAction::hold) {
            return true;
        }

        clampCursor(state);
        const bool commandPressed = isCommandPressed(evt);
        const bool selecting = evt.isShiftPressed;

        if (commandPressed) {
            switch (data.keycode) {
            case KeyCode::a:
                state.selectionAnchorPos = 0;
                state.cursorPos = state.text.size();
                return true;
            case KeyCode::c:
                if (hasTextSelection(state)) {
                    widgetsState.textClipboard = selectedText(state);
                }
                return true;
            case KeyCode::x:
                if (hasTextSelection(state)) {
                    widgetsState.textClipboard = selectedText(state);
                    deleteSelection(state);
                }
                return true;
            case KeyCode::v:
                if (widgetsState.textClipboard.empty()) {
                    return true;
                }
                if (textSizeAfterSelectionDelete(state) >= state.maxLength) {
                    return true;
                }
                {
                    const size_t remaining =
                        state.maxLength - textSizeAfterSelectionDelete(state);
                    const std::string_view clipboard =
                        widgetsState.textClipboard;
                    const size_t pasteSize =
                        clipboard.size() <= remaining
                            ? clipboard.size()
                            : utf8PrefixBoundary(clipboard, remaining);
                    if (pasteSize == 0) {
                        return true;
                    }
                    deleteSelection(state);
                    state.text.insert(state.cursorPos,
                                      clipboard.substr(0, pasteSize));
                    state.cursorPos += pasteSize;
                    clearTextSelection(state);
                    markTextChanged(state);
                }
                return true;
            default:
                break;
            }
        }

        switch (data.keycode) {
        case KeyCode::backspace: {
            if (deleteSelection(state)) {
                return true;
            }

            if (state.cursorPos == 0) {
                return true;
            }

            const size_t eraseBegin =
                evt.isCtrlPressed
                    ? previousWordBoundary(state.text, state.cursorPos)
                    : previousCharBoundary(state.text, state.cursorPos);
            state.text.erase(eraseBegin, state.cursorPos - eraseBegin);
            state.cursorPos = eraseBegin;
            markTextChanged(state);
            return true;
        }
        case KeyCode::del: {
            if (deleteSelection(state)) {
                return true;
            }

            if (state.cursorPos >= state.text.size()) {
                return true;
            }

            const size_t eraseEnd =
                evt.isCtrlPressed
                    ? nextWordBoundary(state.text, state.cursorPos)
                    : nextCharBoundary(state.text, state.cursorPos);
            state.text.erase(state.cursorPos, eraseEnd - state.cursorPos);
            markTextChanged(state);
            return true;
        }
        case KeyCode::arrowLeft:
            if (!selecting && hasTextSelection(state)) {
                moveCursor(state, textSelectionRange(state).first, false);
                return true;
            }
            moveCursor(state,
                       commandPressed
                           ? previousWordBoundary(state.text, state.cursorPos)
                           : previousCharBoundary(state.text, state.cursorPos),
                       selecting);
            return true;
        case KeyCode::arrowRight:
            if (!selecting && hasTextSelection(state)) {
                moveCursor(state, textSelectionRange(state).second, false);
                return true;
            }
            moveCursor(state,
                       commandPressed
                           ? nextWordBoundary(state.text, state.cursorPos)
                           : nextCharBoundary(state.text, state.cursorPos),
                       selecting);
            return true;
        case KeyCode::home:
            moveCursor(state, 0, selecting);
            return true;
        case KeyCode::end:
            moveCursor(state, state.text.size(), selecting);
            return true;
        case KeyCode::enter:
            state.textSubmitted = true;
            return true;
        case KeyCode::escape:
            if (state.text != state.focusStartText) {
                state.text = state.focusStartText;
                state.cursorPos = state.text.size();
                clearTextSelection(state);
                markTextChanged(state);
            }
            state.textCanceled = true;
            return true;
        case KeyCode::tab:
            return true;
        default:
            return true;
        }
    }

    bool handleTextInputCodepoint(WidgetState &state, char32_t codepoint) {
        if (codepoint < 0x20 || codepoint == 0x7F) {
            return true;
        }

        std::string utf8;
        if (!appendUtf8(codepoint, utf8)) {
            return true;
        }

        clampCursor(state);
        if (textSizeAfterSelectionDelete(state) + utf8.size() >
            state.maxLength) {
            return true;
        }

        deleteSelection(state);
        state.text.insert(state.cursorPos, utf8);
        state.cursorPos += utf8.size();
        clearTextSelection(state);
        markTextChanged(state);
        return true;
    }

    bool handleSliderKey(WidgetState &state, const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::key) {
            return false;
        }

        const auto &data = evt.data.keyPress;
        if (data.action != KeyAction::press && data.action != KeyAction::hold) {
            return false;
        }

        switch (data.keycode) {
        case KeyCode::arrowLeft:
        case KeyCode::arrowDown:
            --state.sliderKeyboardDelta;
            return true;
        case KeyCode::arrowRight:
        case KeyCode::arrowUp:
            ++state.sliderKeyboardDelta;
            return true;
        case KeyCode::pageDown:
            state.sliderKeyboardDelta -= 10;
            return true;
        case KeyCode::pageUp:
            state.sliderKeyboardDelta += 10;
            return true;
        case KeyCode::home:
            state.sliderSetToMin = true;
            return true;
        case KeyCode::end:
            state.sliderSetToMax = true;
            return true;
        case KeyCode::escape:
            return true;
        default:
            return false;
        }
    }

    bool handleDropdownKey(SceneWidgetsState &widgetsState,
                           WidgetState &state,
                           const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::key) {
            return false;
        }

        const auto &data = evt.data.keyPress;
        if (data.action != KeyAction::press && data.action != KeyAction::hold) {
            return false;
        }

        auto openDropdown = [&]() {
            if (!state.dropdownOpen) {
                state.dropdownOpen = true;
                state.dropdownOpened = true;
                closeDropdowns(widgetsState, widgetsState.focusedWidgetId);
            }
            ensureDropdownHighlightVisible(state);
        };

        auto moveHighlight = [&](int delta) {
            if (state.dropdownOptionCount == 0) {
                return;
            }

            const auto count = static_cast<int>(state.dropdownOptionCount);
            auto next =
                static_cast<int>(state.dropdownHighlightedIndex) + delta;
            next = ((next % count) + count) % count;
            state.dropdownHighlightedIndex = static_cast<size_t>(next);
            ensureDropdownHighlightVisible(state);
        };

        switch (data.keycode) {
        case KeyCode::space:
        case KeyCode::enter:
            if (state.dropdownOpen) {
                if (state.dropdownOptionCount > 0) {
                    state.pendingDropdownSelection =
                        static_cast<int>(state.dropdownHighlightedIndex);
                }
                state.dropdownOpen = false;
                state.dropdownClosed = true;
            } else {
                openDropdown();
            }
            return true;
        case KeyCode::arrowDown:
            openDropdown();
            moveHighlight(1);
            return true;
        case KeyCode::arrowUp:
            openDropdown();
            moveHighlight(-1);
            return true;
        case KeyCode::home:
            openDropdown();
            state.dropdownHighlightedIndex = 0;
            ensureDropdownHighlightVisible(state);
            return true;
        case KeyCode::end:
            openDropdown();
            if (state.dropdownOptionCount > 0) {
                state.dropdownHighlightedIndex = state.dropdownOptionCount - 1;
            }
            ensureDropdownHighlightVisible(state);
            return true;
        case KeyCode::escape:
            if (state.dropdownOpen) {
                state.dropdownOpen = false;
                state.dropdownClosed = true;
            }
            return true;
        default:
            return false;
        }
    }
} // namespace Bess::Canvas::SceneWidgets::Detail

namespace Bess::Canvas::SceneWidgets {
    void beginFrame(SceneWidgetsState *widgetsState) {
        if (widgetsState == nullptr) {
            return;
        }

        widgetsState->registeredWidgets.clear();
    }

    void endFrame(SceneWidgetsState *widgetsState) {
        if (widgetsState == nullptr) {
            return;
        }

        for (auto it = widgetsState->widgetStates.begin();
             it != widgetsState->widgetStates.end();) {
            if (!widgetsState->registeredWidgets.contains(it->first)) {
                if (widgetsState->hoveredWidgetId == it->first) {
                    widgetsState->hoveredWidgetId = Detail::kInvalidWidgetId;
                }
                if (widgetsState->pressedWidgetId == it->first) {
                    widgetsState->pressedWidgetId = Detail::kInvalidWidgetId;
                }
                if (widgetsState->focusedWidgetId == it->first) {
                    widgetsState->focusedWidgetId = Detail::kInvalidWidgetId;
                }
                it = widgetsState->widgetStates.erase(it);
                continue;
            }

            auto &widget = it->second;
            widget.isHovered = widgetsState->hoveredWidgetId == it->first;
            widget.isPressed = widgetsState->pressedWidgetId == it->first;
            widget.isFocused = widgetsState->focusedWidgetId == it->first;
            widget.isClicked = false;
            widget.textChanged = false;
            widget.textSubmitted = false;
            widget.textCanceled = false;
            widget.focusStarted = false;
            widget.pointerInputQueued = false;
            widget.textPointerSelectionStarted = false;
            widget.textPointerExtendSelection = false;
            widget.sliderKeyboardDelta = 0;
            widget.sliderSetToMin = false;
            widget.sliderSetToMax = false;
            widget.dropdownOpened = false;
            widget.dropdownClosed = false;
            widget.pendingDropdownSelection = -1;
            ++it;
        }
    }

    bool contains(const SceneWidgetsState *widgetsState, const PickingId &id) {
        if (!id.isValid()) {
            return false;
        }

        return widgetsState != nullptr &&
               widgetsState->registeredWidgets.contains(id.toUint64());
    }

    bool isTextInput(const SceneWidgetsState *widgetsState,
                     const PickingId &id) {
        if (widgetsState == nullptr) {
            return false;
        }

        const auto widget = Detail::getWidgetState(*widgetsState, id);
        return widget != nullptr &&
               widget->type == Detail::WidgetState::Type::textInput;
    }

    bool hasPointerCapture(const SceneWidgetsState *widgetsState) {
        return widgetsState != nullptr &&
               widgetsState->pressedWidgetId != Detail::kInvalidWidgetId;
    }

    bool wantsKeyboard(const SceneWidgetsState *widgetsState) {
        auto wantsKeyboardInState =
            [](const Detail::SceneWidgetsState &widgetsState) {
                if (widgetsState.focusedWidgetId == Detail::kInvalidWidgetId) {
                    return false;
                }

                const auto it = widgetsState.widgetStates.find(
                    widgetsState.focusedWidgetId);
                if (it == widgetsState.widgetStates.end()) {
                    return false;
                }

                const auto &widget = it->second;
                return widget.type == Detail::WidgetState::Type::textInput ||
                       widget.type == Detail::WidgetState::Type::slider ||
                       widget.type == Detail::WidgetState::Type::dropdown;
            };

        return widgetsState != nullptr && wantsKeyboardInState(*widgetsState);
    }

    void queuePointerMove(SceneWidgetsState *widgetsState,
                          const glm::vec2 &pos) {
        if (widgetsState == nullptr ||
            widgetsState->pressedWidgetId == Detail::kInvalidWidgetId) {
            return;
        }

        auto pressed =
            widgetsState->widgetStates.find(widgetsState->pressedWidgetId);
        if (pressed == widgetsState->widgetStates.end()) {
            return;
        }

        pressed->second.pointerPos = pos;
        pressed->second.pointerInputQueued = true;
    }

    void queuePress(SceneWidgetsState *widgetsState,
                    const PickingId &id,
                    const glm::vec2 &pos,
                    bool extendSelection) {
        if (widgetsState == nullptr) {
            BESS_WARN("[SceneWidgets] Trying to press widget before scene "
                      "widgets were registered");
            return;
        }

        auto widget = Detail::getWidgetState(*widgetsState, id);
        if (widget == nullptr) {
            BESS_WARN("[SceneWidgets] Trying to press unregistered widget");
            return;
        }

        widgetsState->pressedWidgetId = id.toUint64();
        widget->isPressed = true;
        widget->pointerPos = pos;
        widget->pointerInputQueued = true;
        if (widget->type == Detail::WidgetState::Type::textInput) {
            widget->textPointerSelecting = true;
            widget->textPointerSelectionStarted = true;
            widget->textPointerExtendSelection = extendSelection;
        }

        if (widget->type == Detail::WidgetState::Type::dropdownOption) {
            Detail::closeDropdowns(*widgetsState, widget->ownerWidgetId);
            if (widget->ownerWidgetId != Detail::kInvalidWidgetId) {
                Detail::focusWidget(
                    *widgetsState,
                    PickingId::fromUint64(widget->ownerWidgetId));
            }
            return;
        }

        const uint64_t keepOpenWidgetId =
            widget->type == Detail::WidgetState::Type::dropdown
                ? id.toUint64()
                : Detail::kInvalidWidgetId;
        Detail::closeDropdowns(*widgetsState, keepOpenWidgetId);

        const bool keyboardFocusable =
            widget->type == Detail::WidgetState::Type::textInput ||
            widget->type == Detail::WidgetState::Type::slider ||
            widget->type == Detail::WidgetState::Type::dropdown;
        if (keyboardFocusable) {
            Detail::focusWidget(*widgetsState, id);
        } else {
            Detail::clearFocusState(*widgetsState);
        }
    }

    void queueRelease(SceneWidgetsState *widgetsState,
                      const PickingId &id,
                      const glm::vec2 &pos) {
        if (widgetsState == nullptr) {
            return;
        }

        const auto pressedWidgetId = widgetsState->pressedWidgetId;
        if (pressedWidgetId == Detail::kInvalidWidgetId) {
            return;
        }

        if (auto pressed = widgetsState->widgetStates.find(pressedWidgetId);
            pressed != widgetsState->widgetStates.end()) {
            pressed->second.isPressed = false;
            pressed->second.pointerPos = pos;
            pressed->second.pointerInputQueued = true;
        }

        widgetsState->pressedWidgetId = Detail::kInvalidWidgetId;

        if (id.toUint64() != pressedWidgetId ||
            !widgetsState->registeredWidgets.contains(pressedWidgetId)) {
            return;
        }

        auto widget = Detail::getWidgetState(*widgetsState, id);
        if (widget != nullptr) {
            widget->isClicked = true;
        }
    }

    void queueClick(SceneWidgetsState *widgetsState, const PickingId &id) {
        auto state = Detail::getWidgetState(widgetsState, id);

        if (!state) {
            BESS_WARN("[SceneWidgets] Trying to queue click for "
                      "unregistered widget");
            return;
        }

        state->isClicked = true;
    }

    bool queueKey(SceneWidgetsState *widgetsState, const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::key &&
            evt.type != SceneEvent::Type::textInput) {
            return false;
        }

        if (widgetsState == nullptr ||
            widgetsState->focusedWidgetId == Detail::kInvalidWidgetId) {
            return false;
        }

        auto state =
            widgetsState->widgetStates.find(widgetsState->focusedWidgetId);
        if (state == widgetsState->widgetStates.end()) {
            return false;
        }

        bool handled = false;
        switch (state->second.type) {
        case Detail::WidgetState::Type::textInput:
            handled = evt.type == SceneEvent::Type::key
                          ? Detail::handleTextInputKey(
                                *widgetsState, state->second, evt)
                          : Detail::handleTextInputCodepoint(
                                state->second, evt.data.textInput.codepoint);
            if (state->second.textSubmitted || state->second.textCanceled) {
                Detail::clearFocusState(*widgetsState);
            }
            return handled;
        case Detail::WidgetState::Type::slider:
            if (evt.type == SceneEvent::Type::key &&
                evt.data.keyPress.keycode == KeyCode::escape) {
                Detail::clearFocusState(*widgetsState);
                return true;
            }
            return Detail::handleSliderKey(state->second, evt);
        case Detail::WidgetState::Type::dropdown:
            handled =
                Detail::handleDropdownKey(*widgetsState, state->second, evt);
            if (evt.type == SceneEvent::Type::key &&
                evt.data.keyPress.keycode == KeyCode::escape &&
                !state->second.dropdownOpen) {
                Detail::clearFocusState(*widgetsState);
            }
            return handled;
        case Detail::WidgetState::Type::unknown:
        case Detail::WidgetState::Type::toggleButton:
        case Detail::WidgetState::Type::button:
        case Detail::WidgetState::Type::dropdownOption:
            return false;
        }

        return false;
    }

    bool queueWheel(SceneWidgetsState *widgetsState, const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::mouseWheel) {
            return false;
        }

        if (widgetsState == nullptr) {
            return false;
        }

        auto widget = Detail::getWidgetState(*widgetsState, evt.pickingId);
        if (widget == nullptr) {
            return false;
        }

        if (widget->type == Detail::WidgetState::Type::dropdownOption &&
            widget->ownerWidgetId != Detail::kInvalidWidgetId) {
            widget = Detail::getWidgetState(
                *widgetsState, PickingId::fromUint64(widget->ownerWidgetId));
        }

        if (widget == nullptr ||
            widget->type != Detail::WidgetState::Type::dropdown ||
            !widget->dropdownOpen || widget->dropdownOptionCount == 0 ||
            widget->dropdownMaxVisibleOptions == 0 ||
            widget->dropdownOptionCount <= widget->dropdownMaxVisibleOptions) {
            return false;
        }

        const int delta = evt.data.mouseWheel.delta.y > 0.f ? -1 : 1;
        const auto maxOffset =
            widget->dropdownOptionCount - widget->dropdownMaxVisibleOptions;
        const int nextOffset =
            std::clamp(static_cast<int>(widget->dropdownScrollOffset) + delta,
                       0,
                       static_cast<int>(maxOffset));
        widget->dropdownScrollOffset = static_cast<size_t>(nextOffset);
        return true;
    }

    void clearFocus(SceneWidgetsState *widgetsState) {
        if (widgetsState != nullptr) {
            Detail::closeDropdowns(*widgetsState);
            Detail::clearFocusState(*widgetsState);
        }
    }

    void setHoverId(SceneWidgetsState *widgetsState, const PickingId &id) {
        if (widgetsState == nullptr) {
            return;
        }

        if (widgetsState->hoveredWidgetId == id.toUint64()) {
            return;
        }

        if (widgetsState->hoveredWidgetId != Detail::kInvalidWidgetId) {
            auto prevState =
                widgetsState->widgetStates.find(widgetsState->hoveredWidgetId);
            if (prevState != widgetsState->widgetStates.end()) {
                prevState->second.isHovered = false;
            }
        }

        widgetsState->hoveredWidgetId = id.toUint64();

        if (id.isValid()) {
            auto newState = Detail::getWidgetState(*widgetsState, id);
            if (newState) {
                newState->isHovered = true;
            }
        }
    }
} // namespace Bess::Canvas::SceneWidgets
