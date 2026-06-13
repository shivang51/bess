#include "scene_widgets_internal.h"
#include "common/logger.h"
#include <algorithm>
#include <array>

namespace Bess::Canvas::SceneWidgets::Detail {
    namespace {
        constexpr uint32_t kWidgetGeneratedInfoBit = 1u << 30;
        constexpr uint32_t kWidgetGeneratedInfoMask =
            kWidgetGeneratedInfoBit - 1u;

        uint32_t fnv1a(uint32_t hash, uint32_t value) {
            constexpr uint32_t prime = 16777619u;
            const std::array bytes{
                static_cast<uint8_t>(value & 0xFFu),
                static_cast<uint8_t>((value >> 8u) & 0xFFu),
                static_cast<uint8_t>((value >> 16u) & 0xFFu),
                static_cast<uint8_t>((value >> 24u) & 0xFFu),
            };

            for (const uint8_t byte : bytes) {
                hash ^= byte;
                hash *= prime;
            }
            return hash;
        }
    } // namespace

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

    void closeDropdowns(SceneWidgetsState &widgetsState,
                        uint64_t keepOpenWidgetId) {
        for (auto &[widgetId, widget] : widgetsState.widgetStates) {
            if (widget.type != WidgetState::Type::dropdown ||
                widgetId == keepOpenWidgetId || !widget.dropdownOpen) {
                continue;
            }

            widget.dropdownOpen = false;
            widget.dropdownClosed = true;
        }
    }

    void clampCursor(WidgetState &state) {
        state.cursorPos = std::min(state.cursorPos, state.text.size());
    }

    void markTextChanged(WidgetState &state) {
        state.textChanged = true;
        clampCursor(state);
    }

    PickingId makeChildId(const PickingId &parentId, uint32_t childIndex) {
        constexpr uint32_t offsetBasis = 2166136261u;
        uint32_t hash = fnv1a(offsetBasis, parentId.info);
        hash = fnv1a(hash, childIndex);
        hash = (hash & kWidgetGeneratedInfoMask) | kWidgetGeneratedInfoBit |
               PickingId::InfoFlags::unSelectable;
        return {parentId.runtimeId, hash};
    }

    void ensureDropdownHighlightVisible(WidgetState &state) {
        if (state.dropdownOptionCount == 0) {
            state.dropdownHighlightedIndex = 0;
            state.dropdownScrollOffset = 0;
            return;
        }

        state.dropdownHighlightedIndex = std::min(
            state.dropdownHighlightedIndex, state.dropdownOptionCount - 1);

        const size_t visibleCount =
            state.dropdownMaxVisibleOptions == 0
                ? state.dropdownOptionCount
                : std::min(state.dropdownMaxVisibleOptions,
                           state.dropdownOptionCount);

        if (visibleCount == 0 ||
            state.dropdownOptionCount <= visibleCount) {
            state.dropdownScrollOffset = 0;
            return;
        }

        if (state.dropdownHighlightedIndex < state.dropdownScrollOffset) {
            state.dropdownScrollOffset = state.dropdownHighlightedIndex;
        } else if (state.dropdownHighlightedIndex >=
                   state.dropdownScrollOffset + visibleCount) {
            state.dropdownScrollOffset =
                state.dropdownHighlightedIndex - visibleCount + 1;
        }

        state.dropdownScrollOffset = std::min(
            state.dropdownScrollOffset, state.dropdownOptionCount - visibleCount);
    }
} // namespace Bess::Canvas::SceneWidgets::Detail
