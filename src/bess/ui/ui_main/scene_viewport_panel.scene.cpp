#include "scene_viewport_panel.h"
#include "bess_core/g_app_context.h"
#include "common/types.h"
#include "events/application_event.h"
#include "pages/main_page/main_page.h"
#include "scene.h"
#include "settings/viewport_theme.h"
#include "sub_systems/input_sub_system.h"
#include "sub_systems/renderer_context.h"
#include <cstdint>

namespace Bess::UI {
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
                                 m_attachedScene->isLeftMousePressed());

            const auto isMiddle =
                (mouseBtnState.button == MouseButton::middle &&
                 m_attachedScene->isMiddleMousePressed());

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

        const bool isLeftPressed = m_attachedScene->isLeftMousePressed();

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

            m_attachedScene->panCamera(vel);

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
        const auto &renderer = renderCtx->getRenderer();

        renderer->beginFrame({
            .extent = {(uint32_t)m_viewportSize.x, (uint32_t)m_viewportSize.y},
            .clearColor = ViewportTheme::colors.background,
            .shouldClear = true,
            .targetTexture = m_sceneTexture->getHandle(),
            .pickingTexture = m_pickingTexture->getHandle(),
            .cameraTransform = m_attachedScene->getCameraTransformData(),
        });

        m_attachedScene->draw(renderer);

        renderer->endFrame();

        if (m_attachedScene->getIsFirstFrame()) {
            m_attachedScene->setIsFirstFrame(false);
        }
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
                m_attachedScene->applySelectionReadback(pickingResult.ids);
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

        const glm::vec2 mousePos = m_attachedScene->getMousePos();
        const glm::vec2 textureSize = m_pickingTexture->getSize();
        const uint32_t width =
            textureSize.x > 1.f ? static_cast<uint32_t>(textureSize.x) : 1u;
        const uint32_t height =
            textureSize.y > 1.f ? static_cast<uint32_t>(textureSize.y) : 1u;

        const auto &selectionRequest =
            m_attachedScene->getPickingReadbackRequest();
        if (selectionRequest.active) {
            m_selReadbackX = selectionRequest.x;
            m_selReadbackY = selectionRequest.y;
            m_selReadbackWidth = selectionRequest.width;
            m_selReadbackHeight = selectionRequest.height;
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
