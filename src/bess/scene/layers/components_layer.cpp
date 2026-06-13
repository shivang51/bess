#include "components_layer.h"
#include "common/types.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "scene_event.h"
#include "scene_layer.h"

namespace Bess::Canvas {
    void ComponentsLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
        for (const auto &compId : ctx.sceneState->getRootComponents()) {
            const auto comp = ctx.sceneState->getComponentByUuid(compId);
            comp->update(ts, *ctx.sceneState);
        }
    }

    void ComponentsLayer::draw(SceneRenderContext &ctx) {
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
} // namespace Bess::Canvas
