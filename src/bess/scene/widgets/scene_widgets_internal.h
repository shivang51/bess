#pragma once

#include "scene/scene_event.h"
#include "scene/scene_state/scene_state.h"
#include "scene/widgets/scene_widgets.h"
#include <cstdint>
#include <glm.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Bess::Canvas::SceneWidgets::Detail {
    constexpr uint64_t kInvalidWidgetId = PickingId::invalid().toUint64();

    inline Core::Renderer::Color
    colorOr(const std::optional<Core::Renderer::Color> &overrideColor,
            const glm::vec4 &themeColor) {
        return overrideColor.value_or(Core::Renderer::Color(themeColor));
    }

    struct WidgetState {
        enum class Type : uint8_t {
            unknown,
            toggleButton,
            button,
            textInput,
            slider,
            dropdown,
            dropdownOption,
        };

        Type type = Type::unknown;
        bool isHovered = false;
        bool isPressed = false;
        bool isClicked = false;
        bool isFocused = false;
        glm::vec3 boundsPos{0.f};
        glm::vec2 boundsSize{0.f};
        glm::vec2 pointerPos{0.f};
        bool pointerInputQueued = false;

        std::string text;
        std::string focusStartText;
        size_t cursorPos = 0;
        size_t maxLength = 256;
        bool textInitialized = false;
        bool focusStarted = false;
        bool textChanged = false;
        bool textSubmitted = false;
        bool textCanceled = false;

        int sliderKeyboardDelta = 0;
        bool sliderSetToMin = false;
        bool sliderSetToMax = false;

        bool dropdownOpen = false;
        bool dropdownOpened = false;
        bool dropdownClosed = false;
        int pendingDropdownSelection = -1;
        size_t dropdownHighlightedIndex = 0;
        size_t dropdownScrollOffset = 0;
        size_t dropdownOptionCount = 0;
        size_t dropdownMaxVisibleOptions = 0;
        uint64_t ownerWidgetId = kInvalidWidgetId;
        size_t optionIndex = 0;
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

    WidgetState *registerWidget(SceneState *sceneState,
                                const PickingId &id,
                                WidgetState::Type type);
    bool consumeClick(SceneState *sceneState, const PickingId &id);

    bool isHovering(const SceneState *sceneState, const PickingId &id);
    bool isPressed(const SceneState *sceneState, const PickingId &id);
    bool isFocused(const SceneState *sceneState, const PickingId &id);

    void clearFocusState(SceneWidgetsState &widgetsState);
    void focusWidget(SceneWidgetsState &widgetsState, const PickingId &id);
    void closeDropdowns(SceneWidgetsState &widgetsState,
                        uint64_t keepOpenWidgetId = kInvalidWidgetId);
    void clampCursor(WidgetState &state);
    void markTextChanged(WidgetState &state);
    bool handleTextInputKey(WidgetState &state, const SceneEvent &evt);
    bool handleTextInputCodepoint(WidgetState &state, char32_t codepoint);
    bool handleSliderKey(WidgetState &state, const SceneEvent &evt);
    bool handleDropdownKey(SceneWidgetsState &widgetsState,
                           WidgetState &state,
                           const SceneEvent &evt);
    PickingId makeChildId(const PickingId &parentId, uint32_t childIndex);
    void ensureDropdownHighlightVisible(WidgetState &state);
} // namespace Bess::Canvas::SceneWidgets::Detail
