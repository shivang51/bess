#pragma once

#include "scene/widgets/scene_widgets.h"
#include "scene/scene_event.h"
#include "scene/scene_state/scene_state.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Bess::Canvas::SceneWidgets::Detail {
    constexpr uint64_t kInvalidWidgetId = PickingId::invalid().toUint64();
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

    std::unordered_map<uint64_t, SceneWidgetsState> &sceneStates();
    uint64_t sceneKey(const SceneState *sceneState);
    SceneWidgetsState *findSceneState(const SceneState *sceneState);
    SceneWidgetsState &getSceneState(SceneState *sceneState);

    WidgetState *getWidgetState(SceneWidgetsState &widgetsState,
                                const PickingId &id);
    const WidgetState *getWidgetState(const SceneWidgetsState &widgetsState,
                                      const PickingId &id);
    WidgetState *getWidgetState(const SceneState *sceneState,
                                const PickingId &id);

    WidgetState *registerWidget(SceneState *sceneState, const PickingId &id,
                                WidgetState::Type type);
    bool consumeClick(SceneState *sceneState, const PickingId &id);

    bool isHovering(const SceneState *sceneState, const PickingId &id);
    bool isPressed(const SceneState *sceneState, const PickingId &id);
    bool isFocused(const SceneState *sceneState, const PickingId &id);

    void clearFocusState(SceneWidgetsState &widgetsState);
    void focusWidget(SceneWidgetsState &widgetsState, const PickingId &id);
    void clampCursor(WidgetState &state);
    void markTextChanged(WidgetState &state);
    bool handleTextInputKey(WidgetState &state, const SceneEvent &evt);
    bool handleTextInputCodepoint(WidgetState &state, char32_t codepoint);
} // namespace Bess::Canvas::SceneWidgets::Detail
