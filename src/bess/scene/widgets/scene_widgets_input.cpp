#include "scene_widgets_internal.h"
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

        size_t previousWordBoundary(std::string_view text, size_t cursor) {
            cursor = std::min(cursor, text.size());
            while (cursor > 0 && text[cursor - 1] == ' ') {
                --cursor;
            }
            while (cursor > 0 && text[cursor - 1] != ' ') {
                --cursor;
            }
            return cursor;
        }

        size_t nextWordBoundary(std::string_view text, size_t cursor) {
            cursor = std::min(cursor, text.size());
            while (cursor < text.size() && text[cursor] == ' ') {
                ++cursor;
            }
            while (cursor < text.size() && text[cursor] != ' ') {
                ++cursor;
            }
            return cursor;
        }
    } // namespace

    bool handleTextInputKey(WidgetState &state, const SceneEvent &evt) {
        const auto &data = evt.data.keyPress;
        if (data.action != KeyAction::press &&
            data.action != KeyAction::hold) {
            return true;
        }

        clampCursor(state);

        switch (data.keycode) {
        case KeyCode::backspace: {
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
            if (state.cursorPos >= state.text.size()) {
                return true;
            }

            const size_t eraseEnd =
                evt.isCtrlPressed ? nextWordBoundary(state.text, state.cursorPos)
                                  : nextCharBoundary(state.text, state.cursorPos);
            state.text.erase(state.cursorPos, eraseEnd - state.cursorPos);
            markTextChanged(state);
            return true;
        }
        case KeyCode::arrowLeft:
            state.cursorPos =
                evt.isCtrlPressed
                    ? previousWordBoundary(state.text, state.cursorPos)
                    : previousCharBoundary(state.text, state.cursorPos);
            return true;
        case KeyCode::arrowRight:
            state.cursorPos =
                evt.isCtrlPressed
                    ? nextWordBoundary(state.text, state.cursorPos)
                    : nextCharBoundary(state.text, state.cursorPos);
            return true;
        case KeyCode::home:
            state.cursorPos = 0;
            return true;
        case KeyCode::end:
            state.cursorPos = state.text.size();
            return true;
        case KeyCode::enter:
            state.textSubmitted = true;
            return true;
        case KeyCode::escape:
            if (state.text != state.focusStartText) {
                state.text = state.focusStartText;
                state.cursorPos = state.text.size();
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
        if (codepoint < 0x20 || codepoint == 0x7F ||
            state.text.size() >= state.maxLength) {
            return true;
        }

        std::string utf8;
        if (!appendUtf8(codepoint, utf8) ||
            state.text.size() + utf8.size() > state.maxLength) {
            return true;
        }

        clampCursor(state);
        state.text.insert(state.cursorPos, utf8);
        state.cursorPos += utf8.size();
        markTextChanged(state);
        return true;
    }
} // namespace Bess::Canvas::SceneWidgets::Detail

namespace Bess::Canvas::SceneWidgets {
    void beginFrame(SceneState *sceneState) {
        Detail::getSceneState(sceneState).registeredWidgets.clear();
    }

    void endFrame(SceneState *sceneState) {
        auto widgetsState = Detail::findSceneState(sceneState);
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
            ++it;
        }
    }

    void clearScene(SceneState *sceneState) {
        Detail::sceneStates().erase(Detail::sceneKey(sceneState));
    }

    bool contains(const SceneState *sceneState, const PickingId &id) {
        if (!id.isValid()) {
            return false;
        }

        auto widgetsState = Detail::findSceneState(sceneState);
        return widgetsState != nullptr &&
               widgetsState->registeredWidgets.contains(id.toUint64());
    }

    bool isTextInput(const SceneState *sceneState, const PickingId &id) {
        auto widgetsState = Detail::findSceneState(sceneState);
        if (widgetsState == nullptr) {
            return false;
        }

        const auto widget = Detail::getWidgetState(*widgetsState, id);
        return widget != nullptr &&
               widget->type == Detail::WidgetState::Type::textInput;
    }

    bool hasPointerCapture(const SceneState *sceneState) {
        auto widgetsState = Detail::findSceneState(sceneState);
        return widgetsState != nullptr &&
               widgetsState->pressedWidgetId != Detail::kInvalidWidgetId;
    }

    bool wantsKeyboard(const SceneState *sceneState) {
        auto wantsKeyboardInState =
            [](const Detail::SceneWidgetsState &widgetsState) {
                if (widgetsState.focusedWidgetId == Detail::kInvalidWidgetId) {
                    return false;
                }

                const auto it = widgetsState.widgetStates.find(
                    widgetsState.focusedWidgetId);
                return it != widgetsState.widgetStates.end() &&
                       it->second.type ==
                           Detail::WidgetState::Type::textInput;
            };

        if (sceneState != nullptr) {
            auto widgetsState = Detail::findSceneState(sceneState);
            return widgetsState != nullptr &&
                   wantsKeyboardInState(*widgetsState);
        }

        for (const auto &[_, widgetsState] : Detail::sceneStates()) {
            if (wantsKeyboardInState(widgetsState)) {
                return true;
            }
        }
        return false;
    }

    void queuePress(SceneState *sceneState, const PickingId &id) {
        auto widgetsState = Detail::findSceneState(sceneState);
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

        if (widget->type == Detail::WidgetState::Type::textInput) {
            Detail::focusWidget(*widgetsState, id);
        } else {
            Detail::clearFocusState(*widgetsState);
        }
    }

    void queueRelease(SceneState *sceneState, const PickingId &id) {
        auto widgetsState = Detail::findSceneState(sceneState);
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

    void queueClick(SceneState *sceneState, const PickingId &id) {
        auto state = Detail::getWidgetState(sceneState, id);

        if (!state) {
            BESS_WARN("[SceneWidgets] Trying to queue click for "
                      "unregistered widget");
            return;
        }

        state->isClicked = true;
    }

    bool queueKey(SceneState *sceneState, const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::key &&
            evt.type != SceneEvent::Type::textInput) {
            return false;
        }

        auto widgetsState = Detail::findSceneState(sceneState);
        if (widgetsState == nullptr ||
            widgetsState->focusedWidgetId == Detail::kInvalidWidgetId) {
            return false;
        }

        auto state =
            widgetsState->widgetStates.find(widgetsState->focusedWidgetId);
        if (state == widgetsState->widgetStates.end() ||
            state->second.type != Detail::WidgetState::Type::textInput) {
            return false;
        }

        const bool handled =
            evt.type == SceneEvent::Type::key
                ? Detail::handleTextInputKey(state->second, evt)
                : Detail::handleTextInputCodepoint(
                      state->second, evt.data.textInput.codepoint);
        if (state->second.textSubmitted || state->second.textCanceled) {
            Detail::clearFocusState(*widgetsState);
        }
        return handled;
    }

    void clearFocus(SceneState *sceneState) {
        auto widgetsState = Detail::findSceneState(sceneState);
        if (widgetsState != nullptr) {
            Detail::clearFocusState(*widgetsState);
        }
    }

    void setHoverId(SceneState *sceneState, const PickingId &id) {
        auto widgetsState = Detail::findSceneState(sceneState);
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
