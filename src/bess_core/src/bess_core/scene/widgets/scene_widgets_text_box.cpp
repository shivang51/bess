#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_draw_helpers.h"
#include "bess_core/scene/widgets/scene_widgets_internal.h"
#include "bess_core/settings/viewport_theme.h"
#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace Bess::Canvas::SceneWidgets {
    namespace {
        bool isUtf8Continuation(char ch) {
            return (static_cast<unsigned char>(ch) & 0xC0) == 0x80;
        }

        size_t previousCharBoundary(std::string_view text, size_t cursor) {
            cursor = std::min(cursor, text.size());
            if (cursor == 0) {
                return 0;
            }

            --cursor;
            while (cursor > 0 && isUtf8Continuation(text[cursor])) {
                --cursor;
            }
            return cursor;
        }

        size_t nextCharBoundary(std::string_view text, size_t cursor) {
            cursor = std::min(cursor, text.size());
            if (cursor >= text.size()) {
                return text.size();
            }

            ++cursor;
            while (cursor < text.size() && isUtf8Continuation(text[cursor])) {
                ++cursor;
            }
            return cursor;
        }

        size_t nextStartBoundary(std::string_view text, size_t cursor) {
            cursor = std::min(cursor, text.size());
            while (cursor < text.size() && isUtf8Continuation(text[cursor])) {
                ++cursor;
            }
            return cursor;
        }

        size_t utf8PrefixBoundary(std::string_view text, size_t maxBytes) {
            maxBytes = std::min(maxBytes, text.size());
            while (maxBytes > 0 && maxBytes < text.size() &&
                   isUtf8Continuation(text[maxBytes])) {
                --maxBytes;
            }
            return maxBytes;
        }

        std::string boundedText(std::string_view text, size_t maxBytes) {
            const size_t size = text.size() <= maxBytes
                                    ? text.size()
                                    : utf8PrefixBoundary(text, maxBytes);
            return std::string(text.substr(0, size));
        }

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
            return nextStartBoundary(text, low);
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
            return low == text.size() ? low
                                      : previousCharBoundary(text, low + 1);
        }

        std::pair<size_t, size_t> visibleRangeForCursor(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            std::string_view text,
            size_t cursor,
            float maxWidth,
            const Core::Renderer::FontProps &fontProps) {
            if (text.empty() || maxWidth <= 1.f) {
                return {0, 0};
            }

            if (renderer->measureText(text, fontProps).x <= maxWidth) {
                return {0, text.size()};
            }

            const size_t visibleStart =
                findVisibleStart(renderer, text, cursor, maxWidth, fontProps);
            const size_t visibleEnd = findVisibleEnd(
                renderer, text, visibleStart, cursor, maxWidth, fontProps);
            return {visibleStart, visibleEnd};
        }

        size_t cursorIndexForPointer(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            std::string_view text,
            size_t visibleStart,
            size_t visibleEnd,
            float pointerX,
            float left,
            const Core::Renderer::FontProps &fontProps) {
            visibleStart = std::min(visibleStart, text.size());
            visibleEnd = std::min(visibleEnd, text.size());
            if (visibleStart > visibleEnd) {
                std::swap(visibleStart, visibleEnd);
            }

            if (pointerX <= left || visibleStart == visibleEnd) {
                return visibleStart;
            }

            float x = left;
            size_t cursor = visibleStart;
            while (cursor < visibleEnd) {
                const size_t next = nextCharBoundary(text, cursor);
                const float charWidth =
                    renderer
                        ->measureText(text.substr(cursor, next - cursor),
                                      fontProps)
                        .x;
                if (pointerX < x + (charWidth * 0.5f)) {
                    return cursor;
                }

                x += charWidth;
                cursor = next;
            }
            return visibleEnd;
        }

        void updatePointerSelection(
            Detail::WidgetState &widget,
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            float left,
            float contentWidth,
            const Core::Renderer::FontProps &fontProps) {
            if (!widget.pointerInputQueued ||
                (!widget.textPointerSelectionStarted &&
                 !widget.textPointerSelecting)) {
                return;
            }

            const auto [visibleStart, visibleEnd] =
                visibleRangeForCursor(renderer,
                                      widget.text,
                                      widget.cursorPos,
                                      contentWidth,
                                      fontProps);
            const size_t nextCursor = cursorIndexForPointer(renderer,
                                                            widget.text,
                                                            visibleStart,
                                                            visibleEnd,
                                                            widget.pointerPos.x,
                                                            left,
                                                            fontProps);

            if (widget.textPointerSelectionStarted &&
                !widget.textPointerExtendSelection) {
                widget.selectionAnchorPos = nextCursor;
            }

            widget.cursorPos = nextCursor;
            if (!widget.isPressed) {
                widget.textPointerSelecting = false;
            }
            widget.textPointerSelectionStarted = false;
            widget.textPointerExtendSelection = false;
            widget.pointerInputQueued = false;
            Detail::clampCursor(widget);
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

        auto widget =
            Detail::registerWidget(context.sceneWidgetsState,
                                   id,
                                   Detail::WidgetState::Type::textInput);
        if (widget == nullptr) {
            return result;
        }

        widget->maxLength = options.maxLength;

        if (!widget->textInitialized ||
            (!widget->isFocused && widget->text != *value)) {
            widget->text = boundedText(*value, options.maxLength);
            widget->cursorPos = widget->text.size();
            Detail::clearTextSelection(*widget);
            widget->textInitialized = true;
        }

        if (widget->focusStarted) {
            widget->text = boundedText(*value, options.maxLength);
            widget->focusStartText = widget->text;
            widget->cursorPos = widget->text.size();
            Detail::clearTextSelection(*widget);
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
        const bool focused = Detail::isFocused(context.sceneWidgetsState, id);
        auto bgColor =
            focused ? Detail::colorOr(options.focusedBackgroundColor,
                                      palette.surfaceActive)
                    : Detail::colorOr(options.backgroundColor, palette.surface);
        if (!focused && Detail::isHovering(context.sceneWidgetsState, id)) {
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

        if (focused) {
            updatePointerSelection(
                *widget, context.renderer, left, contentWidth, fontProps);
        }

        const auto [visibleStart, visibleEnd] =
            visibleRangeForCursor(context.renderer,
                                  widget->text,
                                  widget->cursorPos,
                                  contentWidth,
                                  fontProps);
        std::string_view visibleText =
            std::string_view(widget->text)
                .substr(visibleStart, visibleEnd - visibleStart);
        Core::Renderer::Color textColor =
            Detail::colorOr(options.textColor, palette.text);

        if (widget->text.empty() && !options.placeholder.empty()) {
            visibleText = std::string_view(options.placeholder);
            textColor =
                Detail::colorOr(options.placeholderColor, palette.textMuted);
        }

        const float cursorHeight =
            std::max(4.f, size.y - (options.padding.y * 2.f));
        if (focused && Detail::hasTextSelection(*widget)) {
            const auto [selStart, selEnd] = Detail::textSelectionRange(*widget);
            const size_t visibleSelStart =
                std::max(selStart, visibleStart);
            const size_t visibleSelEnd =
                std::min(selEnd, visibleEnd);

            if (visibleSelStart < visibleSelEnd) {
                const auto text = std::string_view(widget->text);
                const auto prefix = text.substr(
                    visibleStart, visibleSelStart - visibleStart);
                const auto selected = text.substr(
                    visibleSelStart, visibleSelEnd - visibleSelStart);
                const float selectionX =
                    left + context.renderer->measureText(prefix, fontProps).x;
                const float selectionWidth = std::max(
                    1.f, context.renderer->measureText(selected, fontProps).x);
                SceneDraw::drawQuad(context,
                                    {selectionX + (selectionWidth * 0.5f),
                                     boxPos.y,
                                     boxPos.z + 0.00015f},
                                    {selectionWidth, cursorHeight},
                                    palette.accent.withAlpha(0.45f),
                                    id);
            }
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
            const size_t visibleCursor =
                std::clamp(widget->cursorPos, visibleStart, visibleEnd);
            const auto cursorText =
                std::string_view(widget->text)
                    .substr(visibleStart, visibleCursor - visibleStart);
            const float cursorX =
                left + context.renderer->measureText(cursorText, fontProps).x;
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
