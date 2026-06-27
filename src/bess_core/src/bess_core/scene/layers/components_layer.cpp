#include "bess_core/scene/layers/components_layer.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_layer.h"
#include "common/types.h"
#include "pages/main_page/scene_components/scene_comp_types.h"

namespace Bess::Canvas {
    void ComponentsLayer::viewportUpdate(TimeMs dt, SceneVpUpdateContext &ctx) {
        SceneUIPrepareCtx prepCtx{
            .sceneState = ctx.sceneState,
            .renderer = ctx.renderer,
        };

        for (const auto &compId : ctx.sceneState->getRootComponents()) {
            const auto comp = ctx.sceneState->getComponentByUuid(compId);
            if (comp->getUIDirty()) {
                comp->prepareUI(prepCtx);
            }
        }

        for (auto &[id, node] :
             ctx.sceneState->getUINodeRegistry()->getAllNodes()) {
            if (node.getParentId() != UUID::null)
                continue;
            node.measure(*ctx.sceneState->getUINodeRegistry(), UUID::null);
            node.layout(*ctx.sceneState->getUINodeRegistry(), UUID::null);
        }
    }

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

        SceneDrawContext drawCtx{
            .sceneState = ctx.sceneState,
            .renderer = ctx.renderer,
            .camera = ctx.camera,
            .viewportId = ctx.viewportCtx->viewportId,
            .isSchematicMode = ctx.viewportCtx->isSchematicMode(),
            .sceneWidgetsState = ctx.sceneWidgetsState,
        };

        for (const auto &compId : ctx.sceneState->getRootComponents()) {
            const auto comp = ctx.sceneState->getComponentByUuid(compId);

            const auto &pos = comp->getAbsolutePosition(
                *ctx.sceneState, ctx.viewportCtx->isSchematicMode());
            const auto x = pos.x - camPos.x;
            const auto y = pos.y - camPos.y;

            // skipping if outside camera and not connection
            // Connections are exempted
            if (comp->getType() != Canvas::SceneComponentType::connection &&
                (x < -span.x || x > span.x || y < -span.y || y > span.y))
                continue;

            if (ctx.viewportCtx->isSchematicMode()) {
                comp->drawSchematic(drawCtx);
            } else {
                comp->draw(drawCtx);
            }
        }
    }

} // namespace Bess::Canvas
