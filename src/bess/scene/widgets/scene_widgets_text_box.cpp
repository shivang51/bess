#include "bess_core/renderer/renderer_2d.h"
#include "scene/scene_draw_helpers.h"
#include "scene_widgets_internal.h"
#include "settings/viewport_theme.h"
#include <algorithm>
#include <string_view>

namespace Bess::Canvas::SceneWidgets {
    namespace {
        size_t findVisibleStart(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            std::string_view text,
            size_t cursor,
            float maxWidth,
            const Core::Renderer::FontProps &fontProps) {
            cursor = std::min(cursor, text.size());
            if (cursor == 0 ||
                renderer->measureText(text.substr(0, cursor), fontProps).x <=
                    maxWidth) {
                return 0;
            }

            size_t low = 0;
            size_t high = cursor;
            while (low < high) {
                const size_t mid = (low + high) / 2;
                if (renderer
                        ->measureText(text.substr(mid, cursor - mid), fontProps)
                        .x <= maxWidth) {
                    high = mid;
                } else {
                    low = mid + 1;
                }
            }
            return low;
        }

        size_t findVisibleEnd(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            std::string_view text,
            size_t start,
            size_t cursor,
            float maxWidth,
            const Core::Renderer::FontProps &fontProps) {
            size_t low = std::min(cursor, text.size());
            size_t high = text.size();
            while (low < high) {
                const size_t mid = (low + high + 1) / 2;
                if (renderer
                        ->measureText(text.substr(start, mid - start),
                                      fontProps)
                        .x <= maxWidth) {
                    low = mid;
                } else {
                    high = mid - 1;
                }
            }
            return low;
        }

        std::string_view visibleTextForCursor(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            std::string_view text,
            size_t cursor,
            float maxWidth,
            const Core::Renderer::FontProps &fontProps,
            size_t &visibleStart) {
            if (text.empty() || maxWidth <= 1.f) {
                visibleStart = 0;
                return {};
            }

            if (renderer->measureText(text, fontProps).x <= maxWidth) {
                visibleStart = 0;
                return text;
            }

            visibleStart =
                findVisibleStart(renderer, text, cursor, maxWidth, fontProps);
            const size_t visibleEnd = findVisibleEnd(
                renderer, text, visibleStart, cursor, maxWidth, fontProps);
            return text.substr(visibleStart, visibleEnd - visibleStart);
        }
    } // namespace

    TextBoxResult textBox(const PickingId &id,
                          std::string *value,
                          const glm::vec3 &boxPos,
                          const glm::vec2 &boxSize,
                          SceneDrawContext &context,
                          const TextBoxOptions &options) {
        TextBoxResult result;
        if (value == nullptr || context.renderer == nullptr) {
            return result;
        }

        auto widget = Detail::registerWidget(
            context.sceneState, id, Detail::WidgetState::Type::textInput);
        if (widget == nullptr) {
            return result;
        }

        widget->maxLength = options.maxLength;

        if (!widget->textInitialized ||
            (!widget->isFocused && widget->text != *value)) {
            widget->text = value->substr(0, options.maxLength);
            widget->cursorPos = widget->text.size();
            widget->textInitialized = true;
        }

        if (widget->focusStarted) {
            widget->text = value->substr(0, options.maxLength);
            widget->focusStartText = widget->text;
            widget->cursorPos = widget->text.size();
            widget->focusStarted = false;
        }

        Detail::clampCursor(*widget);

        if (widget->textChanged || widget->textSubmitted) {
            *value = widget->text;
        }

        result = {
            .changed = widget->textChanged,
            .submitted = widget->textSubmitted,
            .canceled = widget->textCanceled,
            .focused = widget->isFocused,
        };

        auto size = boxSize;
        const auto referenceTextSize =
            context.renderer->measureText("M", {.fontSize = options.fontSize});

        if (size.y == 0.f) {
            size.y = referenceTextSize.y + (options.padding.y * 2.f);
        }

        if (size.x == 0.f) {
            const auto measuredText = context.renderer->measureText(
                value->empty() ? std::string_view(options.placeholder)
                               : std::string_view(*value),
                {.fontSize = options.fontSize});
            size.x = std::max(48.f, measuredText.x + (options.padding.x * 2.f));
        }

        const auto &palette = ViewportTheme::sceneWidgetsColors;
        const bool focused = Detail::isFocused(context.sceneState, id);
        auto bgColor =
            focused ? Detail::colorOr(options.focusedBackgroundColor,
                                      palette.surfaceActive)
                    : Detail::colorOr(options.backgroundColor, palette.surface);
        if (!focused && Detail::isHovering(context.sceneState, id)) {
            bgColor = Detail::colorOr(options.hoverBackgroundColor,
                                      palette.surfaceHover);
        }

        const SceneDraw::QuadStyle style{
            .borderColor =
                focused ? Detail::colorOr(options.focusedBorderColor,
                                          palette.borderFocus)
                        : Detail::colorOr(options.borderColor, palette.border),
            .borderRadius = glm::vec4(2.f),
            .borderSize = glm::vec4(focused ? 0.8f : 0.5f),
        };

        SceneDraw::drawQuad(context, boxPos, size, bgColor, id, style);

        const float contentWidth =
            std::max(1.f, size.x - (options.padding.x * 2.f));
        const float left = boxPos.x - (size.x * 0.5f) + options.padding.x;
        const Core::Renderer::FontProps fontProps{
            .fontSize = options.fontSize,
        };

        size_t visibleStart = 0;
        std::string_view visibleText;
        Core::Renderer::Color textColor =
            Detail::colorOr(options.textColor, palette.text);

        if (widget->text.empty() && !focused && !options.placeholder.empty()) {
            visibleText = std::string_view(options.placeholder);
            textColor =
                Detail::colorOr(options.placeholderColor, palette.textMuted);
        } else {
            visibleText = visibleTextForCursor(context.renderer,
                                               widget->text,
                                               widget->cursorPos,
                                               contentWidth,
                                               fontProps,
                                               visibleStart);
        }

        const float textOffY = context.renderer->textCenterOffsetY(
            visibleText.empty() ? std::string_view("M") : visibleText,
            {.fontSize = options.fontSize});
        const glm::vec3 textPos{
            left,
            boxPos.y + textOffY,
            boxPos.z + 0.0001f,
        };
        SceneDraw::drawText(context,
                            visibleText,
                            textPos,
                            static_cast<size_t>(options.fontSize),
                            textColor,
                            id);

        if (focused) {
            const auto cursorText =
                std::string_view(widget->text)
                    .substr(visibleStart, widget->cursorPos - visibleStart);
            const float cursorX =
                left + context.renderer->measureText(cursorText, fontProps).x;
            const float cursorHeight =
                std::max(4.f, size.y - (options.padding.y * 2.f));
            SceneDraw::drawQuad(
                context,
                {cursorX, boxPos.y, boxPos.z + 0.0002f},
                {1.f, cursorHeight},
                Detail::colorOr(options.cursorColor, palette.text),
                id);
        }

        return result;
    }
} // namespace Bess::Canvas::SceneWidgets
