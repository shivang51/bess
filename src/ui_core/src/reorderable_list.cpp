#include "controls/reorderable_list.h"

#include "ui_painter.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace Bess::UI {
    namespace {
        constexpr float kInsertionZ = 0.09f;

        [[nodiscard]] float nonNegative(float value) noexcept {
            return std::isfinite(value) ? std::max(0.f, value) : 0.f;
        }

        [[nodiscard]] DragOperation
        selectAcceptedOperation(const DragTargetEvent &event,
                                DragOperation accepted) noexcept {
            accepted = accepted & event.allowedOperations;
            if (isSingleDragOperation(event.requestedOperation) &&
                hasDragOperation(accepted, event.requestedOperation)) {
                return event.requestedOperation;
            }
            for (const auto operation : {DragOperation::move,
                                         DragOperation::copy,
                                         DragOperation::link}) {
                if (hasDragOperation(accepted, operation)) {
                    return operation;
                }
            }
            return DragOperation::none;
        }

        [[nodiscard]] bool contains(std::span<const ReorderItemId> items,
                                    ReorderItemId item) {
            return std::find(items.begin(), items.end(), item) != items.end();
        }

        [[nodiscard]] bool validDragData(const ReorderDragData &data) {
            if (!data.source || data.items.empty()) {
                return false;
            }
            for (size_t index = 0; index < data.items.size(); ++index) {
                if (!data.items[index] ||
                    std::find(data.items.begin(),
                              data.items.begin() +
                                  static_cast<ptrdiff_t>(index),
                              data.items[index]) !=
                        data.items.begin() + static_cast<ptrdiff_t>(index)) {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    ReorderableList::ReorderableList(DragDropService &service,
                                     ReorderableListOptions options,
                                     ReorderListId id)
        : m_service(service.handle()),
          m_options(std::move(options)),
          m_listId(id ? id : ReorderListId::generate()) {
    }

    std::string_view ReorderableList::typeName() const noexcept {
        return "ReorderableList";
    }

    WidgetTraits ReorderableList::traits() const noexcept {
        return {.hitTestVisible = true, .clipChildren = m_options.clipChildren};
    }

    void ReorderableList::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        context.layout.setDirection(m_options.axis == ReorderAxis::vertical
                                        ? LayoutDirection::vertical
                                        : LayoutDirection::horizontal);
        context.layout.setPadding(m_options.padding);
        context.layout.setGap(nonNegative(m_options.gap));
        if (m_options.stretchWidth) {
            context.layout.setWidthPercent(1.f);
        }
        if (m_options.stretchHeight) {
            context.layout.setHeightPercent(1.f);
        }

        auto *service = m_service.get();
        if (service == nullptr) {
            return;
        }
        m_registration = service->registerTarget({
            .propose =
                [this](const DragTargetEvent &event) { return propose(event); },
            .onEnter =
                [this](const DragTargetEvent &event) {
                    updateInsertion(event);
                },
            .onOver =
                [this](const DragTargetEvent &event) {
                    updateInsertion(event);
                },
            .onLeave = [this](const DragLeaveEvent &) { clearInsertion(); },
            .onDrop = [this](const DropEvent &event) { return commit(event); },
        });
    }

    void ReorderableList::onUnmount(WidgetTree &, WidgetId) {
        static_cast<void>(m_registration.reset());
        m_state = nullptr;
        m_id = {};
        m_insertion = {};
    }

    void ReorderableList::paintOverlay(WidgetPaintContext &context) const {
        if (!m_insertion.valid || m_insertion.indicatorBounds.empty()) {
            return;
        }
        const auto &style = m_options.insertionStyle.value_or(
            context.state.theme().slider.fill);
        context.painter.drawBox({
            .bounds = m_insertion.indicatorBounds,
            .color = style.background,
            .borderColor = style.border,
            .cornerRadius = style.cornerRadius,
            .borderThickness = style.borderThickness,
            .shadow = style.shadow,
            .zIndex = kInsertionZ,
        });
    }

    DropTargetId ReorderableList::dropTargetId() const noexcept {
        return m_registration.isRegistered() ? m_registration.id()
                                             : DropTargetId{};
    }

    ReorderListId ReorderableList::listId() const noexcept {
        return m_listId;
    }

    const ReorderInsertion &ReorderableList::insertion() const noexcept {
        return m_insertion;
    }

    void ReorderableList::setOnReorder(
        std::function<bool(const ReorderRequest &)> callback) {
        m_options.onReorder = std::move(callback);
    }

    DragProposal ReorderableList::propose(const DragTargetEvent &event) const {
        const auto *data = event.payload.get(DragFormats::reorderItems);
        if (data == nullptr || !validDragData(*data) ||
            (!m_options.allowCrossList && data->source != m_listId) ||
            !m_options.onReorder) {
            return {};
        }
        return {selectAcceptedOperation(event, m_options.acceptedOperations)};
    }

    void ReorderableList::updateInsertion(const DragTargetEvent &event) {
        const auto *data = event.payload.get(DragFormats::reorderItems);
        if (m_state == nullptr || !m_id || data == nullptr ||
            !validDragData(*data)) {
            clearInsertion();
            return;
        }

        const auto children = m_state->getChildren(m_id);
        m_itemGeometry.clear();
        m_itemGeometry.reserve(children.size());
        for (const auto child : children) {
            const auto *item = m_state->getWidget<DraggableListItem>(child);
            if (item != nullptr && item->itemId()) {
                m_itemGeometry.push_back(
                    {item->itemId(), m_state->getBounds(child)});
            }
        }
        const bool vertical = m_options.axis == ReorderAxis::vertical;
        const auto coordinate = [vertical](const ItemGeometry &row) {
            return vertical ? row.bounds.center.y : row.bounds.center.x;
        };

        const float pointer = vertical ? event.position.y : event.position.x;
        size_t insertionIndex = 0;
        while (insertionIndex < m_itemGeometry.size() &&
               pointer >= coordinate(m_itemGeometry[insertionIndex])) {
            ++insertionIndex;
        }

        const bool sameList = data->source == m_listId;
        ReorderItemId before;
        for (size_t index = insertionIndex; index < m_itemGeometry.size();
             ++index) {
            if (!sameList ||
                !contains(data->items, m_itemGeometry[index].item)) {
                before = m_itemGeometry[index].item;
                break;
            }
        }

        const auto listBounds = m_state->getBounds(m_id);
        const float thickness =
            std::max(1.f, nonNegative(m_options.insertionThickness));
        const float inset = nonNegative(m_options.insertionInset);
        float lineCoordinate =
            vertical ? listBounds.topLeft().y : listBounds.topLeft().x;
        if (!m_itemGeometry.empty()) {
            if (insertionIndex < m_itemGeometry.size()) {
                lineCoordinate =
                    vertical
                        ? m_itemGeometry[insertionIndex].bounds.topLeft().y
                        : m_itemGeometry[insertionIndex].bounds.topLeft().x;
            } else {
                lineCoordinate =
                    vertical ? m_itemGeometry.back().bounds.bottomRight().y
                             : m_itemGeometry.back().bounds.bottomRight().x;
            }
        }

        WidgetBounds indicator;
        if (vertical) {
            indicator = {
                .center = {listBounds.center.x, lineCoordinate},
                .size = {std::max(0.f, listBounds.size.x - inset * 2.f),
                         thickness},
            };
        } else {
            indicator = {
                .center = {lineCoordinate, listBounds.center.y},
                .size = {thickness,
                         std::max(0.f, listBounds.size.y - inset * 2.f)},
            };
        }

        const bool changed =
            !m_insertion.valid || m_insertion.before != before ||
            m_insertion.indicatorBounds.center != indicator.center ||
            m_insertion.indicatorBounds.size != indicator.size;
        m_insertion = {
            .before = before,
            .indicatorBounds = indicator,
            .valid = true,
        };
        if (changed) {
            m_state->invalidate(m_id, WidgetInvalidation::paint);
        }
    }

    void ReorderableList::clearInsertion() {
        if (!m_insertion.valid) {
            return;
        }
        m_insertion = {};
        if (m_state != nullptr && m_id) {
            m_state->invalidate(m_id, WidgetInvalidation::paint);
        }
    }

    bool ReorderableList::commit(const DropEvent &event) {
        const auto *data = event.payload.get(DragFormats::reorderItems);
        if (data == nullptr || !validDragData(*data) || !m_options.onReorder) {
            clearInsertion();
            return false;
        }
        updateInsertion(event);
        if (!m_insertion.valid) {
            return false;
        }
        const auto callback = m_options.onReorder;
        const ReorderRequest request{
            .source = data->source,
            .target = m_listId,
            .items = data->items,
            .before = m_insertion.before,
            .operation = event.operation,
        };
        clearInsertion();
        return callback(request);
    }

    DraggableListItem::DraggableListItem(DragDropService &service,
                                         ReorderListId list,
                                         ReorderItemId item,
                                         DraggableListItemOptions options)
        : Draggable(service, draggableOptions(list, item, std::move(options))),
          m_list(list),
          m_item(item) {
        if (!m_list || !m_item) {
            throw std::invalid_argument(
                "DraggableListItem requires valid list and item IDs");
        }
    }

    std::string_view DraggableListItem::typeName() const noexcept {
        return "DraggableListItem";
    }

    ReorderListId DraggableListItem::listId() const noexcept {
        return m_list;
    }

    ReorderItemId DraggableListItem::itemId() const noexcept {
        return m_item;
    }

    DraggableOptions
    DraggableListItem::draggableOptions(ReorderListId list,
                                        ReorderItemId item,
                                        DraggableListItemOptions options) {
        auto itemsProvider = std::move(options.draggedItems);
        return {
            .payload = {},
            .createPayload =
                [list, item, itemsProvider = std::move(itemsProvider)]() {
                    std::vector<ReorderItemId> items =
                        itemsProvider ? itemsProvider()
                                      : std::vector<ReorderItemId>{item};
                    if (items.empty()) {
                        items.push_back(item);
                    }
                    DragPayloadBuilder payload;
                    if (!list || !item ||
                        !payload.set(DragFormats::reorderItems,
                                     ReorderDragData{list, std::move(items)})) {
                        return DragPayload{};
                    }
                    return std::move(payload).build();
                },
            .allowedOperations = options.allowedOperations,
            .preferredOperation = options.preferredOperation,
            .threshold = options.threshold,
            .callbacks = std::move(options.callbacks),
            .idleCursor = options.idleCursor,
            .draggingCursor = options.draggingCursor,
            .enabled = options.enabled,
            .allowFromInteractiveDescendants =
                options.allowFromInteractiveDescendants,
            .consumePress = options.consumePress,
        };
    }

} // namespace Bess::UI
