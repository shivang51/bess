#include "scene_widgets_layer.h"
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
            return handleMouseButton(evt);
        case SceneEvent::Type::mouseWheel:
        case SceneEvent::Type::key:
        case SceneEvent::Type::none:
            return EventResult::Ignored;
        }

        return EventResult::Ignored;
    }

    EventResult SceneWidgetsLayer::handleMouseMove(SceneEvent &evt,
                                                   SceneEventContext &ctx) {
        const bool isWidget = SceneWidgets::contains(evt.pickingId);
        if (isWidget) {
            if (m_hoveredWidget != evt.pickingId) {
                m_hoveredWidget = evt.pickingId;
            }
            if (ctx.inputState) {
                ctx.inputState->cursor = SceneCursor::pointer;
            }
            return EventResult::Handled;
        }

        if (m_hoveredWidget.isValid()) {
            m_hoveredWidget = PickingId::invalid();
            if (ctx.inputState) {
                ctx.inputState->cursor = SceneCursor::normal;
            }
            return EventResult::Handled;
        }

        return EventResult::Ignored;
    }

    EventResult SceneWidgetsLayer::handleMouseButton(SceneEvent &evt) {
        const auto &data = evt.data.mouseButton;
        if (data.button != MouseButton::left ||
            !SceneWidgets::contains(evt.pickingId)) {
            return EventResult::Ignored;
        }

        if (data.action == MouseButtonAction::press) {
            SceneWidgets::queueClick(evt.pickingId);
        }

        return EventResult::Consumed;
    }

    void SceneWidgetsLayer::update(TimeMs ts, SceneUpdateContext &ctx) {}

    void SceneWidgetsLayer::draw(SceneRenderContext &ctx) {}

    void SceneWidgetsLayer::reset(SceneLifecycleContext &ctx) {
        m_hoveredWidget = PickingId::invalid();
        if (ctx.inputState) {
            ctx.inputState->cursor = SceneCursor::inherit;
        }
    }

    void SceneWidgetsLayer::destroy(SceneLifecycleContext &ctx) { reset(ctx); }
} // namespace Bess::Canvas
