#include "bess_core/scene/layers/scene_widgets_layer.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "common/types.h"

namespace Bess::Canvas {
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
        if (SceneWidgets::hasPointerCapture(ctx.sceneState,
                                            ctx.viewportCtx->viewportId)) {
            SceneWidgets::queuePointerMove(ctx.sceneState,
                                           evt.data.mouseMove.pos,
                                           ctx.viewportCtx->viewportId);
            if (ctx.viewportCtx) {
                ctx.viewportCtx->inputCtx.cursor =
                    Bess::Core::Viewport::SceneCursor::pointer;
            }
            return EventResult::Consumed;
        }

        const bool isWidget = SceneWidgets::contains(
            ctx.sceneState, evt.pickingId, ctx.viewportCtx->viewportId);
        if (isWidget) {
            if (m_hoveredWidget != evt.pickingId) {
                m_hoveredWidget = evt.pickingId;
            }
            if (ctx.viewportCtx) {
                ctx.viewportCtx->inputCtx.cursor =
                    SceneWidgets::isTextInput(ctx.sceneState,
                                              evt.pickingId,
                                              ctx.viewportCtx->viewportId)
                        ? Core::Viewport::SceneCursor::text
                        : Core::Viewport::SceneCursor::pointer;
            }
            SceneWidgets::setHoverId(
                ctx.sceneState, evt.pickingId, ctx.viewportCtx->viewportId);
            return EventResult::Consumed;
        }

        if (m_hoveredWidget.isValid()) {
            m_hoveredWidget = PickingId::invalid();
            if (ctx.viewportCtx) {
                ctx.viewportCtx->inputCtx.cursor =
                    Core::Viewport::SceneCursor::normal;
            }
            SceneWidgets::setHoverId(ctx.sceneState,
                                     PickingId::invalid(),
                                     ctx.viewportCtx->viewportId);
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

        const bool isWidget = SceneWidgets::contains(
            ctx.sceneState, evt.pickingId, ctx.viewportCtx->viewportId);

        if (data.action == MouseButtonAction::press ||
            data.action == MouseButtonAction::doubleClick) {
            if (isWidget) {
                SceneWidgets::queuePress(ctx.sceneState,
                                         evt.pickingId,
                                         data.pos,
                                         ctx.viewportCtx->viewportId);
                return EventResult::Consumed;
            }

            SceneWidgets::clearFocus(ctx.sceneState,
                                     ctx.viewportCtx->viewportId);
            return EventResult::Ignored;
        }

        if (data.action == MouseButtonAction::release) {
            if (SceneWidgets::hasPointerCapture(ctx.sceneState,
                                                ctx.viewportCtx->viewportId)) {
                SceneWidgets::queueRelease(ctx.sceneState,
                                           evt.pickingId,
                                           data.pos,
                                           ctx.viewportCtx->viewportId);
                return EventResult::Consumed;
            }
        }

        return EventResult::Ignored;
    }

    EventResult SceneWidgetsLayer::handleMouseWheel(SceneEvent &evt,
                                                    SceneEventContext &ctx) {
        return SceneWidgets::queueWheel(
                   ctx.sceneState, evt, ctx.viewportCtx->viewportId)
                   ? EventResult::Consumed
                   : EventResult::Ignored;
    }

    EventResult SceneWidgetsLayer::handleKey(SceneEvent &evt,
                                             SceneEventContext &ctx) {
        return SceneWidgets::queueKey(
                   ctx.sceneState, evt, ctx.viewportCtx->viewportId)
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
            ctx.viewportCtx->inputCtx.cursor =
                Bess::Core::Viewport::SceneCursor::inherit;
            SceneWidgets::clearFocus(ctx.sceneState,
                                     ctx.viewportCtx->viewportId);
        }
    }

    void SceneWidgetsLayer::destroy(SceneLifecycleContext &ctx) {
        reset(ctx);
    }
} // namespace Bess::Canvas
