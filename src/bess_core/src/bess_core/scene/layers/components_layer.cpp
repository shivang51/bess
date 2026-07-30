#include "bess_core/scene/layers/components_layer.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_layer.h"
#include "bess_core/settings/themes.h"
#include "common/types.h"
#include "bess_core/scene/scene_component_types.h"

namespace Bess::Canvas {
    void ComponentsLayer::viewportUpdate(TimeMs dt, SceneVpUpdateContext &ctx) {
        SceneUIPrepareCtx prepCtx{
            .sceneState = ctx.sceneState,
            .renderer = ctx.renderer,
            .parentNode = nullptr,
            .theme = Config::Themes::getCurrentTheme(),
        };

        for (const auto compId : ctx.sceneState->getRootComponents()) {
            auto comp = ctx.sceneState->getComponentByUuid(compId);
            if (comp && comp->getUIDirty()) {
                comp->prepareUI(prepCtx);
            }
        }

        // Note: Can't think of a better way to do this for now.
        // Doing all components in seperate pass.
        // Because of reallocation of allCompMap flap map when doing all in
        // single pass.
        for (const auto &[compId, comp] : ctx.sceneState->getAllComponents()) {
            if (comp && comp->getUIDirty()) {
                comp->prepareUI(prepCtx);
            }
        }

        for (auto &[id, node] :
             ctx.sceneState->getUINodeRegistry()->getAllNodes()) {
            if (node.getParentId() != UUID::null)
                continue;
            node.measure(*ctx.sceneState->getUINodeRegistry(), UUID::null);
        }
    }

    void ComponentsLayer::update(TimeMs ts, SceneUpdateContext &ctx) {

        for (const auto &compId : ctx.sceneState->getRootComponents()) {
            const auto comp = ctx.sceneState->getComponentByUuid(compId);
            if (comp->getType() == Canvas::SceneComponentType::ui) {
                continue;
            }
            comp->update(ts, *ctx.sceneState);
        }
    }

    void ComponentsLayer::draw(SceneRenderContext &ctx) {
        const auto &cam = ctx.camera;
        const auto &span = (cam->getSpan() / 2.f) + 200.f;
        const auto &camPos = cam->getPos();

        SimDrawCache simDrawCache;
        simDrawCache.setSimEngine(ctx.simEngine);

        SceneDrawContext drawCtx{
            .sceneState = ctx.sceneState,
            .renderer = ctx.renderer,
            .camera = ctx.camera,
            .viewportId = ctx.viewportCtx->viewportId,
            .isSchematicMode = ctx.viewportCtx->isSchematicMode(),
            .sceneWidgetsState = ctx.sceneWidgetsState,
            .simDrawCache = &simDrawCache,
        };

        for (const auto &compId : ctx.sceneState->getRootComponents()) {
            const auto comp = ctx.sceneState->getComponentByUuid(compId);
            if (comp->getType() == Canvas::SceneComponentType::ui) {
                continue;
            }

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
