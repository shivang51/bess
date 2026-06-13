#include "bess_core/renderer/colors.h"
#include "scene/scene_draw_helpers.h"
#include "scene_widgets_internal.h"
#include "settings/viewport_theme.h"

namespace Bess::Canvas::SceneWidgets {
    namespace {
        void drawToggleButton(const PickingId &id, bool isHigh,
                              const glm::vec3 &buttonPos,
                              const glm::vec2 &buttonSize,
                              SceneDrawContext &context) {
            static const SceneDraw::QuadStyle trackProps{
                .borderColor = ViewportTheme::colors.componentBorder,
                .borderRadius = glm::vec4(5.5f),
                .borderSize = glm::vec4(0.5f),
            };
            constexpr SceneDraw::QuadStyle buttonProps{.borderRadius =
                                                           glm::vec4(5.f)};

            auto trackColor = isHigh ? ViewportTheme::colors.stateHigh
                                     : ViewportTheme::colors.background;
            if (Detail::isHovering(context.sceneState, id)) {
                trackColor = Core::Renderer::Color(trackColor) * 1.15f;
            }
            if (Detail::isPressed(context.sceneState, id)) {
                trackColor = Core::Renderer::Color(trackColor) * 0.85f;
            }

            SceneDraw::drawQuad(context, buttonPos, buttonSize, trackColor, id,
                                trackProps);

            const float buttonHeadPosX =
                isHigh
                    ? buttonPos.x + (buttonSize.x / 2.f) - (buttonSize.y / 2.f)
                    : buttonPos.x - (buttonSize.x / 2.f) + (buttonSize.y / 2.f);

            const glm::vec3 buttonHeadPos =
                glm::vec3(buttonHeadPosX, buttonPos.y, buttonPos.z);
            SceneDraw::drawQuad(context, buttonHeadPos,
                                {buttonSize.y - 1.f, buttonSize.y - 1.f},
                                ViewportTheme::colors.stateLow, id,
                                buttonProps);
        }
    } // namespace

    bool toggleButton(const PickingId &id, bool value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context) {
        Detail::registerWidget(context.sceneState, id,
                               Detail::WidgetState::Type::toggleButton);
        drawToggleButton(id, value, buttonPos, buttonSize, context);
        return Detail::consumeClick(context.sceneState, id);
    }

    bool toggleButton(const PickingId &id, bool *value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
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

    bool button(const PickingId &id, const std::string &label,
                const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                const Core::Renderer::Color &labelColor,
                SceneDrawContext &context) {
        constexpr float paddingY = 2.f;
        constexpr float paddingX = 3.f;

        Detail::registerWidget(context.sceneState, id,
                               Detail::WidgetState::Type::button);

        static const SceneDraw::QuadStyle buttonProps{
            .borderColor = Core::Renderer::Colors::slate700,
            .borderRadius = glm::vec4(2.f),
            .borderSize = glm::vec4(0.5f),
        };

        auto bgColor = Core::Renderer::Colors::slate900;
        if (Detail::isHovering(context.sceneState, id)) {
            bgColor = bgColor * 1.2f;
        }
        if (Detail::isPressed(context.sceneState, id)) {
            bgColor = bgColor * 0.8f;
        }

        const auto textSize = context.renderer->measureText(
            label, {.fontSize = Detail::kDefaultButtonTextSize});

        const float textOffY = context.renderer->textCenterOffsetY(
            label, {.fontSize = Detail::kDefaultButtonTextSize});

        auto size = buttonSize;

        if (size.y == 0.f) {
            size.y = textSize.y + (paddingY * 2.f);
        }

        if (size.x == 0.f) {
            size.x = textSize.x + (paddingX * 2.f);
        }

        SceneDraw::drawQuad(context, buttonPos, size, bgColor, id, buttonProps);

        const auto textPos =
            glm::vec3(buttonPos.x - (textSize.x / 2.f), buttonPos.y + textOffY,
                      buttonPos.z + 0.0001f);
        SceneDraw::drawText(context, label, textPos,
                            Detail::kDefaultButtonTextSize, labelColor, id);

        return Detail::consumeClick(context.sceneState, id);
    }
} // namespace Bess::Canvas::SceneWidgets
