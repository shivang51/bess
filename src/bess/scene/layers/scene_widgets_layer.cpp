#include "scene_widgets_layer.h"
#include "common/types.h"
#include "scene/scene_widgets.h"
#include "scene_event.h"
#include "scene_types.h"

namespace Bess::Canvas {
    EventResult SceneWidgetsLayer::handleEvent(SceneEvent &evt,
                                               SceneEventContext &ctx) {
        switch (evt.type) {
        case SceneEvent::Type::mouseMove:
            return handleMouseMove(evt, ctx);
        case SceneEvent::Type::mouseButton:
            return handleMouseButton(evt, ctx);
        case SceneEvent::Type::key:
            return handleKey(evt, ctx);
        case SceneEvent::Type::mouseWheel:
        case SceneEvent::Type::none:
            return EventResult::Ignored;
        }

        return EventResult::Ignored;
    }

    EventResult SceneWidgetsLayer::handleMouseMove(SceneEvent &evt,
                                                   SceneEventContext &ctx) {
        const bool isWidget =
            SceneWidgets::contains(ctx.sceneState, evt.pickingId);
        if (isWidget) {
            if (m_hoveredWidget != evt.pickingId) {
                m_hoveredWidget = evt.pickingId;
            }
            if (ctx.inputState) {
                ctx.inputState->cursor =
                    SceneWidgets::isTextInput(ctx.sceneState, evt.pickingId)
                        ? SceneCursor::text
                        : SceneCursor::pointer;
            }
            SceneWidgets::setHoverId(ctx.sceneState, evt.pickingId);
            return EventResult::Handled;
        }

        if (m_hoveredWidget.isValid()) {
            m_hoveredWidget = PickingId::invalid();
            if (ctx.inputState) {
                ctx.inputState->cursor = SceneCursor::normal;
            }
            SceneWidgets::setHoverId(ctx.sceneState, PickingId::invalid());
            return EventResult::Handled;
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
            SceneWidgets::contains(ctx.sceneState, evt.pickingId);

        if (data.action == MouseButtonAction::press ||
            data.action == MouseButtonAction::doubleClick) {
            if (isWidget) {
                SceneWidgets::queuePress(ctx.sceneState, evt.pickingId);
                return EventResult::Consumed;
            }

            SceneWidgets::clearFocus(ctx.sceneState);
            return EventResult::Ignored;
        }

        if (data.action == MouseButtonAction::release) {
            if (SceneWidgets::hasPointerCapture(ctx.sceneState)) {
                SceneWidgets::queueRelease(ctx.sceneState, evt.pickingId);
                return EventResult::Consumed;
            }
        }

        return EventResult::Ignored;
    }

    EventResult SceneWidgetsLayer::handleKey(SceneEvent &evt,
                                             SceneEventContext &ctx) {
        return SceneWidgets::queueKey(ctx.sceneState, evt)
                   ? EventResult::Consumed
                   : EventResult::Ignored;
    }

    void SceneWidgetsLayer::update(TimeMs ts, SceneUpdateContext &ctx) {}

    void SceneWidgetsLayer::draw(SceneRenderContext &ctx) {}

    void SceneWidgetsLayer::reset(SceneLifecycleContext &ctx) {
        m_hoveredWidget = PickingId::invalid();
        if (ctx.inputState) {
            ctx.inputState->cursor = SceneCursor::inherit;
        }
        SceneWidgets::clearFocus(ctx.sceneState);
    }

    void SceneWidgetsLayer::destroy(SceneLifecycleContext &ctx) { reset(ctx); }
} // namespace Bess::Canvas
