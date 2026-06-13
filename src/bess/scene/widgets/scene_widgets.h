#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "common/types.h"
#include "scene_draw_context.h"
#include <glm.hpp>
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

    void beginFrame(SceneState *sceneState);
    void endFrame(SceneState *sceneState);
    void clearScene(SceneState *sceneState);

    bool contains(const SceneState *sceneState, const PickingId &id);
    bool isTextInput(const SceneState *sceneState, const PickingId &id);
    bool hasPointerCapture(const SceneState *sceneState);
    bool wantsKeyboard(const SceneState *sceneState = nullptr);

    void queuePress(SceneState *sceneState, const PickingId &id);
    void queueRelease(SceneState *sceneState, const PickingId &id);
    void queueClick(SceneState *sceneState, const PickingId &id);
    bool queueKey(SceneState *sceneState, const SceneEvent &evt);
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
} // namespace Bess::Canvas::SceneWidgets
