#include "bess_core/renderer/renderer_types.h"
#include "scene/scene_draw_helpers.h"
#include "scene_widgets_internal.h"
#include "settings/viewport_theme.h"
#include <cstddef>

namespace Bess::Canvas::SceneWidgets {
    namespace {
        void drawToggleButton(const PickingId &id,
                              bool isHigh,
                              const glm::vec3 &buttonPos,
                              const glm::vec2 &buttonSize,
                              SceneDrawContext &context) {
            const auto &palette = ViewportTheme::sceneWidgetsColors;
            static const SceneDraw::QuadStyle trackProps{
                .borderRadius = glm::vec4(5.5f),
                .borderSize = glm::vec4(0.5f),
            };
            constexpr SceneDraw::QuadStyle buttonProps{.borderRadius =
                                                           glm::vec4(5.f)};

            auto style = trackProps;
            style.borderColor = palette.border;

            auto trackColor = isHigh ? Core::Renderer::Color(palette.accent)
                                     : Core::Renderer::Color(palette.track);
            if (Detail::isHovering(context.sceneState, id)) {
                trackColor = Core::Renderer::Color(trackColor) * 1.15f;
            }
            if (Detail::isPressed(context.sceneState, id)) {
                trackColor = Core::Renderer::Color(trackColor) * 0.85f;
            }

            SceneDraw::drawQuad(
                context, buttonPos, buttonSize, trackColor, id, style);

            const float buttonHeadPosX =
                isHigh
                    ? buttonPos.x + (buttonSize.x / 2.f) - (buttonSize.y / 2.f)
                    : buttonPos.x - (buttonSize.x / 2.f) + (buttonSize.y / 2.f);

            const glm::vec3 buttonHeadPos =
                glm::vec3(buttonHeadPosX, buttonPos.y, buttonPos.z);
            SceneDraw::drawQuad(context,
                                buttonHeadPos,
                                {buttonSize.y - 1.f, buttonSize.y - 1.f},
                                palette.knob,
                                id,
                                buttonProps);
        }
    } // namespace

    bool toggleButton(const PickingId &id,
                      bool value,
                      const glm::vec3 &buttonPos,
                      const glm::vec2 &buttonSize,
                      SceneDrawContext &context) {
        Detail::registerWidget(
            context.sceneState, id, Detail::WidgetState::Type::toggleButton);
        drawToggleButton(id, value, buttonPos, buttonSize, context);
        return Detail::consumeClick(context.sceneState, id);
    }

    bool toggleButton(const PickingId &id,
                      bool *value,
                      const glm::vec3 &buttonPos,
                      const glm::vec2 &buttonSize,
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

    bool button(const PickingId &id,
                std::string_view label,
                const glm::vec3 &buttonPos,
                SceneDrawContext &context,
                const ButtonOptions &options) {
        Detail::registerWidget(
            context.sceneState, id, Detail::WidgetState::Type::button);

        const auto &palette = ViewportTheme::sceneWidgetsColors;

        auto bgColor = Core::Renderer::Color(palette.surface);
        if (Detail::isHovering(context.sceneState, id)) {
            bgColor = Detail::colorOr(options.hoverBackgroundColor,
                                      palette.surfaceHover);
        }
        if (Detail::isPressed(context.sceneState, id)) {
            bgColor = Detail::colorOr(options.pressedBackgroundColor,
                                      palette.surfaceActive);
        }

        const auto textSize =
            context.renderer->measureText(label,
                                          {
                                              .fontSize = options.textSize,
                                          });

        const float textOffY = context.renderer->textCenterOffsetY(
            label,
            {
                .fontSize = options.textSize,
            });

        auto size = options.buttonSize;
        if (size.x == 0.f) {
            size.x = textSize.x + (options.padding.x * 2.f);
        }
        if (size.y == 0.f) {
            size.y = textSize.y + (options.padding.y * 2.f);
        }

        const Core::Renderer::QuadProps btnProps{
            .position = glm::vec2(buttonPos.x, buttonPos.y),
            .size = size,
            .zIndex = buttonPos.z,
            .color = bgColor,
            .id = id,
            .transformMode = context.transformMode,
            .radius = options.borderRadius,
            .thickness = options.borderThickness,
            .borderColor = Detail::colorOr(options.borderColor, palette.border),
            .shadow = options.shadow,
        };

        context.renderer->drawQuad(btnProps);

        const auto textPos = glm::vec3(buttonPos.x - (textSize.x / 2.f),
                                       buttonPos.y + textOffY,
                                       buttonPos.z + 0.0001f);

        const auto textColor = options.textColor.has_value()
                                   ? options.textColor.value()
                                   : Core::Renderer::Color(palette.text);
        SceneDraw::drawText(
            context, label, textPos, (size_t)options.textSize, textColor, id);

        return Detail::consumeClick(context.sceneState, id);
    }
} // namespace Bess::Canvas::SceneWidgets
