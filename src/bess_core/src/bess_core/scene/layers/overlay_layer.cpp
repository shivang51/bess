#include "bess_core/scene/layers/overlay_layer.h"
#include "bess_core/scene/scene_component_types.h"
#include "bess_core/scene/scene_draw_helpers.h"
#include "bess_core/settings/viewport_theme.h"
#include "common/bess_uuid.h"

namespace Bess::Canvas {
    void OverlayLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void OverlayLayer::draw(SceneRenderContext &ctx) {
        if (!ctx.sceneState || !ctx.renderer || !ctx.camera) {
            return;
        }

        SceneDrawContext drawCtx{
            .sceneState = ctx.sceneState,
            .renderer = ctx.renderer,
            .camera = ctx.camera,
            .viewportId = ctx.viewportCtx->viewportId,
            .sceneWidgetsState = ctx.sceneWidgetsState,
        };

        drawCtx.isSchematicMode = ctx.viewportCtx->isSchematicMode();

        drawGhostConnection(drawCtx, ctx);
        drawSelectionBox(drawCtx, ctx);
    }

    void OverlayLayer::drawGhostConnection(SceneDrawContext &drawCtx,
                                           SceneRenderContext &ctx) const {

        if (ctx.sceneState->getConnectionStartSlot() == UUID::null ||
            !ctx.viewportCtx || !ctx.viewportCtx->isFocused) {
            return;
        }

        const auto comp = ctx.sceneState->getComponentByUuid(
            ctx.sceneState->getConnectionStartSlot());
        if (!comp) {
            ctx.sceneState->setConnectionStartSlot(UUID::null);
            return;
        }

        const auto startPos =
            comp->getConnectionPos(*ctx.sceneState, drawCtx.isSchematicMode);

        const auto endPos =
            ctx.camera->toWorldPos(ctx.viewportCtx->inputCtx.mousePos);
        const float midX = (startPos.x + endPos.x) / 2.f;

        const auto &id = PickingId::invalid();
        constexpr float z = 0.48f;

        SceneDraw::beginPath(drawCtx,
                             glm::vec3(startPos.x, startPos.y, z),
                             2.f,
                             ViewportTheme::colors.wire,
                             id,
                             {.roundedJoints = true});
        SceneDraw::pathLineTo(drawCtx, glm::vec3(midX, startPos.y, z), 2.f);
        SceneDraw::pathLineTo(drawCtx, glm::vec3(midX, endPos.y, z), 2.f);
        SceneDraw::pathLineTo(drawCtx, glm::vec3(endPos, z), 2.f);
        SceneDraw::endPath(drawCtx);
    }

    void OverlayLayer::drawSelectionBox(SceneDrawContext &drawCtx,
                                        SceneRenderContext &ctx) const {
        if (!ctx.viewportCtx || !ctx.viewportCtx->selBoxCtx.draw) {
            return;
        }

        const auto &selBox = ctx.viewportCtx->selBoxCtx;
        const auto start = ctx.camera->toWorldPos(selBox.start);
        const auto end =
            ctx.camera->toWorldPos(ctx.viewportCtx->inputCtx.mousePos);

        auto size = end - start;
        const auto pos = start + (size / 2.f);
        size = glm::abs(size);

        SceneDraw::QuadStyle props;
        props.borderColor = ViewportTheme::colors.selectionBoxBorder;
        props.borderSize = glm::vec4(1.f);

        SceneDraw::drawQuad(drawCtx,
                            glm::vec3(pos, 7.f),
                            size,
                            ViewportTheme::colors.selectionBoxFill,
                            PickingId::invalid(),
                            props);
    }
} // namespace Bess::Canvas
