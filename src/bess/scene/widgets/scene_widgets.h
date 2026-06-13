#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "common/types.h"
#include "scene_draw_context.h"
#include <glm.hpp>
#include <span>
#include <string>
#include <string_view>

namespace Bess::Canvas {
    struct SceneEvent;
    class SceneState;
}

namespace Bess::Canvas::SceneWidgets {
    struct TextBoxOptions {
        std::string_view placeholder = {};
        size_t maxLength = 256;
        float fontSize = 8.f;
        glm::vec2 padding{4.f, 2.f};
        Core::Renderer::Color backgroundColor =
            Core::Renderer::Color::fromHex(0x0F172AFF);
        Core::Renderer::Color hoverBackgroundColor =
            Core::Renderer::Color::fromHex(0x182033FF);
        Core::Renderer::Color focusedBackgroundColor =
            Core::Renderer::Color::fromHex(0x111827FF);
        Core::Renderer::Color borderColor =
            Core::Renderer::Color::fromHex(0x334155FF);
        Core::Renderer::Color focusedBorderColor =
            Core::Renderer::Color::fromHex(0x60A5FAFF);
        Core::Renderer::Color textColor =
            Core::Renderer::Color::fromHex(0xF1F5F9FF);
        Core::Renderer::Color placeholderColor =
            Core::Renderer::Color::fromHex(0x64748BFF);
        Core::Renderer::Color cursorColor =
            Core::Renderer::Color::fromHex(0xF8FAFCFF);
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
        Core::Renderer::Color backgroundColor =
            Core::Renderer::Color::fromHex(0x0F172AFF);
        Core::Renderer::Color hoverBackgroundColor =
            Core::Renderer::Color::fromHex(0x182033FF);
        Core::Renderer::Color focusedBorderColor =
            Core::Renderer::Color::fromHex(0x60A5FAFF);
        Core::Renderer::Color trackColor =
            Core::Renderer::Color::fromHex(0x334155FF);
        Core::Renderer::Color fillColor =
            Core::Renderer::Color::fromHex(0x38BDF8FF);
        Core::Renderer::Color knobColor =
            Core::Renderer::Color::fromHex(0xF8FAFCFF);
        Core::Renderer::Color textColor =
            Core::Renderer::Color::fromHex(0xCBD5E1FF);
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
        Core::Renderer::Color backgroundColor =
            Core::Renderer::Color::fromHex(0x0F172AFF);
        Core::Renderer::Color hoverBackgroundColor =
            Core::Renderer::Color::fromHex(0x182033FF);
        Core::Renderer::Color expandedBackgroundColor =
            Core::Renderer::Color::fromHex(0x111827FF);
        Core::Renderer::Color borderColor =
            Core::Renderer::Color::fromHex(0x334155FF);
        Core::Renderer::Color focusedBorderColor =
            Core::Renderer::Color::fromHex(0x60A5FAFF);
        Core::Renderer::Color optionHoverColor =
            Core::Renderer::Color::fromHex(0x1E293BFF);
        Core::Renderer::Color optionSelectedColor =
            Core::Renderer::Color::fromHex(0x075985FF);
        Core::Renderer::Color textColor =
            Core::Renderer::Color::fromHex(0xF1F5F9FF);
        Core::Renderer::Color mutedTextColor =
            Core::Renderer::Color::fromHex(0x94A3B8FF);
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
    void queuePress(SceneState *sceneState, const PickingId &id,
                    const glm::vec2 &pos);
    void queueRelease(SceneState *sceneState, const PickingId &id,
                      const glm::vec2 &pos);
    void queueClick(SceneState *sceneState, const PickingId &id);
    bool queueKey(SceneState *sceneState, const SceneEvent &evt);
    bool queueWheel(SceneState *sceneState, const SceneEvent &evt);
    void clearFocus(SceneState *sceneState);
    void setHoverId(SceneState *sceneState, const PickingId &id);

    bool toggleButton(const PickingId &id, bool value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context);

    bool toggleButton(const PickingId &id, bool *value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context);

    bool button(const PickingId &id, const std::string &label,
                const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                const Core::Renderer::Color &labelColor,
                SceneDrawContext &context);

    TextBoxResult textBox(const PickingId &id, std::string *value,
                          const glm::vec3 &boxPos,
                          const glm::vec2 &boxSize,
                          SceneDrawContext &context,
                          const TextBoxOptions &options = {});

    SliderResult sliderFloat(const PickingId &id, float *value, float minValue,
                             float maxValue, const glm::vec3 &sliderPos,
                             const glm::vec2 &sliderSize,
                             SceneDrawContext &context,
                             const SliderOptions &options = {});

    SliderResult sliderInt(const PickingId &id, int *value, int minValue,
                           int maxValue, const glm::vec3 &sliderPos,
                           const glm::vec2 &sliderSize,
                           SceneDrawContext &context,
                           const SliderOptions &options = {});

    DropdownResult dropdown(const PickingId &id, int *selectedIndex,
                            std::span<const std::string_view> items,
                            const glm::vec3 &boxPos,
                            const glm::vec2 &boxSize,
                            SceneDrawContext &context,
                            const DropdownOptions &options = {});
} // namespace Bess::Canvas::SceneWidgets
