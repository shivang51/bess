#include "bess_core/scene/layers/hover_layer.h"
#include "bess_core/scene/scene_component_types.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/widgets/scene_widgets.h"

namespace Bess::Canvas {
    namespace {
        constexpr uint8_t kCursorPriorityReset = 5;
        constexpr uint8_t kCursorPriorityHover = 10;
    } // namespace

    EventResult HoverLayer::handleEvent(SceneEvent &evt,
                                        SceneEventContext &ctx) {
        if (evt.type != SceneEvent::Type::mouseMove) {
            return EventResult::Ignored;
        }

        return handleMouseMove(evt, ctx);
    }

    bool HoverLayer::shouldReceiveConsumedEvent(const SceneEvent &evt) const {
        return evt.type == SceneEvent::Type::mouseMove;
    }

    EventResult HoverLayer::handleMouseMove(SceneEvent &evt,
                                            SceneEventContext &ctx) {
        if (!ctx.sceneState) {
            return EventResult::Ignored;
        }

        const auto &data = evt.data.mouseMove;
        if (SceneWidgets::contains(ctx.sceneWidgetsState, evt.pickingId)) {
            clearHover(*ctx.sceneState, data.pos);
            m_pickingId = PickingId::invalid();
            return EventResult::Handled;
        }

        const auto comp =
            evt.pickingId.isValid()
                ? ctx.sceneState->getComponentByPickingId(evt.pickingId)
                : nullptr;

        if (!comp) {
            const bool hadHoveredComponent = m_pickingId.isValid();
            clearHover(*ctx.sceneState, data.pos);
            m_pickingId = PickingId::invalid();
            if (ctx.viewportCtx && hadHoveredComponent) {
                ctx.viewportCtx->inputCtx.requestCursor(
                    Core::Viewport::SceneCursor::normal, kCursorPriorityReset);
            }
            return evt.pickingId.isValid() ? EventResult::Handled
                                           : EventResult::Ignored;
        }

        if (comp->getType() == SceneComponentType::ui) {
            clearHover(*ctx.sceneState, data.pos);
            m_pickingId = PickingId::invalid();
            return evt.pickingId.isValid() ? EventResult::Handled
                                           : EventResult::Ignored;
        }

        if (evt.pickingId == m_pickingId) {
            if (ctx.viewportCtx) {
                ctx.viewportCtx->inputCtx.requestCursor(comp->getCursor(),
                                                        kCursorPriorityHover);
            }
            return EventResult::Ignored;
        }

        clearHover(*ctx.sceneState, data.pos);
        m_pickingId = evt.pickingId;
        comp->onMouseEnter({data.pos, m_pickingId.info});
        if (ctx.viewportCtx) {
            ctx.viewportCtx->inputCtx.requestCursor(comp->getCursor(),
                                                    kCursorPriorityHover);
        }
        return EventResult::Handled;
    }

    void HoverLayer::clearHover(SceneState &state, const glm::vec2 &mousePos) {
        if (!m_pickingId.isValid()) {
            return;
        }

        const auto &comp = state.getComponentByPickingId(m_pickingId);
        if (comp) {
            comp->onMouseLeave({mousePos, m_pickingId.info});
        }
    }

    void HoverLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void HoverLayer::draw(SceneRenderContext &ctx) {
    }

    void HoverLayer::reset(SceneLifecycleContext &ctx) {
        if (ctx.sceneState && ctx.viewportCtx) {
            clearHover(*ctx.sceneState,
                       ctx.camera ? ctx.camera->toWorldPos(
                                        ctx.viewportCtx->inputCtx.mousePos)
                                  : glm::vec2{0.f});
        }
        m_pickingId = PickingId::invalid();
    }

    void HoverLayer::destroy(SceneLifecycleContext &ctx) {
        reset(ctx);
    }
} // namespace Bess::Canvas
