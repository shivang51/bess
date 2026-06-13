#include "components_layer.h"
#include "common/types.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "scene_event.h"
#include "scene_layer.h"
#include <cstdint>

namespace Bess::Canvas {
    EventResult ComponentsLayer::handleEvent(SceneEvent &evt,
                                             SceneContext &ctx) {

        if (evt.type == SceneEvent::Type::mouseMove) {
            handleMouseMove(evt, ctx);
            return EventResult::Handled;
        }

        return EventResult::Ignored;
    }

    void ComponentsLayer::handleMouseMove(SceneEvent &evt, SceneContext &ctx) {
        const auto &data = evt.data.mouseMove;

        if (evt.pickingId != m_pickingId) {
            if (m_pickingId.isValid()) {
                const auto &comp =
                    ctx.sceneState->getComponentByPickingId(m_pickingId);
                if (comp) {
                    comp->onMouseLeave({data.pos, m_pickingId.info});
                } else {
                    BESS_WARN("[ComponentsLayer] PickingId is valid but no "
                              "component found for id {} in scene {}",
                              (uint64_t)m_pickingId,
                              (uint64_t)ctx.sceneState->getSceneId());
                }
            }

            m_pickingId = evt.pickingId;

            if (m_pickingId.isValid()) {
                const auto &comp =
                    ctx.sceneState->getComponentByPickingId(m_pickingId);
                if (comp) {
                    comp->onMouseEnter({data.pos, m_pickingId.info});
                } else {
                    BESS_WARN("[ComponentsLayer] PickingId is valid but no "
                              "component found for id {} in scene {}",
                              (uint64_t)m_pickingId,
                              (uint64_t)ctx.sceneState->getSceneId());
                }
            }
        }
    }

    void ComponentsLayer::destroy(SceneContext &ctx) {}

    void ComponentsLayer::update(TimeMs ts, SceneContext &ctx) {}

    void ComponentsLayer::draw(SceneContext &ctx) {
        const auto &cam = ctx.camera;
        const auto &span = (cam->getSpan() / 2.f) + 200.f;
        const auto &camPos = cam->getPos();

        // FIXME: Temp
        SceneDrawContext drawCtx{
            ctx.sceneState,
            ctx.renderer,
            ctx.camera,
        };

        for (const auto &compId : ctx.sceneState->getRootComponents()) {
            const auto comp = ctx.sceneState->getComponentByUuid(compId);

            const auto &pos = comp->getAbsolutePosition(*ctx.sceneState);
            const auto x = pos.x - camPos.x;
            const auto y = pos.y - camPos.y;

            // skipping if outside camera and not connection
            // Connections are exempted
            if (comp->getType() != Canvas::SceneComponentType::connection &&
                (x < -span.x || x > span.x || y < -span.y || y > span.y))
                continue;

            if (ctx.sceneState->getIsSchematicView()) {
                comp->drawSchematic(drawCtx);
            } else {
                comp->draw(drawCtx);
            }
        }
    }

    void ComponentsLayer::init(SceneContext &ctx) {}
} // namespace Bess::Canvas
