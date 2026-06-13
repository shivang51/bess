#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "common/types.h"
#include "scene_draw_context.h"
#include <glm.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Bess::Canvas {
    struct SceneEvent;
    class SceneState;
} // namespace Bess::Canvas

namespace Bess::Canvas::SceneWidgets {
    struct TextBoxOptions {
        std::string_view placeholder;
        size_t maxLength = 256;
        float fontSize = 8.f;
        glm::vec2 padding{4.f, 2.f};
        std::optional<Core::Renderer::Color> backgroundColor;
        std::optional<Core::Renderer::Color> hoverBackgroundColor;
        std::optional<Core::Renderer::Color> focusedBackgroundColor;
        std::optional<Core::Renderer::Color> borderColor;
        std::optional<Core::Renderer::Color> focusedBorderColor;
        std::optional<Core::Renderer::Color> textColor;
        std::optional<Core::Renderer::Color> placeholderColor;
        std::optional<Core::Renderer::Color> cursorColor;
    };

    struct TextBoxResult {
        bool changed = false;
        bool submitted = false;
        bool canceled = false;
        bool focused = false;
    };

    struct SliderOptions {
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

    struct SliderResult {
        bool changed = false;
        bool editing = false;
        bool focused = false;
    };

    struct DropdownOptions {
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

    struct DropdownResult {
        bool changed = false;
        bool opened = false;
        bool closed = false;
        bool expanded = false;
        int selectedIndex = -1;
    };

    void beginFrame(SceneState *sceneState);
    void endFrame(SceneState *sceneState);
    void clearScene(SceneState *sceneState);

    bool contains(const SceneState *sceneState, const PickingId &id);
    bool isTextInput(const SceneState *sceneState, const PickingId &id);
    bool hasPointerCapture(const SceneState *sceneState);
    bool wantsKeyboard(const SceneState *sceneState = nullptr);

    void queuePointerMove(SceneState *sceneState, const glm::vec2 &pos);
    void queuePress(SceneState *sceneState,
                    const PickingId &id,
                    const glm::vec2 &pos);
    void queueRelease(SceneState *sceneState,
                      const PickingId &id,
                      const glm::vec2 &pos);
    void queueClick(SceneState *sceneState, const PickingId &id);
    bool queueKey(SceneState *sceneState, const SceneEvent &evt);
    bool queueWheel(SceneState *sceneState, const SceneEvent &evt);
    void clearFocus(SceneState *sceneState);
    void setHoverId(SceneState *sceneState, const PickingId &id);

    bool toggleButton(const PickingId &id,
                      bool value,
                      const glm::vec3 &buttonPos,
                      const glm::vec2 &buttonSize,
                      SceneDrawContext &context);

    bool toggleButton(const PickingId &id,
                      bool *value,
                      const glm::vec3 &buttonPos,
                      const glm::vec2 &buttonSize,
                      SceneDrawContext &context);

    bool button(const PickingId &id,
                std::string_view label,
                const glm::vec3 &buttonPos,
                const glm::vec2 &buttonSize,
                const Core::Renderer::Color &labelColor,
                SceneDrawContext &context);

    TextBoxResult textBox(const PickingId &id,
                          std::string *value,
                          const glm::vec3 &boxPos,
                          const glm::vec2 &boxSize,
                          SceneDrawContext &context,
                          const TextBoxOptions &options = {});

    SliderResult sliderFloat(const PickingId &id,
                             float *value,
                             float minValue,
                             float maxValue,
                             const glm::vec3 &sliderPos,
                             const glm::vec2 &sliderSize,
                             SceneDrawContext &context,
                             const SliderOptions &options = {});

    SliderResult sliderInt(const PickingId &id,
                           int *value,
                           int minValue,
                           int maxValue,
                           const glm::vec3 &sliderPos,
                           const glm::vec2 &sliderSize,
                           SceneDrawContext &context,
                           const SliderOptions &options = {});

    DropdownResult dropdown(const PickingId &id,
                            int *selectedIndex,
                            std::span<const std::string_view> items,
                            const glm::vec3 &boxPos,
                            const glm::vec2 &boxSize,
                            SceneDrawContext &context,
                            const DropdownOptions &options = {});
} // namespace Bess::Canvas::SceneWidgets
