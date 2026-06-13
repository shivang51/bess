#include "scene_widgets_internal.h"
#include "common/logger.h"
#include <algorithm>

namespace Bess::Canvas::SceneWidgets::Detail {
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
        auto &states = sceneStates();
        const auto it = states.find(sceneKey(sceneState));
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

    WidgetState *registerWidget(SceneState *sceneState, const PickingId &id,
                                WidgetState::Type type) {
        if (sceneState == nullptr || !id.isValid()) {
            return nullptr;
        }

        auto &widgetsState = getSceneState(sceneState);
        widgetsState.registeredWidgets.insert(id.toUint64());

        auto &state = widgetsState.widgetStates[id.toUint64()];
        if (state.type != WidgetState::Type::unknown && state.type != type) {
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

    void clearFocusState(SceneWidgetsState &widgetsState) {
        if (widgetsState.focusedWidgetId == kInvalidWidgetId) {
            return;
        }

        auto prev = widgetsState.widgetStates.find(widgetsState.focusedWidgetId);
        if (prev != widgetsState.widgetStates.end()) {
            prev->second.isFocused = false;
            prev->second.focusStarted = false;
        }
        widgetsState.focusedWidgetId = kInvalidWidgetId;
    }

    void focusWidget(SceneWidgetsState &widgetsState, const PickingId &id) {
        const auto widgetId = id.toUint64();
        if (widgetsState.focusedWidgetId == widgetId) {
            return;
        }

        clearFocusState(widgetsState);

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
} // namespace Bess::Canvas::SceneWidgets::Detail
