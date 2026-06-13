#include "hover_layer.h"
#include "common/logger.h"
#include "scene/scene_state/components/scene_component.h"

namespace Bess::Canvas {
    EventResult HoverLayer::handleEvent(SceneEvent &evt,
                                        SceneEventContext &ctx) {
        if (evt.type != SceneEvent::Type::mouseMove) {
            return EventResult::Ignored;
        }

        return handleMouseMove(evt, ctx);
    }

    EventResult HoverLayer::handleMouseMove(SceneEvent &evt,
                                            SceneEventContext &ctx) {
        if (!ctx.sceneState) {
            return EventResult::Ignored;
        }

        const auto &data = evt.data.mouseMove;
        if (evt.pickingId == m_pickingId) {
            return EventResult::Ignored;
        }

        clearHover(*ctx.sceneState, data.pos);
        m_pickingId = evt.pickingId;

        if (m_pickingId.isValid()) {
            const auto &comp =
                ctx.sceneState->getComponentByPickingId(m_pickingId);
            if (comp) {
                comp->onMouseEnter({data.pos, m_pickingId.info});
            } else {
                BESS_WARN("[HoverLayer] PickingId is valid but no component "
                          "found for id {} in scene {}",
                          (uint64_t)m_pickingId,
                          (uint64_t)ctx.sceneState->getSceneId());
            }
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
        } else {
            BESS_WARN("[HoverLayer] PickingId is valid but no component found "
                      "for id {} in scene {}",
                      (uint64_t)m_pickingId, (uint64_t)state.getSceneId());
        }
    }

    void HoverLayer::update(TimeMs ts, SceneUpdateContext &ctx) {}

    void HoverLayer::draw(SceneRenderContext &ctx) {}

    void HoverLayer::reset(SceneLifecycleContext &ctx) {
        if (ctx.sceneState && ctx.inputState) {
            clearHover(*ctx.sceneState,
                       ctx.camera
                           ? ctx.camera->toWorldPos(ctx.inputState->mousePos)
                           : glm::vec2{0.f});
        }
        m_pickingId = PickingId::invalid();
    }

    void HoverLayer::destroy(SceneLifecycleContext &ctx) { reset(ctx); }
} // namespace Bess::Canvas
