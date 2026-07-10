#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_event.h"
#include "common/types.h"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Bess::Core::Renderer {
    class IRenderer2D;
} // namespace Bess::Core::Renderer

namespace Bess::Canvas::UI {
    struct TextBoxContextResult {
        bool handled = false;
        bool changed = false;
        bool submitted = false;
        bool canceled = false;
    };

    class TextBoxContext {
      public:
        void syncExternalValue(std::string_view value, size_t maxLength);
        void focus(std::string_view value,
                   size_t maxLength,
                   bool selectAllOnFocus = false);
        void blur();

        TextBoxContextResult handleEvent(const SceneEvent &evt);

        void queuePointerPress(const glm::vec2 &pos,
                               bool extendSelection = false);
        void queuePointerMove(const glm::vec2 &pos);
        void queuePointerRelease(const glm::vec2 &pos);
        bool hasPointerCapture() const;

        void updatePointerSelection(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            float left,
            float contentWidth,
            const Core::Renderer::FontProps &fontProps);

        std::pair<size_t, size_t> visibleRangeForCursor(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            float maxWidth,
            const Core::Renderer::FontProps &fontProps) const;

        bool hasSelection() const;
        std::pair<size_t, size_t> selectionRange() const;

        const std::string &text() const {
            return m_text;
        }

        size_t cursorPos() const {
            return m_cursorPos;
        }

        bool isFocused() const {
            return m_focused;
        }

      private:
        void clampCursor();
        void clearSelection();
        void markChanged(TextBoxContextResult &result);
        bool deleteSelection(TextBoxContextResult &result);
        size_t textSizeAfterSelectionDelete() const;
        void moveCursor(size_t nextCursor,
                        bool selecting,
                        TextBoxContextResult &result);

      private:
        std::string m_text;
        std::string m_focusStartText;
        size_t m_cursorPos = 0;
        size_t m_selectionAnchorPos = 0;
        size_t m_maxLength = 256;
        bool m_focused = false;
        bool m_pointerSelecting = false;
        bool m_pointerInputQueued = false;
        bool m_pointerSelectionStarted = false;
        bool m_pointerExtendSelection = false;
        glm::vec2 m_pointerPos{0.f};
    };

    struct TextBoxContextDrawOptions {
        std::string_view placeholder;
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
        bool hovered = false;
    };

    void drawTextBoxContext(
        const PickingId &id,
        TextBoxContext &input,
        const glm::vec3 &boxPos,
        const glm::vec2 &boxSize,
        SceneDrawContext &context,
        const TextBoxContextDrawOptions &options = {});
} // namespace Bess::Canvas::UI
