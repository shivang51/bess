#include "overlay_layer.h"
#include "common/bess_uuid.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "scene/scene_draw_helpers.h"
#include "settings/viewport_theme.h"

namespace Bess::Canvas {
    void OverlayLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void OverlayLayer::draw(SceneRenderContext &ctx) {
        if (!ctx.sceneState || !ctx.renderer || !ctx.camera) {
            return;
        }

        SceneDrawContext drawCtx{
            ctx.sceneState,
            ctx.renderer,
            ctx.camera,
        };

        drawCtx.isSchematicMode = *ctx.isSchematicMode;

        drawGhostConnection(drawCtx, ctx);
        drawSelectionBox(drawCtx, ctx);
    }

    void OverlayLayer::drawGhostConnection(SceneDrawContext &drawCtx,
                                           SceneRenderContext &ctx) const {

        if (ctx.sceneState->getConnectionStartSlot() == UUID::null ||
            !ctx.inputState || !ctx.isInFocusedViewport) {
            return;
        }

        const auto comp = ctx.sceneState->getComponentByUuid(
            ctx.sceneState->getConnectionStartSlot());
        if (!comp) {
            ctx.sceneState->setConnectionStartSlot(UUID::null);
            return;
        }

        glm::vec3 startPos;
        if (comp->getType() == Canvas::SceneComponentType::slot) {
            startPos =
                comp->cast<Canvas::SlotSceneComponent>()->getConnectionPos(
                    *ctx.sceneState, drawCtx.isSchematicMode);
        } else {
            startPos = comp->getAbsolutePosition(*drawCtx.sceneState,
                                                 drawCtx.isSchematicMode);
        }

        const auto endPos = ctx.camera->toWorldPos(ctx.inputState->mousePos);
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
        if (!ctx.inputState || !ctx.inputState->selectionBox.draw) {
            return;
        }

        const auto start =
            ctx.camera->toWorldPos(ctx.inputState->selectionBox.start);
        const auto end = ctx.camera->toWorldPos(ctx.inputState->mousePos);

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
