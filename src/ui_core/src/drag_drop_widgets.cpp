#include "controls/drag_drop_widgets.h"

#include "widget_tree.h"

#include <utility>

namespace Bess::UI {
    namespace {
        constexpr float kDropFeedbackZ = 0.08f;

        BoxPaint feedbackPaint(WidgetBounds bounds, const UIBoxStyle &style) {
            return {
                .bounds = bounds,
                .color = style.background,
                .borderColor = style.border,
                .cornerRadius = style.cornerRadius,
                .borderThickness = style.borderThickness,
                .shadow = style.shadow,
                .zIndex = kDropFeedbackZ,
            };
        }
    } // namespace

    DropZone::DropZone(DragDropService &service, DropZoneOptions options)
        : m_service(service.handle()),
          m_options(std::move(options)) {
    }

    std::string_view DropZone::typeName() const noexcept {
        return "DropZone";
    }

    WidgetTraits DropZone::traits() const noexcept {
        return {.hitTestVisible = true, .clipChildren = m_options.clipChildren};
    }

    void DropZone::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        auto *service = m_service.get();
        if (service == nullptr) {
            return;
        }

        m_registration = service->registerTarget({
            .propose =
                [this](const DragTargetEvent &event) {
                    return m_options.callbacks.propose
                               ? m_options.callbacks.propose(event)
                               : DragProposal{};
                },
            .onEnter =
                [this](const DragTargetEvent &event) {
                    setDragOver(true, event.operation, event.localPosition);
                    if (m_options.callbacks.onEnter) {
                        m_options.callbacks.onEnter(event);
                    }
                },
            .onOver =
                [this](const DragTargetEvent &event) {
                    setDragOver(true, event.operation, event.localPosition);
                    if (m_options.callbacks.onOver) {
                        m_options.callbacks.onOver(event);
                    }
                },
            .onLeave =
                [this](const DragLeaveEvent &event) {
                    setDragOver(false);
                    if (m_options.callbacks.onLeave) {
                        m_options.callbacks.onLeave(event);
                    }
                },
            .onDrop =
                [this](const DropEvent &event) {
                    // Clear first so a model callback which removes this widget
                    // never leaves stale visual state behind.
                    setDragOver(false);
                    return m_options.callbacks.onDrop &&
                           m_options.callbacks.onDrop(event);
                },
        });
    }

    void DropZone::onUnmount(WidgetTree &, WidgetId) {
        static_cast<void>(m_registration.reset());
        m_state = nullptr;
        m_id = {};
        m_dragOver = false;
        m_operation = DragOperation::none;
    }

    void DropZone::arrange(WidgetArrangeContext &context) {
        const auto children = context.children();
        for (size_t index = 0; index < children.size(); ++index) {
            static_cast<void>(
                context.setChildVisible(children[index], index == 0U));
        }
    }

    void DropZone::paintOverlay(WidgetPaintContext &context) const {
        if (!m_dragOver || !m_options.showFeedback) {
            return;
        }
        const auto &style = m_options.dragOverStyle.value_or(
            context.state.theme().dock.dropPreview);
        context.painter.drawBox(feedbackPaint(context.bounds, style));
    }

    DropTargetId DropZone::dropTargetId() const noexcept {
        return m_registration.isRegistered() ? m_registration.id()
                                             : DropTargetId{};
    }

    bool DropZone::isDragOver() const noexcept {
        return m_dragOver;
    }

    DragOperation DropZone::proposedOperation() const noexcept {
        return m_operation;
    }

    glm::vec2 DropZone::dragLocalPosition() const noexcept {
        return m_localPosition;
    }

    void DropZone::setDragOver(bool value,
                               DragOperation operation,
                               glm::vec2 localPosition) {
        const bool changed = m_dragOver != value || m_operation != operation ||
                             m_localPosition != localPosition;
        m_dragOver = value;
        m_operation = value ? operation : DragOperation::none;
        m_localPosition = localPosition;
        if (changed && m_state != nullptr && m_id) {
            m_state->invalidate(m_id, WidgetInvalidation::paint);
        }
    }

    Draggable::Draggable(DragDropService &service, DraggableOptions options)
        : m_service(service.handle()),
          m_options(std::move(options)) {
    }

    std::string_view Draggable::typeName() const noexcept {
        return "Draggable";
    }

    WidgetTraits Draggable::traits() const noexcept {
        return {.hitTestVisible = true, .clipChildren = m_options.clipChildren};
    }

    void Draggable::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
    }

    void Draggable::onUnmount(WidgetTree &, WidgetId) {
        if (auto *service = m_service.get(); service != nullptr) {
            static_cast<void>(service->sourceUnavailable(m_source));
        }
        m_state = nullptr;
        m_id = {};
        m_dragging = false;
    }

    void Draggable::arrange(WidgetArrangeContext &context) {
        const auto children = context.children();
        for (size_t index = 0; index < children.size(); ++index) {
            static_cast<void>(
                context.setChildVisible(children[index], index == 0U));
        }
    }

    CursorIcon Draggable::cursor(const WidgetCursorContext &) const noexcept {
        return isDragging() ? m_options.draggingCursor : m_options.idleCursor;
    }

    UIEventReply Draggable::onEvent(WidgetEventContext &context,
                                    const UIEvent &event) {
        auto *service = m_service.get();
        if (service == nullptr || !m_options.enabled || !context.enabled ||
            (context.phase != UIEventPhase::capture &&
             context.phase != UIEventPhase::target)) {
            return {};
        }
        if (!m_options.allowFromInteractiveDescendants &&
            context.target != context.id) {
            return {};
        }

        // Capture routing visits outer ancestors first. Defer to the nearest
        // enabled source wrapper so nesting a draggable row inside a broader
        // draggable region remains deterministic.
        if (context.phase == UIEventPhase::capture) {
            WidgetId descendant = context.target;
            while (descendant && descendant != context.id) {
                const auto *provider = dynamic_cast<const DragSourceProvider *>(
                    context.state.getWidget(descendant));
                if (provider != nullptr && provider->canStartDrag()) {
                    return {};
                }
                descendant = context.state.getParent(descendant);
            }
        }
        const auto *button = event.getIf<Input::MouseButtonEvent>();
        if (button == nullptr || button->button != MouseButton::left ||
            button->action != MouseButtonAction::press ||
            !context.hasPointerPosition || !context.pointerInside() ||
            service->hasSession()) {
            return {};
        }

        auto callbacks = m_options.callbacks;
        const auto userStarted = std::move(callbacks.onStarted);
        const auto userCompleted = std::move(callbacks.onCompleted);
        callbacks.onStarted = [this,
                               userStarted](const DragStartedEvent &started) {
            m_dragging = true;
            if (m_state != nullptr && m_id) {
                m_state->invalidate(m_id, WidgetInvalidation::paint);
            }
            if (userStarted) {
                userStarted(started);
            }
        };
        callbacks.onCompleted =
            [this, userCompleted](const DragCompletedEvent &completed) {
                m_dragging = false;
                if (m_state != nullptr && m_id) {
                    m_state->invalidate(m_id, WidgetInvalidation::paint);
                }
                if (userCompleted) {
                    userCompleted(completed);
                }
            };

        const auto session = service->arm({
            .source = m_source,
            .pressPosition = context.pointerPosition,
            .payload = m_options.payload,
            .createPayload = m_options.createPayload,
            .allowedOperations = m_options.allowedOperations,
            .preferredOperation = m_options.preferredOperation,
            .threshold = m_options.threshold,
            .cursor = m_options.draggingCursor,
            .callbacks = std::move(callbacks),
        });
        return {
            .handled = m_options.consumePress && static_cast<bool>(session),
        };
    }

    DragSourceId Draggable::dragSourceId() const noexcept {
        return m_source;
    }

    bool Draggable::canStartDrag() const noexcept {
        const auto *service = m_service.get();
        return m_options.enabled && service != nullptr &&
               !service->hasSession();
    }

    bool Draggable::isDragging() const noexcept {
        const auto *service = m_service.get();
        return m_dragging && service != nullptr &&
               service->ownsSession(m_source) && service->isDragging();
    }

    bool Draggable::isArmed() const noexcept {
        const auto *service = m_service.get();
        return service != nullptr && service->ownsSession(m_source) &&
               !service->isDragging();
    }

    void Draggable::setEnabled(bool enabled) noexcept {
        m_options.enabled = enabled;
        if (!enabled) {
            if (auto *service = m_service.get(); service != nullptr) {
                static_cast<void>(service->sourceUnavailable(m_source));
            }
        }
    }

} // namespace Bess::UI
