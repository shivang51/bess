#include "bess_core/scene/layers/scene_widgets_layer.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "common/types.h"

namespace Bess::Canvas {
    namespace {
        constexpr uint8_t kCursorPriorityReset = 5;
        constexpr uint8_t kCursorPriorityHover = 20;
        constexpr uint8_t kCursorPriorityCapture = 30;
    } // namespace

    EventResult SceneWidgetsLayer::handleEvent(SceneEvent &evt,
                                               SceneEventContext &ctx) {
        switch (evt.type) {
        case SceneEvent::Type::mouseMove:
            return handleMouseMove(evt, ctx);
        case SceneEvent::Type::mouseButton:
            return handleMouseButton(evt, ctx);
        case SceneEvent::Type::key:
        case SceneEvent::Type::textInput:
            return handleKey(evt, ctx);
        case SceneEvent::Type::mouseWheel:
            return handleMouseWheel(evt, ctx);
        case SceneEvent::Type::none:
            return EventResult::Ignored;
        }

        return EventResult::Ignored;
    }

    EventResult SceneWidgetsLayer::handleMouseMove(SceneEvent &evt,
                                                   SceneEventContext &ctx) {

        BESS_ASSERT(ctx.sceneState,
                    "SceneWidgetsLayer.handleMouseMove missing scene state");
        BESS_ASSERT(
            ctx.viewportCtx,
            "SceneWidgetsLayer.handleMouseMove missing viewport context");
        if (SceneWidgets::hasPointerCapture(ctx.sceneWidgetsState)) {
            SceneWidgets::queuePointerMove(ctx.sceneWidgetsState,
                                           evt.data.mouseMove.pos);
            if (ctx.viewportCtx) {
                ctx.viewportCtx->inputCtx.requestCursor(
                    Bess::Core::Viewport::SceneCursor::pointer,
                    kCursorPriorityCapture);
            }
            return EventResult::Consumed;
        }

        const bool isWidget =
            SceneWidgets::contains(ctx.sceneWidgetsState, evt.pickingId);
        if (isWidget) {
            if (m_hoveredWidget != evt.pickingId) {
                m_hoveredWidget = evt.pickingId;
            }
            if (ctx.viewportCtx) {
                const auto cursor =
                    SceneWidgets::isTextInput(ctx.sceneWidgetsState,
                                              evt.pickingId)
                        ? Core::Viewport::SceneCursor::text
                        : Core::Viewport::SceneCursor::pointer;
                ctx.viewportCtx->inputCtx.requestCursor(
                    cursor, kCursorPriorityHover);
            }
            SceneWidgets::setHoverId(ctx.sceneWidgetsState, evt.pickingId);
            return EventResult::Consumed;
        }

        if (m_hoveredWidget.isValid()) {
            m_hoveredWidget = PickingId::invalid();
            if (ctx.viewportCtx) {
                ctx.viewportCtx->inputCtx.requestCursor(
                    Core::Viewport::SceneCursor::normal,
                    kCursorPriorityReset);
            }
            SceneWidgets::setHoverId(ctx.sceneWidgetsState,
                                     PickingId::invalid());
            return EventResult::Consumed;
        }

        return EventResult::Ignored;
    }

    EventResult SceneWidgetsLayer::handleMouseButton(SceneEvent &evt,
                                                     SceneEventContext &ctx) {
        const auto &data = evt.data.mouseButton;
        if (data.button != MouseButton::left) {
            return EventResult::Ignored;
        }

        const bool isWidget =
            SceneWidgets::contains(ctx.sceneWidgetsState, evt.pickingId);

        if (data.action == MouseButtonAction::press ||
            data.action == MouseButtonAction::doubleClick) {
            if (isWidget) {
                if (ctx.sceneState) {
                    ctx.sceneState->clearUIFocus({
                        .mousePos = data.pos,
                        .details = evt.pickingId.info,
                        .sceneState = ctx.sceneState,
                    });
                }
                SceneWidgets::queuePress(ctx.sceneWidgetsState,
                                         evt.pickingId,
                                         data.pos,
                                         evt.isShiftPressed);
                return EventResult::Consumed;
            }

            SceneWidgets::clearFocus(ctx.sceneWidgetsState);
            return EventResult::Ignored;
        }

        if (data.action == MouseButtonAction::release) {
            if (SceneWidgets::hasPointerCapture(ctx.sceneWidgetsState)) {
                SceneWidgets::queueRelease(
                    ctx.sceneWidgetsState, evt.pickingId, data.pos);
                return EventResult::Consumed;
            }
        }

        return EventResult::Ignored;
    }

    EventResult SceneWidgetsLayer::handleMouseWheel(SceneEvent &evt,
                                                    SceneEventContext &ctx) {
        return SceneWidgets::queueWheel(ctx.sceneWidgetsState, evt)
                   ? EventResult::Consumed
                   : EventResult::Ignored;
    }

    EventResult SceneWidgetsLayer::handleKey(SceneEvent &evt,
                                             SceneEventContext &ctx) {
        return SceneWidgets::queueKey(ctx.sceneWidgetsState, evt)
                   ? EventResult::Consumed
                   : EventResult::Ignored;
    }

    void SceneWidgetsLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void SceneWidgetsLayer::draw(SceneRenderContext &ctx) {
    }

    void SceneWidgetsLayer::reset(SceneLifecycleContext &ctx) {
        m_hoveredWidget = PickingId::invalid();
        if (ctx.viewportCtx) {
            ctx.viewportCtx->inputCtx.resetCursorRequest();
            SceneWidgets::clearFocus(ctx.sceneWidgetsState);
        }
    }

    void SceneWidgetsLayer::destroy(SceneLifecycleContext &ctx) {
        reset(ctx);
    }
} // namespace Bess::Canvas
