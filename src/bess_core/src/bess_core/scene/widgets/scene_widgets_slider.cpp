#include "scene/scene_draw_helpers.h"
#include "scene_widgets_internal.h"
#include "settings/viewport_theme.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>

namespace Bess::Canvas::SceneWidgets {
    namespace {
        constexpr float kMinSliderWidth = 72.f;
        constexpr float kMinSliderHeight = 18.f;

        float sanitizeValue(float value, float fallback) {
            return std::isfinite(value) ? value : fallback;
        }

        float
        snappedValue(float value, float minValue, float maxValue, float step) {
            value = std::clamp(value, minValue, maxValue);
            if (step <= 0.f || maxValue <= minValue) {
                return value;
            }

            const float steps = std::round((value - minValue) / step);
            return std::clamp(minValue + (steps * step), minValue, maxValue);
        }

        float valueToNormalized(float value, float minValue, float maxValue) {
            if (maxValue <= minValue) {
                return 0.f;
            }
            return std::clamp(
                (value - minValue) / (maxValue - minValue), 0.f, 1.f);
        }

        float pointerToValue(const Detail::WidgetState &widget,
                             float minValue,
                             float maxValue,
                             float valueTextWidth,
                             const SliderOptions &options) {
            const float left = widget.boundsPos.x -
                               (widget.boundsSize.x * 0.5f) + options.padding.x;
            const float right = widget.boundsPos.x +
                                (widget.boundsSize.x * 0.5f) -
                                options.padding.x - valueTextWidth;
            const float width = std::max(1.f, right - left);
            const float t =
                std::clamp((widget.pointerPos.x - left) / width, 0.f, 1.f);
            return minValue + ((maxValue - minValue) * t);
        }

        std::string formatValue(float value, int precision) {
            std::array<char, 64> buffer{};
            std::to_chars_result result{};
            if (precision <= 0) {
                result =
                    std::to_chars(buffer.data(),
                                  buffer.data() + buffer.size(),
                                  static_cast<long long>(std::llround(value)));
            } else {
                result = std::to_chars(buffer.data(),
                                       buffer.data() + buffer.size(),
                                       value,
                                       std::chars_format::fixed,
                                       precision);
            }

            if (result.ec != std::errc{}) {
                return {};
            }
            return {buffer.data(), result.ptr};
        }

        glm::vec2 resolveSliderSize(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            const glm::vec2 &requestedSize,
            const SliderOptions &options) {
            auto size = requestedSize;
            const auto textSize =
                renderer->measureText("0", {.fontSize = options.fontSize});

            if (size.x <= 0.f) {
                size.x = kMinSliderWidth;
            }
            if (size.y <= 0.f) {
                size.y =
                    std::max(kMinSliderHeight,
                             std::max(textSize.y, options.knobRadius * 2.f) +
                                 (options.padding.y * 2.f));
            }
            return size;
        }

        float valueTextWidth(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            const std::string &valueLabel,
            const SliderOptions &options) {
            if (!options.showValue) {
                return 0.f;
            }
            return renderer
                       ->measureText(valueLabel, {.fontSize = options.fontSize})
                       .x +
                   options.padding.x;
        }

        bool applySliderInput(Detail::WidgetState &widget,
                              float *value,
                              float minValue,
                              float maxValue,
                              float valueTextWidth,
                              const SliderOptions &options) {
            if (value == nullptr || maxValue <= minValue) {
                widget.pointerInputQueued = false;
                widget.sliderKeyboardDelta = 0;
                widget.sliderSetToMin = false;
                widget.sliderSetToMax = false;
                return false;
            }

            const float range = maxValue - minValue;
            const float step = options.step > 0.f
                                   ? options.step
                                   : std::max(range / 100.f, 0.f);
            float next = sanitizeValue(*value, minValue);
            bool hasInput = false;

            if (widget.pointerInputQueued) {
                next = pointerToValue(
                    widget, minValue, maxValue, valueTextWidth, options);
                hasInput = true;
            }

            if (widget.sliderSetToMin) {
                next = minValue;
                hasInput = true;
            } else if (widget.sliderSetToMax) {
                next = maxValue;
                hasInput = true;
            } else if (widget.sliderKeyboardDelta != 0) {
                next += step * static_cast<float>(widget.sliderKeyboardDelta);
                hasInput = true;
            }

            widget.pointerInputQueued = false;
            widget.sliderKeyboardDelta = 0;
            widget.sliderSetToMin = false;
            widget.sliderSetToMax = false;

            if (!hasInput) {
                return false;
            }

            next = snappedValue(next, minValue, maxValue, options.step);
            const float epsilon = std::max(
                std::numeric_limits<float>::epsilon() * 8.f, range * 0.000001f);
            if (std::abs(next - *value) <= epsilon) {
                return false;
            }

            *value = next;
            return true;
        }

        void drawSlider(Detail::WidgetState &widget,
                        const PickingId &id,
                        float value,
                        float minValue,
                        float maxValue,
                        const std::string &valueLabel,
                        float valueTextWidth,
                        const glm::vec3 &sliderPos,
                        const glm::vec2 &size,
                        SceneDrawContext &context,
                        const SliderOptions &options) {
            const auto &palette = ViewportTheme::sceneWidgetsColors;
            const bool focused = widget.isFocused;
            auto bgColor =
                widget.isHovered
                    ? Detail::colorOr(options.hoverBackgroundColor,
                                      palette.surfaceHover)
                    : Detail::colorOr(options.backgroundColor, palette.surface);
            if (widget.isPressed) {
                bgColor = options.backgroundColor.has_value()
                              ? *options.backgroundColor * 0.85f
                              : Core::Renderer::Color(palette.surfaceActive);
            }

            const SceneDraw::QuadStyle backgroundStyle{
                .borderColor = focused
                                   ? Detail::colorOr(options.focusedBorderColor,
                                                     palette.borderFocus)
                                   : Core::Renderer::Color(0.f, 0.f, 0.f, 0.f),
                .borderRadius = glm::vec4(2.f),
                .borderSize = glm::vec4(focused ? 0.8f : 0.f),
            };

            SceneDraw::drawQuad(
                context, sliderPos, size, bgColor, id, backgroundStyle);

            const float left =
                sliderPos.x - (size.x * 0.5f) + options.padding.x;
            const float right = sliderPos.x + (size.x * 0.5f) -
                                options.padding.x - valueTextWidth;
            const float trackWidth = std::max(1.f, right - left);
            const float trackCenterX = left + (trackWidth * 0.5f);
            const float t = valueToNormalized(value, minValue, maxValue);
            const float knobX = left + (trackWidth * t);

            const SceneDraw::QuadStyle trackStyle{
                .borderRadius = glm::vec4(options.trackHeight * 0.5f),
            };
            SceneDraw::drawQuad(
                context,
                {trackCenterX, sliderPos.y, sliderPos.z + 0.0001f},
                {trackWidth, options.trackHeight},
                Detail::colorOr(options.trackColor, palette.track),
                id,
                trackStyle);

            const float fillWidth = std::max(0.5f, knobX - left);
            SceneDraw::drawQuad(
                context,
                {left + (fillWidth * 0.5f), sliderPos.y, sliderPos.z + 0.0002f},
                {fillWidth, options.trackHeight},
                Detail::colorOr(options.fillColor, palette.accent),
                id,
                trackStyle);

            SceneDraw::drawCircle(
                context,
                {knobX, sliderPos.y, sliderPos.z + 0.0003f},
                options.knobRadius,
                options.knobColor.has_value()
                    ? *options.knobColor
                    : Core::Renderer::Color(widget.isPressed ? palette.accent
                                                             : palette.knob),
                id);

            if (options.showValue) {
                const float textOffY = context.renderer->textCenterOffsetY(
                    valueLabel, {.fontSize = options.fontSize});
                const glm::vec3 textPos{
                    right + options.padding.x,
                    sliderPos.y + textOffY,
                    sliderPos.z + 0.0004f,
                };
                SceneDraw::drawText(
                    context,
                    valueLabel,
                    textPos,
                    static_cast<size_t>(options.fontSize),
                    Detail::colorOr(options.textColor, palette.textMuted),
                    id);
            }
        }
    } // namespace

    SliderResult sliderFloat(const PickingId &id,
                             float *value,
                             float minValue,
                             float maxValue,
                             const glm::vec3 &sliderPos,
                             const glm::vec2 &sliderSize,
                             SceneDrawContext &context,
                             const SliderOptions &options) {
        SliderResult result;
        if (value == nullptr || context.renderer == nullptr) {
            return result;
        }

        const float rangeMin = std::min(minValue, maxValue);
        const float rangeMax = std::max(minValue, maxValue);
        *value = snappedValue(
            sanitizeValue(*value, rangeMin), rangeMin, rangeMax, options.step);

        auto widget = Detail::registerWidget(context.sceneState,
                                             id,
                                             Detail::WidgetState::Type::slider,
                                             context.viewportId);
        if (widget == nullptr) {
            return result;
        }

        const auto size =
            resolveSliderSize(context.renderer, sliderSize, options);
        widget->boundsPos = sliderPos;
        widget->boundsSize = size;

        auto valueLabel = formatValue(*value, options.precision);
        auto reservedValueWidth =
            valueTextWidth(context.renderer, valueLabel, options);

        result.changed = applySliderInput(
            *widget, value, rangeMin, rangeMax, reservedValueWidth, options);

        valueLabel = formatValue(*value, options.precision);
        reservedValueWidth =
            valueTextWidth(context.renderer, valueLabel, options);

        drawSlider(*widget,
                   id,
                   *value,
                   rangeMin,
                   rangeMax,
                   valueLabel,
                   reservedValueWidth,
                   sliderPos,
                   size,
                   context,
                   options);

        result.editing = widget->isPressed;
        result.focused = widget->isFocused;
        return result;
    }

    SliderResult sliderInt(const PickingId &id,
                           int *value,
                           int minValue,
                           int maxValue,
                           const glm::vec3 &sliderPos,
                           const glm::vec2 &sliderSize,
                           SceneDrawContext &context,
                           const SliderOptions &options) {
        SliderResult result;
        if (value == nullptr) {
            return result;
        }

        const int rangeMin = std::min(minValue, maxValue);
        const int rangeMax = std::max(minValue, maxValue);
        *value = std::clamp(*value, rangeMin, rangeMax);

        auto intOptions = options;
        intOptions.step = intOptions.step > 0.f ? intOptions.step : 1.f;
        intOptions.precision = 0;

        float floatValue = static_cast<float>(*value);
        result = sliderFloat(id,
                             &floatValue,
                             static_cast<float>(rangeMin),
                             static_cast<float>(rangeMax),
                             sliderPos,
                             sliderSize,
                             context,
                             intOptions);

        const int nextValue = std::clamp(
            static_cast<int>(std::lround(floatValue)), rangeMin, rangeMax);
        result.changed = nextValue != *value;
        if (result.changed) {
            *value = nextValue;
        }
        return result;
    }
} // namespace Bess::Canvas::SceneWidgets
