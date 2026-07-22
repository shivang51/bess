#pragma once

#include "drag_drop.h"
#include "ui_painter.h"
#include "ui_style.h"
#include "widget.h"

#include <functional>
#include <optional>

namespace Bess::UI {

    struct DropZoneOptions {
        DropTargetCallbacks callbacks;
        // Optional adorner. Omitting it makes the zone behavior-only so a
        // composed child may provide richer feedback.
        std::optional<UIBoxStyle> dragOverStyle;
        bool clipChildren = false;
        bool showFeedback = true;
    };

    // Transparent single-child target wrapper. Target discovery happens via
    // DropTargetProvider while WidgetTree walks the hit widget's ancestry, so
    // interactive descendants remain fully interactive.
    class BESS_API DropZone : public Widget, public DropTargetProvider {
      public:
        DropZone(DragDropService &service, DropZoneOptions options);

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void arrange(WidgetArrangeContext &context) override;
        void paintOverlay(WidgetPaintContext &context) const override;

        [[nodiscard]] DropTargetId dropTargetId() const noexcept override;
        [[nodiscard]] bool isDragOver() const noexcept;
        [[nodiscard]] DragOperation proposedOperation() const noexcept;
        [[nodiscard]] glm::vec2 dragLocalPosition() const noexcept;

      private:
        void setDragOver(bool value,
                         DragOperation operation = DragOperation::none,
                         glm::vec2 localPosition = {0.f, 0.f});

        DragDropServiceHandle m_service;
        DropZoneOptions m_options;
        DropTargetRegistration m_registration;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        bool m_dragOver = false;
        DragOperation m_operation = DragOperation::none;
        glm::vec2 m_localPosition{0.f, 0.f};
    };

    struct DraggableOptions {
        DragPayload payload;
        std::function<DragPayload()> createPayload;
        DragOperation allowedOperations = DragOperation::move;
        DragOperation preferredOperation = DragOperation::move;
        std::optional<float> threshold;
        DragSourceCallbacks callbacks;
        CursorIcon idleCursor = CursorIcon::inherit;
        CursorIcon draggingCursor = CursorIcon::move;
        bool enabled = true;
        // Interactive descendants (buttons, sliders, text fields) keep their
        // own gesture by default. Enable this only for a dedicated drag handle
        // or a source whose entire subtree is intentionally draggable.
        bool allowFromInteractiveDescendants = false;
        // Usually false: selection/buttons inside a draggable row should still
        // receive their ordinary press. The global service owns the drag after
        // the threshold is crossed.
        bool consumePress = false;
        bool clipChildren = false;
    };

    // Transparent single-child source wrapper. It only arms a drag; target
    // resolution and release are centralized in WidgetTree/DragDropService so
    // pointer capture cannot hide the widgets beneath the drag preview.
    class BESS_API Draggable : public Widget, public DragSourceProvider {
      public:
        Draggable(DragDropService &service, DraggableOptions options);

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void arrange(WidgetArrangeContext &context) override;
        [[nodiscard]] CursorIcon
        cursor(const WidgetCursorContext &context) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] DragSourceId dragSourceId() const noexcept override;
        [[nodiscard]] bool canStartDrag() const noexcept override;
        [[nodiscard]] bool isDragging() const noexcept;
        [[nodiscard]] bool isArmed() const noexcept;
        void setEnabled(bool enabled) noexcept;

      private:
        DragDropServiceHandle m_service;
        DraggableOptions m_options;
        DragSourceId m_source = DragSourceId::generate();
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        bool m_dragging = false;
    };

} // namespace Bess::UI
