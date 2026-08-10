#pragma once

#include "common/bess_api.h"

#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_draw_context.h"
#include "common/types.h"
#include <glm.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Bess::Canvas {
    struct SceneEvent;
    class SceneState;
} // namespace Bess::Canvas

namespace Bess::Core::Viewport {
    struct ViewportContext;
} // namespace Bess::Core::Viewport

namespace Bess::Canvas::SceneWidgets {
    struct SceneWidgetsState;
    struct ViewportSceneWidgetsState;

    struct BESS_API TextBoxOptions {
        std::string_view placeholder;
        size_t maxLength = 256;
        bool selectAllOnFocus = false;
        float fontSize = 8.f;
        glm::vec2 padding{4.f, 2.f};
        std::optional<Core::Renderer::Color> backgroundColor;
        std::optional<Core::Renderer::Color> hoverBackgroundColor;
        std::optional<Core::Renderer::Color> focusedBackgroundColor;
        std::optional<Core::Renderer::Color> borderColor;
        std::optional<Core::Renderer::Color> focusedBorderColor;
        std::optional<Core::Renderer::Color> textColor;
        std::optional<Core::Renderer::Color> placeholderColor;
        std::optional<Core::Renderer::Color> selectionColor;
        std::optional<Core::Renderer::Color> cursorColor;
    };

    struct BESS_API TextBoxResult {
        bool changed = false;
        bool submitted = false;
        bool canceled = false;
        bool focused = false;
    };

    struct BESS_API SliderOptions {
        float step = 0.f;
        int precision = 2;
        bool showValue = true;
        float fontSize = 8.f;
        float trackHeight = 4.f;
        float knobRadius = 5.f;
        glm::vec2 padding{6.f, 2.f};
        std::optional<Core::Renderer::Color> backgroundColor;
        std::optional<Core::Renderer::Color> hoverBackgroundColor;
        std::optional<Core::Renderer::Color> focusedBorderColor;
        std::optional<Core::Renderer::Color> trackColor;
        std::optional<Core::Renderer::Color> fillColor;
        std::optional<Core::Renderer::Color> knobColor;
        std::optional<Core::Renderer::Color> textColor;
    };

    struct BESS_API SliderResult {
        bool changed = false;
        bool editing = false;
        bool focused = false;
    };

    struct BESS_API DropdownOptions {
        std::string_view placeholder = "Select";
        float fontSize = 8.f;
        float optionHeight = 18.f;
        size_t maxVisibleOptions = 8;
        glm::vec2 padding{5.f, 2.f};
        std::optional<Core::Renderer::Color> backgroundColor;
        std::optional<Core::Renderer::Color> hoverBackgroundColor;
        std::optional<Core::Renderer::Color> expandedBackgroundColor;
        std::optional<Core::Renderer::Color> borderColor;
        std::optional<Core::Renderer::Color> focusedBorderColor;
        std::optional<Core::Renderer::Color> optionHoverColor;
        std::optional<Core::Renderer::Color> optionSelectedColor;
        std::optional<Core::Renderer::Color> textColor;
        std::optional<Core::Renderer::Color> mutedTextColor;
    };

    struct BESS_API DropdownResult {
        bool changed = false;
        bool opened = false;
        bool closed = false;
        bool expanded = false;
        int selectedIndex = -1;
    };

    BESS_API SceneWidgetsState *
    getState(Core::Viewport::ViewportContext *viewportCtx,
             const SceneState *sceneState);
    BESS_API const SceneWidgetsState *
    findState(const Core::Viewport::ViewportContext *viewportCtx,
              const SceneState *sceneState);
    BESS_API void clearScene(Core::Viewport::ViewportContext *viewportCtx,
                             const SceneState *sceneState);

    BESS_API void beginFrame(SceneWidgetsState *widgetsState);
    BESS_API void endFrame(SceneWidgetsState *widgetsState);

    BESS_API bool contains(const SceneWidgetsState *widgetsState,
                           const PickingId &id);
    BESS_API bool isTextInput(const SceneWidgetsState *widgetsState,
                              const PickingId &id);
    BESS_API bool hasPointerCapture(const SceneWidgetsState *widgetsState);
    BESS_API bool wantsKeyboard(const SceneWidgetsState *widgetsState);

    BESS_API void queuePointerMove(SceneWidgetsState *widgetsState,
                                   const glm::vec2 &pos);
    BESS_API void queuePress(SceneWidgetsState *widgetsState,
                             const PickingId &id,
                             const glm::vec2 &pos,
                             bool extendSelection = false);
    BESS_API void queueRelease(SceneWidgetsState *widgetsState,
                               const PickingId &id,
                               const glm::vec2 &pos);
    BESS_API void queueClick(SceneWidgetsState *widgetsState,
                             const PickingId &id);
    BESS_API bool queueKey(SceneWidgetsState *widgetsState,
                           const SceneEvent &evt);
    BESS_API bool queueWheel(SceneWidgetsState *widgetsState,
                             const SceneEvent &evt);
    BESS_API void clearFocus(SceneWidgetsState *widgetsState);
    BESS_API void setHoverId(SceneWidgetsState *widgetsState,
                             const PickingId &id);

    BESS_API bool toggleButton(const PickingId &id,
                               bool value,
                               const glm::vec3 &buttonPos,
                               const glm::vec2 &buttonSize,
                               SceneDrawContext &context);

    BESS_API bool toggleButton(const PickingId &id,
                               bool *value,
                               const glm::vec3 &buttonPos,
                               const glm::vec2 &buttonSize,
                               SceneDrawContext &context);

    struct BESS_API ButtonOptions {
        float textSize = 12.f;
        glm::vec2 buttonSize{0.f};
        glm::vec2 padding{3.f, 2.f};
        glm::vec4 borderThickness{1.f};
        glm::vec4 borderRadius{2.f};
        Core::Renderer::ShadowProps shadow = {};
        std::optional<Core::Renderer::Color> backgroundColor;
        std::optional<Core::Renderer::Color> hoverBackgroundColor;
        std::optional<Core::Renderer::Color> pressedBackgroundColor;
        std::optional<Core::Renderer::Color> borderColor;
        std::optional<Core::Renderer::Color> textColor;
    };

    BESS_API bool button(const PickingId &id,
                         std::string_view label,
                         const glm::vec3 &buttonPos,
                         SceneDrawContext &context,
                         const ButtonOptions &options = {});

    BESS_API TextBoxResult textBox(const PickingId &id,
                                   std::string *value,
                                   const glm::vec3 &boxPos,
                                   const glm::vec2 &boxSize,
                                   SceneDrawContext &context,
                                   const TextBoxOptions &options = {});

    BESS_API SliderResult sliderFloat(const PickingId &id,
                                      float *value,
                                      float minValue,
                                      float maxValue,
                                      const glm::vec3 &sliderPos,
                                      const glm::vec2 &sliderSize,
                                      SceneDrawContext &context,
                                      const SliderOptions &options = {});

    BESS_API SliderResult sliderInt(const PickingId &id,
                                    int *value,
                                    int minValue,
                                    int maxValue,
                                    const glm::vec3 &sliderPos,
                                    const glm::vec2 &sliderSize,
                                    SceneDrawContext &context,
                                    const SliderOptions &options = {});

    BESS_API DropdownResult dropdown(const PickingId &id,
                                     int *selectedIndex,
                                     std::span<const std::string_view> items,
                                     const glm::vec3 &boxPos,
                                     const glm::vec2 &boxSize,
                                     SceneDrawContext &context,
                                     const DropdownOptions &options = {});
} // namespace Bess::Canvas::SceneWidgets
