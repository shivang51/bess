#include "scene/scene_draw_helpers.h"
#include "scene_widgets_internal.h"
#include "settings/viewport_theme.h"
#include <algorithm>
#include <string_view>

namespace Bess::Canvas::SceneWidgets {
    namespace {
        constexpr float kMinDropdownWidth = 72.f;
        constexpr float kMinDropdownHeight = 18.f;

        glm::vec2 resolveDropdownSize(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            const std::vector<std::string> &items,
            const glm::vec2 &requestedSize, const DropdownOptions &options) {
            auto size = requestedSize;
            const auto referenceSize =
                renderer->measureText("M", {.fontSize = options.fontSize});

            if (size.y <= 0.f) {
                size.y = std::max(kMinDropdownHeight,
                                  referenceSize.y + (options.padding.y * 2.f));
            }

            if (size.x <= 0.f) {
                float maxTextWidth =
                    renderer
                        ->measureText(options.placeholder,
                                      {.fontSize = options.fontSize})
                        .x;
                for (const auto &item : items) {
                    maxTextWidth = std::max(
                        maxTextWidth,
                        renderer
                            ->measureText(item, {.fontSize = options.fontSize})
                            .x);
                }
                size.x = maxTextWidth + (options.padding.x * 3.f) + 8.f;
            }

            size.x = std::max(size.x, kMinDropdownWidth);
            size.y = std::max(size.y, kMinDropdownHeight);
            return size;
        }

        size_t visibleOptionCount(size_t itemCount,
                                  const DropdownOptions &options) {
            if (itemCount == 0) {
                return 0;
            }
            if (options.maxVisibleOptions == 0) {
                return itemCount;
            }
            return std::min(itemCount, options.maxVisibleOptions);
        }

        int clampedSelection(int selectedIndex, size_t itemCount) {
            if (selectedIndex < 0 || itemCount == 0) {
                return -1;
            }
            return std::min(selectedIndex, static_cast<int>(itemCount - 1));
        }

        glm::vec3 optionCenter(const glm::vec3 &boxPos,
                               const glm::vec2 &boxSize, float optionHeight,
                               size_t visibleIndex) {
            return {boxPos.x,
                    boxPos.y + (boxSize.y * 0.5f) + (optionHeight * 0.5f) +
                        (static_cast<float>(visibleIndex) * optionHeight),
                    boxPos.z + 0.006f};
        }

        void openDropdown(Detail::SceneWidgetsState &widgetsState,
                          Detail::WidgetState &widget, uint64_t widgetId,
                          int selectedIndex) {
            Detail::closeDropdowns(widgetsState, widgetId);
            if (!widget.dropdownOpen) {
                widget.dropdownOpen = true;
                widget.dropdownOpened = true;
            }

            if (selectedIndex >= 0) {
                widget.dropdownHighlightedIndex =
                    static_cast<size_t>(selectedIndex);
            }
            Detail::ensureDropdownHighlightVisible(widget);
        }

        void closeDropdown(Detail::WidgetState &widget) {
            if (!widget.dropdownOpen) {
                return;
            }

            widget.dropdownOpen = false;
            widget.dropdownClosed = true;
        }

        bool applySelection(Detail::WidgetState &widget, int *selectedIndex,
                            int nextSelection, size_t itemCount) {
            if (selectedIndex == nullptr || nextSelection < 0 ||
                itemCount == 0 ||
                nextSelection >= static_cast<int>(itemCount)) {
                return false;
            }

            widget.dropdownHighlightedIndex =
                static_cast<size_t>(nextSelection);
            closeDropdown(widget);

            if (*selectedIndex == nextSelection) {
                return false;
            }

            *selectedIndex = nextSelection;
            return true;
        }

        void processVisibleOptionClicks(SceneState *sceneState,
                                        const PickingId &id,
                                        Detail::WidgetState &widget,
                                        int *selectedIndex, size_t itemCount,
                                        bool &changed) {
            if (!widget.dropdownOpen || itemCount == 0) {
                return;
            }

            const size_t count =
                std::min(itemCount, widget.dropdownMaxVisibleOptions);
            const size_t start =
                std::min(widget.dropdownScrollOffset, itemCount - count);

            for (size_t row = 0; row < count; ++row) {
                const auto optionIndex = start + row;
                const auto optionId =
                    Detail::makeChildId(id, static_cast<uint32_t>(optionIndex));
                if (Detail::getWidgetState(sceneState, optionId) != nullptr &&
                    Detail::consumeClick(sceneState, optionId)) {
                    changed |= applySelection(widget, selectedIndex,
                                              static_cast<int>(optionIndex),
                                              itemCount);
                    return;
                }
            }
        }

        void drawBase(Detail::WidgetState &widget, const PickingId &id,
                      std::string_view label, bool hasSelection,
                      const glm::vec3 &boxPos, const glm::vec2 &boxSize,
                      SceneDrawContext &context,
                      const DropdownOptions &options) {
            const auto &palette = ViewportTheme::sceneWidgetsColors;
            const bool focused = widget.isFocused || widget.dropdownOpen;
            auto bgColor =
                widget.dropdownOpen
                    ? Detail::colorOr(options.expandedBackgroundColor,
                                      palette.surfaceActive)
                    : Detail::colorOr(options.backgroundColor, palette.surface);
            if (!widget.dropdownOpen && widget.isHovered) {
                bgColor = Detail::colorOr(options.hoverBackgroundColor,
                                          palette.surfaceHover);
            }

            const SceneDraw::QuadStyle style{
                .borderColor =
                    focused
                        ? Detail::colorOr(options.focusedBorderColor,
                                          palette.borderFocus)
                        : Detail::colorOr(options.borderColor, palette.border),
                .borderRadius = glm::vec4(2.f),
                .borderSize = glm::vec4(focused ? 0.8f : 0.5f),
            };
            SceneDraw::drawQuad(context, boxPos, boxSize, bgColor, id, style);

            const float left =
                boxPos.x - (boxSize.x * 0.5f) + options.padding.x;
            const float textOffY = context.renderer->textCenterOffsetY(
                label.empty() ? std::string_view("M") : label,
                {.fontSize = options.fontSize});
            SceneDraw::drawText(
                context, label, {left, boxPos.y + textOffY, boxPos.z + 0.001f},
                static_cast<size_t>(options.fontSize),
                hasSelection ? Detail::colorOr(options.textColor, palette.text)
                             : Detail::colorOr(options.mutedTextColor,
                                               palette.textMuted),
                id);

            const std::string_view caret = widget.dropdownOpen ? "^" : "v";
            const auto caretSize = context.renderer->measureText(
                caret, {.fontSize = options.fontSize});
            SceneDraw::drawText(
                context, caret,
                {boxPos.x + (boxSize.x * 0.5f) - options.padding.x -
                     caretSize.x,
                 boxPos.y + textOffY, boxPos.z + 0.001f},
                static_cast<size_t>(options.fontSize),
                Detail::colorOr(options.mutedTextColor, palette.textMuted), id);
        }

        void drawOptions(Detail::WidgetState &widget, const PickingId &id,
                         int selectedIndex,
                         const std::vector<std::string> &items,
                         const glm::vec3 &boxPos, const glm::vec2 &boxSize,
                         SceneDrawContext &context,
                         const DropdownOptions &options) {
            if (!widget.dropdownOpen || items.empty()) {
                return;
            }

            const auto &palette = ViewportTheme::sceneWidgetsColors;
            const size_t count = visibleOptionCount(items.size(), options);
            const size_t start =
                std::min(widget.dropdownScrollOffset, items.size() - count);

            for (size_t row = 0; row < count; ++row) {
                const size_t optionIndex = start + row;
                const auto optionId =
                    Detail::makeChildId(id, static_cast<uint32_t>(optionIndex));
                auto optionState = Detail::registerWidget(
                    context.sceneState, optionId,
                    Detail::WidgetState::Type::dropdownOption);
                if (optionState == nullptr) {
                    continue;
                }

                const auto rowCenter =
                    optionCenter(boxPos, boxSize, options.optionHeight, row);
                const glm::vec2 rowSize{boxSize.x, options.optionHeight};
                optionState->ownerWidgetId = id.toUint64();
                optionState->optionIndex = optionIndex;
                optionState->boundsPos = rowCenter;
                optionState->boundsSize = rowSize;

                auto rowColor = Detail::colorOr(options.expandedBackgroundColor,
                                                palette.popupSurface);
                if (static_cast<int>(optionIndex) == selectedIndex) {
                    rowColor = Detail::colorOr(options.optionSelectedColor,
                                               palette.accentStrong);
                } else if (optionState->isHovered ||
                           optionIndex == widget.dropdownHighlightedIndex) {
                    rowColor = Detail::colorOr(options.optionHoverColor,
                                               palette.surfaceHover);
                }

                const SceneDraw::QuadStyle rowStyle{
                    .borderColor =
                        Detail::colorOr(options.borderColor, palette.border),
                    .borderRadius = glm::vec4(0.f),
                    .borderSize = glm::vec4(0.f, 0.f, 0.5f, 0.f),
                };
                SceneDraw::drawQuad(context, rowCenter, rowSize, rowColor,
                                    optionId, rowStyle);

                const float left =
                    rowCenter.x - (rowSize.x * 0.5f) + options.padding.x;
                const float textOffY = context.renderer->textCenterOffsetY(
                    items[optionIndex], {.fontSize = options.fontSize});
                SceneDraw::drawText(
                    context, items[optionIndex],
                    {left, rowCenter.y + textOffY, rowCenter.z + 0.001f},
                    static_cast<size_t>(options.fontSize),
                    Detail::colorOr(options.textColor, palette.text), optionId);
            }
        }
    } // namespace

    DropdownResult dropdown(const PickingId &id, int *selectedIndex,
                            const std::vector<std::string> &items,
                            const glm::vec3 &boxPos, const glm::vec2 &boxSize,
                            SceneDrawContext &context,
                            const DropdownOptions &options) {
        DropdownResult result;
        result.selectedIndex = selectedIndex != nullptr ? *selectedIndex : -1;
        if (selectedIndex == nullptr || context.renderer == nullptr) {
            return result;
        }

        auto widgetsState = Detail::findSceneState(context.sceneState);
        auto widget = Detail::registerWidget(
            context.sceneState, id, Detail::WidgetState::Type::dropdown);
        if (widgetsState == nullptr || widget == nullptr) {
            return result;
        }

        const auto size =
            resolveDropdownSize(context.renderer, items, boxSize, options);
        const size_t itemCount = items.size();
        const int safeSelection = clampedSelection(*selectedIndex, itemCount);
        const size_t visibleCount = visibleOptionCount(itemCount, options);

        widget->boundsPos = boxPos;
        widget->boundsSize = size;
        widget->dropdownOptionCount = itemCount;
        widget->dropdownMaxVisibleOptions = visibleCount;

        if (itemCount == 0) {
            widget->dropdownHighlightedIndex = 0;
            widget->dropdownScrollOffset = 0;
        } else if (safeSelection >= 0 &&
                   widget->dropdownHighlightedIndex >= itemCount) {
            widget->dropdownHighlightedIndex =
                static_cast<size_t>(safeSelection);
        }
        Detail::ensureDropdownHighlightVisible(*widget);

        result.opened = widget->dropdownOpened;
        result.closed = widget->dropdownClosed;

        if (Detail::consumeClick(context.sceneState, id)) {
            if (widget->dropdownOpen) {
                closeDropdown(*widget);
                result.closed = true;
            } else {
                openDropdown(*widgetsState, *widget, id.toUint64(),
                             safeSelection);
                result.opened = true;
            }
        }

        if (widget->pendingDropdownSelection >= 0) {
            result.changed |=
                applySelection(*widget, selectedIndex,
                               widget->pendingDropdownSelection, itemCount);
            widget->pendingDropdownSelection = -1;
            result.closed = true;
        }

        processVisibleOptionClicks(context.sceneState, id, *widget,
                                   selectedIndex, itemCount, result.changed);

        const int selected = clampedSelection(*selectedIndex, itemCount);
        const bool hasSelection = selected >= 0;
        const std::string_view label =
            hasSelection ? items[static_cast<size_t>(selected)]
                         : options.placeholder;

        drawBase(*widget, id, label, hasSelection, boxPos, size, context,
                 options);
        drawOptions(*widget, id, selected, items, boxPos, size, context,
                    options);

        result.expanded = widget->dropdownOpen;
        result.opened |= widget->dropdownOpened;
        result.closed |= widget->dropdownClosed;
        result.selectedIndex = *selectedIndex;
        return result;
    }
} // namespace Bess::Canvas::SceneWidgets
