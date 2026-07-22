#pragma once

#include "controls/drag_drop_widgets.h"
#include "layout.h"

#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace Bess::UI {

    struct ReorderListIdTag;
    struct ReorderItemIdTag;
    using ReorderListId = StableId<ReorderListIdTag>;
    using ReorderItemId = StableId<ReorderItemIdTag>;

    struct ReorderDragData {
        ReorderListId source;
        std::vector<ReorderItemId> items;
    };

    namespace DragFormats {
        inline const DragFormat<ReorderDragData> reorderItems{
            "application/x-bess-reorder-items"};
    } // namespace DragFormats

    enum class ReorderAxis : uint8_t { vertical, horizontal };

    struct ReorderRequest {
        ReorderListId source;
        ReorderListId target;
        // Valid for the duration of the callback. Copy it only when the model
        // deliberately queues the operation for another thread/frame.
        std::span<const ReorderItemId> items;
        // Invalid means append after the last remaining item.
        ReorderItemId before;
        DragOperation operation = DragOperation::move;
    };

    struct ReorderInsertion {
        ReorderItemId before;
        WidgetBounds indicatorBounds;
        bool valid = false;
    };

    struct ReorderableListOptions {
        ReorderAxis axis = ReorderAxis::vertical;
        Core::Style::Padding padding{};
        float gap = 0.f;
        bool stretchWidth = true;
        bool stretchHeight = false;
        bool clipChildren = false;
        bool allowCrossList = true;
        DragOperation acceptedOperations = DragOperation::move;
        std::function<bool(const ReorderRequest &)> onReorder;
        std::optional<UIBoxStyle> insertionStyle;
        float insertionThickness = 2.f;
        float insertionInset = 2.f;
    };

    // A model-driven list drop target. It computes one stable `before` item
    // and never mutates WidgetTree order itself; the callback updates the
    // application model, after which declarative composition reflects it.
    class BESS_API ReorderableList : public Widget, public DropTargetProvider {
      public:
        ReorderableList(DragDropService &service,
                        ReorderableListOptions options = {},
                        ReorderListId id = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void paintOverlay(WidgetPaintContext &context) const override;

        [[nodiscard]] DropTargetId dropTargetId() const noexcept override;
        [[nodiscard]] ReorderListId listId() const noexcept;
        [[nodiscard]] const ReorderInsertion &insertion() const noexcept;
        void setOnReorder(std::function<bool(const ReorderRequest &)> callback);

      private:
        struct ItemGeometry {
            ReorderItemId item;
            WidgetBounds bounds;
        };

        [[nodiscard]] DragProposal propose(const DragTargetEvent &event) const;
        void updateInsertion(const DragTargetEvent &event);
        void clearInsertion();
        [[nodiscard]] bool commit(const DropEvent &event);

        DragDropServiceHandle m_service;
        ReorderableListOptions m_options;
        ReorderListId m_listId;
        DropTargetRegistration m_registration;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        ReorderInsertion m_insertion;
        // Retain capacity across pointer updates. Stable lists therefore do
        // not allocate temporary geometry on every mouse movement.
        std::vector<ItemGeometry> m_itemGeometry;
    };

    struct DraggableListItemOptions {
        // Supplies the current selected item IDs at the moment the threshold
        // is crossed. An empty result falls back to the represented item.
        std::function<std::vector<ReorderItemId>()> draggedItems;
        DragOperation allowedOperations = DragOperation::move;
        DragOperation preferredOperation = DragOperation::move;
        std::optional<float> threshold;
        DragSourceCallbacks callbacks;
        CursorIcon idleCursor = CursorIcon::inherit;
        CursorIcon draggingCursor = CursorIcon::move;
        bool enabled = true;
        bool allowFromInteractiveDescendants = false;
        bool consumePress = false;
    };

    // Direct child used by ReorderableList for stable identity and geometry.
    // Content remains an ordinary single child composed beneath this wrapper.
    class BESS_API DraggableListItem : public Draggable {
      public:
        DraggableListItem(DragDropService &service,
                          ReorderListId list,
                          ReorderItemId item,
                          DraggableListItemOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] ReorderListId listId() const noexcept;
        [[nodiscard]] ReorderItemId itemId() const noexcept;

      private:
        [[nodiscard]] static DraggableOptions
        draggableOptions(ReorderListId list,
                         ReorderItemId item,
                         DraggableListItemOptions options);

        ReorderListId m_list;
        ReorderItemId m_item;
    };

} // namespace Bess::UI
