#include "scene_viewport_panel.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "scene.h"
#include "scene_draw_context.h"
#include "scene_state/components/scene_component_types.h"
#include "settings/viewport_theme.h"
#include "vulkan_core.h"

namespace Bess::UI {
    uint64_t decodeGpuHoverValue(const glm::uvec2 &encodedId) {
        uint64_t id = static_cast<uint64_t>(encodedId.x);
        id |= (static_cast<uint64_t>(encodedId.y) << 32);
        return id;
    }

    void SceneViewportPanel::updateScene(TimeMs ts,
                                         const std::vector<ApplicationEvent> &events) {

        Canvas::ViewportTransform vpTrans{.pos = m_viewportPos, .size = m_viewportSize};
        m_attachedScene->updateViewportTransform(vpTrans);

        auto &sceneState = m_attachedScene->getState();
        auto &selCtx = m_attachedScene->getSelBoxContext();

        if (selCtx.queueSelInNextFrame) {
            selCtx.queueForSel = true;
            selCtx.queueSelInNextFrame = false;
        } else if (selCtx.queueForSel) {
            const auto &start = selCtx.start;
            const auto &end = selCtx.end;
            const glm::vec2 pos = {std::min(start.x, end.x), std::max(start.y, end.y)};
            const auto size = glm::abs(end - start);
            int w = (int)size.x;
            int h = (int)size.y;
            int x = (int)pos.x;
            int y = (int)(m_viewportSize.y - pos.y);
            m_viewport->setPickingCoord(x, y, w, h);
            m_viewport->tryUpdatePickingResults();

            selCtx.queueForSel = false;
            selCtx.readIds = true;
        } else if (!selCtx.draw && !selCtx.readIds &&
                   !m_viewport->isPickingPending()) {
            auto mousePos_ = m_attachedScene->getMousePos();
            mousePos_.y = m_viewportSize.y - mousePos_.y;
            int x = static_cast<int>(mousePos_.x);
            int y = static_cast<int>(mousePos_.y);
            m_viewport->setPickingCoord(x, y);
        }

        if (selCtx.readIds && !m_viewport->isPickingPending()) {

            const std::vector<glm::uvec2> rawIds = m_viewport->getPickingIdsResult();

            selCtx.readIds = false;

            if (rawIds.size() > 0) {
                std::set<Canvas::PickingId> ids;
                for (const auto &rawId : rawIds) {
                    auto id = Canvas::PickingId::fromUint64(decodeGpuHoverValue(rawId));
                    ids.insert(id);
                }

                sceneState.clearSelectedComponents();
                for (const auto &id : ids) {
                    auto comp = sceneState.getComponentByPickingId(id);
                    if (comp == nullptr)
                        continue;
                    sceneState.addSelectedComponent(id);
                }
            }

        } else {
            const auto &ids = m_viewport->getPickingIdsResult();
            const uint64_t hoverValue = (ids.empty())
                                            ? Canvas::PickingId::invalid()
                                            : decodeGpuHoverValue(ids[0]);

            m_attachedScene->setPickingId(Canvas::PickingId::fromUint64(hoverValue));
        }
    }

    void SceneViewportPanel::renderAttachedScene() {
        auto renderers = m_viewport->getRenderers();

        auto &sceneState = m_attachedScene->getState();

        SceneDrawContext context;
        context.materialRenderer = renderers.materialRenderer;
        context.pathRenderer = renderers.pathRenderer;
        context.camera = m_viewport->getCamera();
        context.sceneState = &m_attachedScene->getState();

        auto &inst = Bess::Vulkan::VulkanCore::instance();

        m_viewport->begin((int)inst.getCurrentFrameIdx(),
                          ViewportTheme::colors.background,
                          {0, Canvas::PickingId::invalid().runtimeId});

        drawGrid(context);

        if (sceneState.getConnectionStartSlot() != UUID::null) {
            const auto comp = sceneState.getComponentByUuid(sceneState.getConnectionStartSlot());
            if (!comp) {
                sceneState.setConnectionStartSlot(UUID::null);
                return;
            }

            glm::vec3 pos;
            if (comp->getType() == Canvas::SceneComponentType::slot) {
                pos = comp->cast<Canvas::SlotSceneComponent>()->getConnectionPos(sceneState);
            } else {
                pos = comp->getAbsolutePosition(sceneState);
            }

            const auto endPos = m_attachedScene->toScenePos(
                m_attachedScene->getMousePos());

            drawGhostConnection(renderers.pathRenderer, glm::vec2(pos), endPos);
        }

        drawComponents(context);

        const auto &selCtx = m_attachedScene->getSelBoxContext();
        if (selCtx.draw) {
            drawSelectionBox(context);
        }
        m_viewport->end();
        m_viewport->submit();
    }

    void SceneViewportPanel::drawGrid(SceneDrawContext &context) {
        context.materialRenderer->drawGrid(
            glm::vec3(0.f, 0.f, 0.1f),
            context.camera->getSpan(),
            Canvas::PickingId::invalid(),
            {
                .minorColor = ViewportTheme::colors.gridMinorColor,
                .majorColor = ViewportTheme::colors.gridMajorColor,
                .axisXColor = ViewportTheme::colors.gridAxisXColor,
                .axisYColor = ViewportTheme::colors.gridAxisYColor,
            },
            context.camera);
    }

    void SceneViewportPanel::drawGhostConnection(const std::shared_ptr<PathRenderer> &pathRenderer,
                                                 const glm::vec2 &startPos,
                                                 const glm::vec2 &endPos) {
        auto midX = (startPos.x + endPos.x) / 2.f;

        const auto &id = Canvas::PickingId::invalid();

        pathRenderer->beginPathMode(glm::vec3(startPos.x, startPos.y, 0.8f),
                                    2.f,
                                    ViewportTheme::colors.ghostWire,
                                    id);

        pathRenderer->pathLineTo(glm::vec3(midX, startPos.y, 0.8f),
                                 2.f,
                                 ViewportTheme::colors.ghostWire,
                                 id);

        pathRenderer->pathLineTo(glm::vec3(midX, endPos.y, 0.8f),
                                 2.f,
                                 ViewportTheme::colors.ghostWire,
                                 id);

        pathRenderer->pathLineTo(glm::vec3(endPos, 0.8f),
                                 2.f,
                                 ViewportTheme::colors.ghostWire,
                                 id);

        pathRenderer->endPathMode(false, false, glm::vec4(1.f), true, true);
    }

    void SceneViewportPanel::drawComponents(SceneDrawContext &context) {
        auto &sceneState = *context.sceneState;
        const auto &cam = context.camera;
        const auto &span = (cam->getSpan() / 2.f) + 200.f;
        const auto &camPos = cam->getPos();

        for (const auto &compId : sceneState.getRootComponents()) {
            const auto comp = sceneState.getComponentByUuid(compId);

            const auto &pos = comp->getAbsolutePosition(sceneState);
            const auto x = pos.x - camPos.x;
            const auto y = pos.y - camPos.y;

            // skipping if outside camera and not connection
            if (
                (x < -span.x || x > span.x || y < -span.y || y > span.y))
                continue;

            if (sceneState.getIsSchematicView()) {
                comp->drawSchematic(sceneState,
                                    context.materialRenderer,
                                    context.pathRenderer);
            } else {
                comp->draw(sceneState,
                           context.materialRenderer,
                           context.pathRenderer);
            }
        }
    }

    void SceneViewportPanel::drawSelectionBox(SceneDrawContext &context) {
        const auto &state = *context.sceneState;
        const auto &selCtx = m_attachedScene->getSelBoxContext();

        const auto start = m_attachedScene->toScenePos(selCtx.start);
        const auto end = m_attachedScene->toScenePos(
            m_attachedScene->getMousePos());

        auto size = end - start;
        const auto pos = start + (size / 2.f);
        size = glm::abs(size);

        Renderer::QuadRenderProperties props;
        props.borderColor = ViewportTheme::colors.selectionBoxBorder;
        props.borderSize = glm::vec4(1.f);

        context.materialRenderer->drawQuad(glm::vec3(pos, 7.f),
                                           size,
                                           ViewportTheme::colors.selectionBoxFill,
                                           -1, props);
    }

} // namespace Bess::UI
