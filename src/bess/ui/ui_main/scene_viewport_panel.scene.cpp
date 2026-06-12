#include "scene_viewport_panel.h"
#include "bess_core/g_app_context.h"
#include "common/types.h"
#include "events/application_event.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "scene.h"
#include "scene/scene_draw_context.h"
#include "scene/scene_draw_helpers.h"
#include "settings/viewport_theme.h"
#include "sub_systems/input_sub_system.h"
#include "sub_systems/renderer_context.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace Bess::UI {
    uint64_t decodeGpuHoverValue(const glm::uvec2 &encodedId) {
        uint64_t id = static_cast<uint64_t>(encodedId.x);
        id |= (static_cast<uint64_t>(encodedId.y) << 32);
        return id;
    }

    void SceneViewportPanel::updateScene(TimeMs ts) {
        Canvas::ViewportTransform vpTrans{.pos = m_viewportPos,
                                          .size = m_viewportSize};
        m_attachedScene->updateViewportTransform(vpTrans);

        auto &appCtx = Bess::GAppContext::getInstance();
        auto inputSystem = appCtx.getSubSystem<InputSubSystem>();

        const auto &frameInputState = inputSystem->getFrameInpState();

        bool mouseMoved = false;
        if (frameInputState.hasMouseMoved) {
            mouseMoved = true;
            const auto &mouseMoveState = inputSystem->getMouseMoveState();
            ApplicationEvent::MouseMoveData data{
                mouseMoveState.pos.x,
                mouseMoveState.pos.y,
            };
            handleMouseMoveEvt(
                ApplicationEvent(ApplicationEventType::MouseMove, data));
        }

        auto mouseBtnState = frameInputState.mouseBtnState;
        const auto isInsideVp = isInsideViewport(mouseBtnState.pos);

        if (frameInputState.hasMouseBtnEvent && !isInsideVp &&
            mouseBtnState.action == MouseButtonAction::release) {
            // manually updating mouse state
            // because we don't update scene if its not active

            const auto isLeft = (mouseBtnState.button == MouseButton::left &&
                                 m_attachedScene->getIsLeftMousePressed());

            const auto isMiddle =
                (mouseBtnState.button == MouseButton::middle &&
                 m_attachedScene->getIsMiddleMousePressed());

            if (isLeft) {
                m_attachedScene->onLeftMouse(false);
            } else if (isMiddle) {
                m_attachedScene->onMiddleMouse(false);
            }
        }

        if (!m_attachedScene->getIsFirstFrame() &&
            !m_attachedScene->isDragging()) {
            updatePickingIds(mouseMoved);
        }
    }

    bool SceneViewportPanel::isInsideViewport(const glm::vec2 &pos) const {
        return pos.x >= m_viewportPos.x &&
               pos.x < m_viewportPos.x + m_viewportSize.x &&
               pos.y >= m_viewportPos.y &&
               pos.y < m_viewportPos.y + m_viewportSize.y;
    }

    void SceneViewportPanel::handleMouseMoveEvt(const ApplicationEvent &event) {
        const auto data = event.getData<ApplicationEvent::MouseMoveData>();

        const glm::vec2 mousePos(data.x, data.y);

        const bool isInsideVp = isInsideViewport(mousePos);

        auto window = Pages::MainPage::getInstance()->getParentWindow();

        if (isInsideVp) {
            window->setEnableCursor(true);
            return;
        }

        const bool isLeftPressed = m_attachedScene->getIsLeftMousePressed();

        if (isLeftPressed) {

            auto newPos = mousePos;

            auto vel = glm::vec2(0.f);
            if (mousePos.x < m_viewportPos.x) {
                vel.x = -10;
                newPos.x = m_viewportPos.x + 1.f;
            } else if (mousePos.x > m_viewportPos.x + m_viewportSize.x) {
                vel.x = 10;
                newPos.x = m_viewportPos.x + m_viewportSize.x - 1.f;
            } else if (mousePos.y < m_viewportPos.y) {
                vel.y = -10;
                newPos.y = m_viewportPos.y + 1.f;
            } else if (mousePos.y > m_viewportPos.y + m_viewportSize.y) {
                vel.y = 10;
                newPos.y = m_viewportPos.y + m_viewportSize.y - 1.f;
            }

            m_attachedScene->getCamera()->incrementPos(vel);

            window->setEnableCursor(false);
            window->setMousePos(newPos);

            m_attachedScene->onMouseMove({newPos.x, newPos.y});
        } else {
            window->setEnableCursor(true);
        }
    }

    void SceneViewportPanel::renderAttachedScene() {
        if (m_sceneTexture == nullptr || m_pickingTexture == nullptr) {
            return;
        }
        const auto &appCtx = GAppContext::getInstance();
        const auto &renderCtx = appCtx.getSubSystem<RendererContext>();
        auto &sceneState = m_attachedScene->getState();

        const auto &renderer = renderCtx->getRenderer();

        renderer->beginFrame({
            .extent = {(uint32_t)m_viewportSize.x, (uint32_t)m_viewportSize.y},
            .clearColor = ViewportTheme::colors.background,
            .shouldClear = true,
            .targetTexture = m_sceneTexture->getHandle(),
            .pickingTexture = m_pickingTexture->getHandle(),
            .cameraTransform =
                glm::value_ptr(m_attachedScene->getCamera()->getTransform()),
        });

        SceneDrawContext context;
        context.sceneState = &sceneState;
        context.renderer = renderer;
        context.camera = m_attachedScene->getCamera();

        m_attachedScene->draw(renderer);

        if (sceneState.getConnectionStartSlot() != UUID::null) {
            const auto comp = sceneState.getComponentByUuid(
                sceneState.getConnectionStartSlot());
            if (!comp) {
                sceneState.setConnectionStartSlot(UUID::null);
                return;
            }

            glm::vec3 pos;
            if (comp->getType() == Canvas::SceneComponentType::slot) {
                pos =
                    comp->cast<Canvas::SlotSceneComponent>()->getConnectionPos(
                        sceneState);
            } else {
                pos = comp->getAbsolutePosition(sceneState);
            }

            const auto endPos =
                m_attachedScene->toScenePos(m_attachedScene->getMousePos());

            drawGhostConnection(context, glm::vec2(pos), endPos);
        }

        if (m_sceneDrawFlags.drawSelectionBox &&
            m_attachedScene->getSelBoxContext().draw) {
            drawSelectionBox(context);
        }

        renderer->endFrame();

        if (m_attachedScene->getIsFirstFrame()) {
            m_attachedScene->setIsFirstFrame(false);
        }
    }

    void SceneViewportPanel::drawGhostConnection(SceneDrawContext &context,
                                                 const glm::vec2 &startPos,
                                                 const glm::vec2 &endPos) {
        auto midX = (startPos.x + endPos.x) / 2.f;

        const auto &id = PickingId::invalid();
        constexpr float z = 0.48f; // Behind the connections so i can do joints

        Canvas::SceneDraw::beginPath(
            context, glm::vec3(startPos.x, startPos.y, z), 2.f,
            ViewportTheme::colors.ghostWire, id, {.roundedJoints = true});
        Canvas::SceneDraw::pathLineTo(context, glm::vec3(midX, startPos.y, z),
                                      2.f);
        Canvas::SceneDraw::pathLineTo(context, glm::vec3(midX, endPos.y, z),
                                      2.f);
        Canvas::SceneDraw::pathLineTo(context, glm::vec3(endPos, z), 2.f);
        Canvas::SceneDraw::endPath(context);
    }

    void SceneViewportPanel::drawSelectionBox(SceneDrawContext &context) {
        const auto &state = *context.sceneState;
        const auto &selCtx = m_attachedScene->getSelBoxContext();

        const auto start = m_attachedScene->toScenePos(selCtx.start);
        const auto end =
            m_attachedScene->toScenePos(m_attachedScene->getMousePos());

        auto size = end - start;
        const auto pos = start + (size / 2.f);
        size = glm::abs(size);

        Canvas::SceneDraw::QuadStyle props;
        props.borderColor = ViewportTheme::colors.selectionBoxBorder;
        props.borderSize = glm::vec4(1.f);

        Canvas::SceneDraw::drawQuad(context, glm::vec3(pos, 7.f), size,
                                    ViewportTheme::colors.selectionBoxFill,
                                    PickingId::invalid(), props);
    }

    void SceneViewportPanel::updatePickingIds(bool mouseMoved) {
        const auto &renderer = GAppContext::getInstance()
                                   .getSubSystem<RendererContext>()
                                   ->getRenderer();

        Core::Renderer::PickingReadbackResult pickingResult;
        if (renderer->tryGetPickingIds(pickingResult) &&
            !pickingResult.empty()) {
            const bool isSelectionResult =
                m_waitingForSelReadback && pickingResult.x == m_selReadbackX &&
                pickingResult.y == m_selReadbackY &&
                pickingResult.width == m_selReadbackWidth &&
                pickingResult.height == m_selReadbackHeight;

            if (isSelectionResult) {
                auto &sceneState = m_attachedScene->getState();
                sceneState.clearSelectedComponents();

                std::unordered_set<uint32_t> selectedRuntimeIds;
                for (const auto &id : pickingResult.ids) {
                    if (!id.isValid() || !id.isSelectable()) {
                        continue;
                    }
                    if (!selectedRuntimeIds.insert(id.runtimeId).second) {
                        continue;
                    }

                    const auto comp = sceneState.getComponentByPickingId(id);
                    if (comp) {
                        sceneState.addSelectedComponent(comp->getUuid());
                    }
                }

                m_waitingForSelReadback = false;
                return;
            }

            if (!m_waitingForSelReadback) {
                m_attachedScene->setPickingId(pickingResult.firstOrInvalid());
            }
        }

        if (m_waitingForSelReadback) {
            return;
        }

        if (m_pickingTexture == nullptr || m_pickingTexture->getHandle() == 0) {
            return;
        }

        auto &selCtx = m_attachedScene->getSelBoxContext();
        if (selCtx.queueSelInNextFrame) {
            selCtx.queueSelInNextFrame = false;
            selCtx.queueForSel = true;
            return;
        }

        const glm::vec2 mousePos = m_attachedScene->getMousePos();
        const glm::vec2 textureSize = m_pickingTexture->getSize();
        const uint32_t width =
            textureSize.x > 1.f ? static_cast<uint32_t>(textureSize.x) : 1u;
        const uint32_t height =
            textureSize.y > 1.f ? static_cast<uint32_t>(textureSize.y) : 1u;

        if (selCtx.queueForSel) {
            selCtx.queueForSel = false;

            const glm::vec2 start = selCtx.start;
            const glm::vec2 end = selCtx.end;
            const float minX = std::min(start.x, end.x);
            const float minY = std::min(start.y, end.y);
            const float maxX = std::max(start.x, end.x);
            const float maxY = std::max(start.y, end.y);

            const auto x0 = static_cast<uint32_t>(std::clamp(
                std::floor(minX), 0.f, static_cast<float>(width - 1u)));
            const auto y0 = static_cast<uint32_t>(std::clamp(
                std::floor(minY), 0.f, static_cast<float>(height - 1u)));
            const auto x1 = static_cast<uint32_t>(
                std::clamp(std::ceil(maxX), static_cast<float>(x0 + 1u),
                           static_cast<float>(width)));
            const auto y1 = static_cast<uint32_t>(
                std::clamp(std::ceil(maxY), static_cast<float>(y0 + 1u),
                           static_cast<float>(height)));

            m_selReadbackX = x0;
            m_selReadbackY = y0;
            m_selReadbackWidth = x1 - x0;
            m_selReadbackHeight = y1 - y0;
            m_waitingForSelReadback = true;

            renderer->requestPickingIds(
                {.texture = m_pickingTexture->getHandle(),
                 .x = m_selReadbackX,
                 .y = m_selReadbackY,
                 .width = m_selReadbackWidth,
                 .height = m_selReadbackHeight});
            return;
        }

        if (!mouseMoved) {
            return;
        }

        if (mousePos.x < 0.f || mousePos.y < 0.f ||
            mousePos.x >= static_cast<float>(width) ||
            mousePos.y >= static_cast<float>(height)) {
            m_attachedScene->setPickingId(PickingId::invalid());
            return;
        }

        renderer->requestPickingId(m_pickingTexture->getHandle(),
                                   static_cast<uint32_t>(mousePos.x),
                                   static_cast<uint32_t>(mousePos.y));
    }
} // namespace Bess::UI
