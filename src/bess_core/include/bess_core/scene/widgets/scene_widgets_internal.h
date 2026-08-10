#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include <cstdint>
#include <glm.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Bess::Canvas::SceneWidgets::Detail {
    constexpr uint64_t kInvalidWidgetId = PickingId::invalid().toUint64();

    inline Core::Renderer::Color
    colorOr(const std::optional<Core::Renderer::Color> &overrideColor,
            const glm::vec4 &themeColor) {
        return overrideColor.value_or(Core::Renderer::Color(themeColor));
    }

    struct BESS_API WidgetState {
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
        size_t selectionAnchorPos = 0;
        size_t maxLength = 256;
        bool textInitialized = false;
        bool focusStarted = false;
        bool textChanged = false;
        bool textSubmitted = false;
        bool textCanceled = false;
        bool textPointerSelecting = false;
        bool textPointerSelectionStarted = false;
        bool textPointerExtendSelection = false;

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

} // namespace Bess::Canvas::SceneWidgets::Detail

namespace Bess::Canvas::SceneWidgets {
    struct BESS_API SceneWidgetsState {
        std::unordered_set<uint64_t> registeredWidgets;
        std::unordered_map<uint64_t, Detail::WidgetState> widgetStates;
        uint64_t hoveredWidgetId = Detail::kInvalidWidgetId;
        uint64_t pressedWidgetId = Detail::kInvalidWidgetId;
        uint64_t focusedWidgetId = Detail::kInvalidWidgetId;
        std::string textClipboard;
    };

    struct BESS_API ViewportSceneWidgetsState {
        std::unordered_map<UUID, SceneWidgetsState> sceneStates;

        void clear() {
            sceneStates.clear();
        }
    };
} // namespace Bess::Canvas::SceneWidgets

namespace Bess::Canvas::SceneWidgets::Detail {
    using Bess::Canvas::SceneWidgets::SceneWidgetsState;

    SceneWidgetsState *
    findSceneWidgetsState(ViewportSceneWidgetsState *viewportState,
                          const SceneState *sceneState);
    const SceneWidgetsState *
    findSceneWidgetsState(const ViewportSceneWidgetsState *viewportState,
                          const SceneState *sceneState);
    SceneWidgetsState *getWidgetsState(ViewportSceneWidgetsState *viewportState,
                                       const SceneState *sceneState);

    WidgetState *getWidgetState(SceneWidgetsState &widgetsState,
                                const PickingId &id);

    const WidgetState *getWidgetState(const SceneWidgetsState &widgetsState,
                                      const PickingId &id);

    WidgetState *getWidgetState(SceneWidgetsState *widgetsState,
                                const PickingId &id,
                                const char *warningContext = nullptr);

    WidgetState *registerWidget(SceneWidgetsState *widgetsState,
                                const PickingId &id,
                                WidgetState::Type type);
    bool consumeClick(SceneWidgetsState *widgetsState, const PickingId &id);

    bool isHovering(const SceneWidgetsState *widgetsState, const PickingId &id);
    bool isPressed(const SceneWidgetsState *widgetsState, const PickingId &id);
    bool isFocused(const SceneWidgetsState *widgetsState, const PickingId &id);

    void clearFocusState(SceneWidgetsState &widgetsState);
    void focusWidget(SceneWidgetsState &widgetsState, const PickingId &id);
    void closeDropdowns(SceneWidgetsState &widgetsState,
                        uint64_t keepOpenWidgetId = kInvalidWidgetId);
    void clampCursor(WidgetState &state);
    void clearTextSelection(WidgetState &state);
    bool hasTextSelection(const WidgetState &state);
    std::pair<size_t, size_t> textSelectionRange(const WidgetState &state);
    void markTextChanged(WidgetState &state);
    bool handleTextInputKey(SceneWidgetsState &widgetsState,
                            WidgetState &state,
                            const SceneEvent &evt);
    bool handleTextInputCodepoint(WidgetState &state, char32_t codepoint);
    bool handleSliderKey(WidgetState &state, const SceneEvent &evt);
    bool handleDropdownKey(SceneWidgetsState &widgetsState,
                           WidgetState &state,
                           const SceneEvent &evt);
    PickingId makeChildId(const PickingId &parentId, uint32_t childIndex);
    void ensureDropdownHighlightVisible(WidgetState &state);
} // namespace Bess::Canvas::SceneWidgets::Detail
