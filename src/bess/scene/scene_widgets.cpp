#include "scene_widgets.h"
#include "bess_core/renderer/colors.h"
#include "bess_core/renderer/renderer_types.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "common/types.h"
#include "scene/scene_draw_helpers.h"
#include "scene/scene_event.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace Bess::Canvas::SceneWidgets {
    namespace {
        constexpr uint64_t kInvalidWidgetId =
            PickingId::invalid().toUint64();
        constexpr float kDefaultButtonTextSize = 8.f;

        struct WidgetState {
            enum class Type : uint8_t {
                unknown,
                toggleButton,
                button,
                textInput,
            };

            Type type = Type::unknown;
            bool isHovered = false;
            bool isPressed = false;
            bool isClicked = false;
            bool isFocused = false;

            std::string text;
            std::string focusStartText;
            size_t cursorPos = 0;
            size_t maxLength = 256;
            bool textInitialized = false;
            bool focusStarted = false;
            bool textChanged = false;
            bool textSubmitted = false;
            bool textCanceled = false;
        };

        struct SceneWidgetsState {
            std::unordered_set<uint64_t> registeredWidgets;
            std::unordered_map<uint64_t, WidgetState> widgetStates;
            uint64_t hoveredWidgetId = kInvalidWidgetId;
            uint64_t pressedWidgetId = kInvalidWidgetId;
            uint64_t focusedWidgetId = kInvalidWidgetId;
        };

        std::unordered_map<uint64_t, SceneWidgetsState> &sceneStates() {
            static std::unordered_map<uint64_t, SceneWidgetsState> states;
            return states;
        }

        uint64_t sceneKey(const SceneState *sceneState) {
            if (sceneState == nullptr) {
                return 0;
            }
            return static_cast<uint64_t>(sceneState->getSceneId());
        }

        SceneWidgetsState *findSceneState(const SceneState *sceneState) {
            const auto key = sceneKey(sceneState);
            auto &states = sceneStates();
            const auto it = states.find(key);
            if (it == states.end()) {
                return nullptr;
            }
            return &it->second;
        }

        SceneWidgetsState &getSceneState(SceneState *sceneState) {
            return sceneStates()[sceneKey(sceneState)];
        }

        WidgetState *getWidgetState(SceneWidgetsState &widgetsState,
                                    const PickingId &id) {
            const auto it = widgetsState.widgetStates.find(id.toUint64());
            if (it == widgetsState.widgetStates.end()) {
                return nullptr;
            }
            return &it->second;
        }

        const WidgetState *getWidgetState(const SceneWidgetsState &widgetsState,
                                          const PickingId &id) {
            const auto it = widgetsState.widgetStates.find(id.toUint64());
            if (it == widgetsState.widgetStates.end()) {
                return nullptr;
            }
            return &it->second;
        }

        WidgetState *getWidgetState(const SceneState *sceneState,
                                    const PickingId &id) {
            auto widgetsState = findSceneState(sceneState);
            if (widgetsState == nullptr) {
                return nullptr;
            }
            return getWidgetState(*widgetsState, id);
        }

        WidgetState *registerWidget(SceneState *sceneState,
                                    const PickingId &id,
                                    WidgetState::Type type) {
            if (sceneState == nullptr || !id.isValid()) {
                return nullptr;
            }

            auto &widgetsState = getSceneState(sceneState);
            widgetsState.registeredWidgets.insert(id.toUint64());

            auto &state = widgetsState.widgetStates[id.toUint64()];
            if (state.type != WidgetState::Type::unknown &&
                state.type != type) {
                state = {};
            }

            state.type = type;
            state.isHovered = widgetsState.hoveredWidgetId == id.toUint64();
            state.isPressed = widgetsState.pressedWidgetId == id.toUint64();
            state.isFocused = widgetsState.focusedWidgetId == id.toUint64();
            return &state;
        }

        bool consumeClick(SceneState *sceneState, const PickingId &id) {
            auto state = getWidgetState(sceneState, id);

            if (state == nullptr) {
                BESS_WARN("[SceneWidgets] Trying to consume click for "
                          "unregistered widget with id {}",
                          (uint64_t)id);
                return false;
            }

            if (!state->isClicked) {
                return false;
            }

            state->isClicked = false;
            return true;
        }

        bool isHovering(const SceneState *sceneState, const PickingId &id) {
            auto widgetsState = findSceneState(sceneState);
            return widgetsState != nullptr &&
                   widgetsState->hoveredWidgetId == id.toUint64();
        }

        bool isPressed(const SceneState *sceneState, const PickingId &id) {
            auto widgetsState = findSceneState(sceneState);
            return widgetsState != nullptr &&
                   widgetsState->pressedWidgetId == id.toUint64();
        }

        bool isFocused(const SceneState *sceneState, const PickingId &id) {
            auto widgetsState = findSceneState(sceneState);
            return widgetsState != nullptr &&
                   widgetsState->focusedWidgetId == id.toUint64();
        }

        void clearFocus(SceneWidgetsState &widgetsState) {
            if (widgetsState.focusedWidgetId == kInvalidWidgetId) {
                return;
            }

            auto prev = widgetsState.widgetStates.find(
                widgetsState.focusedWidgetId);
            if (prev != widgetsState.widgetStates.end()) {
                prev->second.isFocused = false;
                prev->second.focusStarted = false;
            }
            widgetsState.focusedWidgetId = kInvalidWidgetId;
        }

        void focusWidget(SceneWidgetsState &widgetsState,
                         const PickingId &id) {
            const auto widgetId = id.toUint64();
            if (widgetsState.focusedWidgetId == widgetId) {
                return;
            }

            clearFocus(widgetsState);

            auto state = getWidgetState(widgetsState, id);
            if (state == nullptr) {
                return;
            }

            widgetsState.focusedWidgetId = widgetId;
            state->isFocused = true;
            state->focusStarted = true;
        }

        void clampCursor(WidgetState &state) {
            state.cursorPos = std::min(state.cursorPos, state.text.size());
        }

        void markTextChanged(WidgetState &state) {
            state.textChanged = true;
            clampCursor(state);
        }

        std::optional<char> keyToChar(KeyCode key, bool shift) {
            if (key >= KeyCode::a && key <= KeyCode::z) {
                const char base = shift ? 'A' : 'a';
                return static_cast<char>(
                    base + (static_cast<uint16_t>(key) -
                            static_cast<uint16_t>(KeyCode::a)));
            }

            if (key >= KeyCode::d0 && key <= KeyCode::d9) {
                static constexpr char shiftedDigits[] = {
                    ')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
                const auto digit =
                    static_cast<size_t>(static_cast<uint16_t>(key) -
                                        static_cast<uint16_t>(KeyCode::d0));
                return shift ? shiftedDigits[digit]
                             : static_cast<char>('0' + digit);
            }

            switch (key) {
            case KeyCode::space:
                return ' ';
            case KeyCode::apostrophe:
                return shift ? '"' : '\'';
            case KeyCode::comma:
                return shift ? '<' : ',';
            case KeyCode::minus:
                return shift ? '_' : '-';
            case KeyCode::period:
                return shift ? '>' : '.';
            case KeyCode::slash:
                return shift ? '?' : '/';
            case KeyCode::semicolon:
                return shift ? ':' : ';';
            case KeyCode::equal:
                return shift ? '+' : '=';
            case KeyCode::leftBracket:
                return shift ? '{' : '[';
            case KeyCode::backslash:
                return shift ? '|' : '\\';
            case KeyCode::rightBracket:
                return shift ? '}' : ']';
            case KeyCode::graveAccent:
                return shift ? '~' : '`';
            default:
                return std::nullopt;
            }
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
                        : state.cursorPos - 1;
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
                    evt.isCtrlPressed
                        ? nextWordBoundary(state.text, state.cursorPos)
                        : state.cursorPos + 1;
                state.text.erase(state.cursorPos, eraseEnd - state.cursorPos);
                markTextChanged(state);
                return true;
            }
            case KeyCode::arrowLeft:
                state.cursorPos =
                    evt.isCtrlPressed
                        ? previousWordBoundary(state.text, state.cursorPos)
                        : (state.cursorPos > 0 ? state.cursorPos - 1 : 0);
                return true;
            case KeyCode::arrowRight:
                state.cursorPos =
                    evt.isCtrlPressed
                        ? nextWordBoundary(state.text, state.cursorPos)
                        : std::min(state.cursorPos + 1, state.text.size());
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
                break;
            }

            if (evt.isCtrlPressed || evt.isAltPressed) {
                return true;
            }

            const auto ch = keyToChar(data.keycode, evt.isShiftPressed);
            if (!ch.has_value() || state.text.size() >= state.maxLength) {
                return true;
            }

            state.text.insert(state.cursorPos, 1, *ch);
            ++state.cursorPos;
            markTextChanged(state);
            return true;
        }

        void drawToggleButton(const PickingId &id, bool isHigh,
                              const glm::vec3 &buttonPos,
                              const glm::vec2 &buttonSize,
                              SceneDrawContext &context) {
            static const SceneDraw::QuadStyle trackProps{
                .borderColor = ViewportTheme::colors.componentBorder,
                .borderRadius = glm::vec4(5.5f),
                .borderSize = glm::vec4(0.5f),
            };
            constexpr SceneDraw::QuadStyle buttonProps{.borderRadius =
                                                           glm::vec4(5.f)};

            auto trackColor = isHigh ? ViewportTheme::colors.stateHigh
                                     : ViewportTheme::colors.background;
            if (isHovering(context.sceneState, id)) {
                trackColor = Core::Renderer::Color(trackColor) * 1.15f;
            }
            if (isPressed(context.sceneState, id)) {
                trackColor = Core::Renderer::Color(trackColor) * 0.85f;
            }

            SceneDraw::drawQuad(context, buttonPos, buttonSize, trackColor, id,
                                trackProps);

            const float buttonHeadPosX =
                isHigh
                    ? buttonPos.x + (buttonSize.x / 2.f) - (buttonSize.y / 2.f)
                    : buttonPos.x - (buttonSize.x / 2.f) + (buttonSize.y / 2.f);

            const glm::vec3 buttonHeadPos =
                glm::vec3(buttonHeadPosX, buttonPos.y, buttonPos.z);
            SceneDraw::drawQuad(context, buttonHeadPos,
                                {buttonSize.y - 1.f, buttonSize.y - 1.f},
                                ViewportTheme::colors.stateLow, id,
                                buttonProps);
        }

        size_t findVisibleStart(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            std::string_view text, size_t cursor, float maxWidth,
            const Core::Renderer::FontProps &fontProps) {
            cursor = std::min(cursor, text.size());
            if (cursor == 0 || renderer->measureText(text.substr(0, cursor),
                                                     fontProps)
                                      .x <= maxWidth) {
                return 0;
            }

            size_t low = 0;
            size_t high = cursor;
            while (low < high) {
                const size_t mid = (low + high) / 2;
                if (renderer->measureText(text.substr(mid, cursor - mid),
                                          fontProps)
                        .x <= maxWidth) {
                    high = mid;
                } else {
                    low = mid + 1;
                }
            }
            return low;
        }

        size_t findVisibleEnd(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            std::string_view text, size_t start, size_t cursor, float maxWidth,
            const Core::Renderer::FontProps &fontProps) {
            size_t low = std::min(cursor, text.size());
            size_t high = text.size();
            while (low < high) {
                const size_t mid = (low + high + 1) / 2;
                if (renderer->measureText(text.substr(start, mid - start),
                                          fontProps)
                        .x <= maxWidth) {
                    low = mid;
                } else {
                    high = mid - 1;
                }
            }
            return low;
        }

        std::string_view visibleTextForCursor(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            std::string_view text, size_t cursor, float maxWidth,
            const Core::Renderer::FontProps &fontProps, size_t &visibleStart) {
            if (text.empty() || maxWidth <= 1.f) {
                visibleStart = 0;
                return {};
            }

            if (renderer->measureText(text, fontProps).x <= maxWidth) {
                visibleStart = 0;
                return text;
            }

            visibleStart =
                findVisibleStart(renderer, text, cursor, maxWidth, fontProps);
            const size_t visibleEnd = findVisibleEnd(
                renderer, text, visibleStart, cursor, maxWidth, fontProps);
            return text.substr(visibleStart, visibleEnd - visibleStart);
        }
    } // namespace

    void beginFrame(SceneState *sceneState) {
        getSceneState(sceneState).registeredWidgets.clear();
    }

    void endFrame(SceneState *sceneState) {
        auto widgetsState = findSceneState(sceneState);
        if (widgetsState == nullptr) {
            return;
        }

        for (auto it = widgetsState->widgetStates.begin();
             it != widgetsState->widgetStates.end();) {
            if (!widgetsState->registeredWidgets.contains(it->first)) {
                if (widgetsState->hoveredWidgetId == it->first) {
                    widgetsState->hoveredWidgetId = kInvalidWidgetId;
                }
                if (widgetsState->pressedWidgetId == it->first) {
                    widgetsState->pressedWidgetId = kInvalidWidgetId;
                }
                if (widgetsState->focusedWidgetId == it->first) {
                    widgetsState->focusedWidgetId = kInvalidWidgetId;
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
        sceneStates().erase(sceneKey(sceneState));
    }

    bool contains(const SceneState *sceneState, const PickingId &id) {
        if (!id.isValid()) {
            return false;
        }

        auto widgetsState = findSceneState(sceneState);
        return widgetsState != nullptr &&
               widgetsState->registeredWidgets.contains(id.toUint64());
    }

    bool isTextInput(const SceneState *sceneState, const PickingId &id) {
        auto widgetsState = findSceneState(sceneState);
        if (widgetsState == nullptr) {
            return false;
        }

        const auto widget = getWidgetState(*widgetsState, id);
        return widget != nullptr &&
               widget->type == WidgetState::Type::textInput;
    }

    bool hasPointerCapture(const SceneState *sceneState) {
        auto widgetsState = findSceneState(sceneState);
        return widgetsState != nullptr &&
               widgetsState->pressedWidgetId != kInvalidWidgetId;
    }

    bool wantsKeyboard(const SceneState *sceneState) {
        auto wantsKeyboardInState = [](const SceneWidgetsState &widgetsState) {
            if (widgetsState.focusedWidgetId == kInvalidWidgetId) {
                return false;
            }

            const auto it =
                widgetsState.widgetStates.find(widgetsState.focusedWidgetId);
            return it != widgetsState.widgetStates.end() &&
                   it->second.type == WidgetState::Type::textInput;
        };

        if (sceneState != nullptr) {
            auto widgetsState = findSceneState(sceneState);
            return widgetsState != nullptr &&
                   wantsKeyboardInState(*widgetsState);
        }

        for (const auto &[_, widgetsState] : sceneStates()) {
            if (wantsKeyboardInState(widgetsState)) {
                return true;
            }
        }
        return false;
    }

    void queuePress(SceneState *sceneState, const PickingId &id) {
        auto widgetsState = findSceneState(sceneState);
        if (widgetsState == nullptr) {
            BESS_WARN("[SceneWidgets] Trying to press widget before scene "
                      "widgets were registered");
            return;
        }

        auto widget = getWidgetState(*widgetsState, id);
        if (widget == nullptr) {
            BESS_WARN("[SceneWidgets] Trying to press unregistered widget");
            return;
        }

        widgetsState->pressedWidgetId = id.toUint64();
        widget->isPressed = true;

        if (widget->type == WidgetState::Type::textInput) {
            focusWidget(*widgetsState, id);
        } else {
            clearFocus(*widgetsState);
        }
    }

    void queueRelease(SceneState *sceneState, const PickingId &id) {
        auto widgetsState = findSceneState(sceneState);
        if (widgetsState == nullptr) {
            return;
        }

        const auto pressedWidgetId = widgetsState->pressedWidgetId;
        if (pressedWidgetId == kInvalidWidgetId) {
            return;
        }

        if (auto pressed =
                widgetsState->widgetStates.find(pressedWidgetId);
            pressed != widgetsState->widgetStates.end()) {
            pressed->second.isPressed = false;
        }

        widgetsState->pressedWidgetId = kInvalidWidgetId;

        if (id.toUint64() != pressedWidgetId ||
            !widgetsState->registeredWidgets.contains(pressedWidgetId)) {
            return;
        }

        auto widget = getWidgetState(*widgetsState, id);
        if (widget != nullptr) {
            widget->isClicked = true;
        }
    }

    void queueClick(SceneState *sceneState, const PickingId &id) {
        auto state = getWidgetState(sceneState, id);

        if (!state) {
            BESS_WARN("[SceneWidgets] Trying to queue click for "
                      "unregistered widget");
            return;
        }

        state->isClicked = true;
    }

    bool queueKey(SceneState *sceneState, const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::key) {
            return false;
        }

        auto widgetsState = findSceneState(sceneState);
        if (widgetsState == nullptr ||
            widgetsState->focusedWidgetId == kInvalidWidgetId) {
            return false;
        }

        auto state = widgetsState->widgetStates.find(
            widgetsState->focusedWidgetId);
        if (state == widgetsState->widgetStates.end() ||
            state->second.type != WidgetState::Type::textInput) {
            return false;
        }

        const bool handled = handleTextInputKey(state->second, evt);
        if (state->second.textSubmitted || state->second.textCanceled) {
            clearFocus(*widgetsState);
        }
        return handled;
    }

    void clearFocus(SceneState *sceneState) {
        auto widgetsState = findSceneState(sceneState);
        if (widgetsState != nullptr) {
            clearFocus(*widgetsState);
        }
    }

    void setHoverId(SceneState *sceneState, const PickingId &id) {
        auto widgetsState = findSceneState(sceneState);
        if (widgetsState == nullptr) {
            return;
        }

        if (widgetsState->hoveredWidgetId == id.toUint64()) {
            return;
        }

        if (widgetsState->hoveredWidgetId != kInvalidWidgetId) {
            auto prevState =
                widgetsState->widgetStates.find(widgetsState->hoveredWidgetId);
            if (prevState != widgetsState->widgetStates.end()) {
                prevState->second.isHovered = false;
            }
        }

        widgetsState->hoveredWidgetId = id.toUint64();

        if (id.isValid()) {
            auto newState = getWidgetState(*widgetsState, id);
            if (newState) {
                newState->isHovered = true;
            }
        }
    }

    bool toggleButton(const PickingId &id, bool value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context) {
        registerWidget(context.sceneState, id, WidgetState::Type::toggleButton);
        drawToggleButton(id, value, buttonPos, buttonSize, context);
        return consumeClick(context.sceneState, id);
    }

    bool toggleButton(const PickingId &id, bool *value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context) {
        if (value == nullptr) {
            return false;
        }

        const bool clicked =
            toggleButton(id, *value, buttonPos, buttonSize, context);
        if (clicked) {
            *value = !*value;
        }
        return clicked;
    }

    bool button(const PickingId &id, const std::string &label,
                const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                const Core::Renderer::Color &labelColor,
                SceneDrawContext &context) {
        constexpr float paddingY = 2.f;
        constexpr float paddingX = 3.f;

        registerWidget(context.sceneState, id, WidgetState::Type::button);

        static const SceneDraw::QuadStyle buttonProps{
            .borderColor = Core::Renderer::Colors::slate700,
            .borderRadius = glm::vec4(2.f),
            .borderSize = glm::vec4(0.5f),
        };

        auto bgColor = Core::Renderer::Colors::slate900;
        if (isHovering(context.sceneState, id)) {
            bgColor = bgColor * 1.2f;
        }
        if (isPressed(context.sceneState, id)) {
            bgColor = bgColor * 0.8f;
        }

        const auto textSize = context.renderer->measureText(
            label, {.fontSize = kDefaultButtonTextSize});

        const float textOffY = context.renderer->textCenterOffsetY(
            label, {.fontSize = kDefaultButtonTextSize});

        auto size = buttonSize;

        if (size.y == 0.f) {
            size.y = textSize.y + (paddingY * 2.f);
        }

        if (size.x == 0.f) {
            size.x = textSize.x + (paddingX * 2.f);
        }

        SceneDraw::drawQuad(context, buttonPos, size, bgColor, id,
                            buttonProps);

        const auto textPos =
            glm::vec3(buttonPos.x - (textSize.x / 2.f), buttonPos.y + textOffY,
                      buttonPos.z + 0.001f);
        SceneDraw::drawText(context, label, textPos, kDefaultButtonTextSize,
                            labelColor, id);

        return consumeClick(context.sceneState, id);
    }

    TextBoxResult textBox(const PickingId &id, std::string *value,
                          const glm::vec3 &boxPos,
                          const glm::vec2 &boxSize,
                          SceneDrawContext &context,
                          const TextBoxOptions &options) {
        TextBoxResult result;
        if (value == nullptr || context.renderer == nullptr) {
            return result;
        }

        auto widget =
            registerWidget(context.sceneState, id, WidgetState::Type::textInput);
        if (widget == nullptr) {
            return result;
        }

        widget->maxLength = options.maxLength;

        if (!widget->textInitialized ||
            (!widget->isFocused && widget->text != *value)) {
            widget->text = value->substr(0, options.maxLength);
            widget->cursorPos = widget->text.size();
            widget->textInitialized = true;
        }

        if (widget->focusStarted) {
            widget->text = value->substr(0, options.maxLength);
            widget->focusStartText = widget->text;
            widget->cursorPos = widget->text.size();
            widget->focusStarted = false;
        }

        clampCursor(*widget);

        if (widget->textChanged) {
            *value = widget->text;
        }

        result = {
            .changed = widget->textChanged,
            .submitted = widget->textSubmitted,
            .canceled = widget->textCanceled,
            .focused = widget->isFocused,
        };

        auto size = boxSize;
        const auto referenceTextSize =
            context.renderer->measureText("M", {.fontSize = options.fontSize});

        if (size.y == 0.f) {
            size.y = referenceTextSize.y + (options.padding.y * 2.f);
        }

        if (size.x == 0.f) {
            const auto measuredText = context.renderer->measureText(
                value->empty() ? options.placeholder : std::string_view(*value),
                {.fontSize = options.fontSize});
            size.x = std::max(48.f, measuredText.x + (options.padding.x * 2.f));
        }

        const bool focused = isFocused(context.sceneState, id);
        auto bgColor = focused ? options.focusedBackgroundColor
                               : options.backgroundColor;
        if (!focused && isHovering(context.sceneState, id)) {
            bgColor = options.hoverBackgroundColor;
        }

        const SceneDraw::QuadStyle style{
            .borderColor = focused ? options.focusedBorderColor
                                   : options.borderColor,
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

        size_t visibleStart = 0;
        std::string_view visibleText;
        Core::Renderer::Color textColor = options.textColor;

        if (widget->text.empty() && !focused && !options.placeholder.empty()) {
            visibleText = options.placeholder;
            textColor = options.placeholderColor;
        } else {
            visibleText = visibleTextForCursor(
                context.renderer, widget->text, widget->cursorPos,
                contentWidth, fontProps, visibleStart);
        }

        const float textOffY = context.renderer->textCenterOffsetY(
            visibleText.empty() ? std::string_view("M") : visibleText,
            {.fontSize = options.fontSize});
        const glm::vec3 textPos{
            left,
            boxPos.y + textOffY,
            boxPos.z + 0.001f,
        };
        SceneDraw::drawText(context, visibleText, textPos,
                            static_cast<size_t>(options.fontSize), textColor,
                            id);

        if (focused) {
            const auto cursorText = std::string_view(widget->text).substr(
                visibleStart, widget->cursorPos - visibleStart);
            const float cursorX =
                left + context.renderer->measureText(cursorText, fontProps).x;
            const float cursorHeight =
                std::max(4.f, size.y - (options.padding.y * 2.f));
            SceneDraw::drawQuad(
                context, {cursorX, boxPos.y, boxPos.z + 0.002f},
                {1.f, cursorHeight}, options.cursorColor, id);
        }

        return result;
    }
} // namespace Bess::Canvas::SceneWidgets
