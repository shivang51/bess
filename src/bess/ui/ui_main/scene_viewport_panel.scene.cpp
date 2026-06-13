#include "scene_viewport_panel.h"
#include "bess_core/g_app_context.h"
#include "common/types.h"
#include "pages/main_page/main_page.h"
#include "scene.h"
#include "settings/viewport_theme.h"
#include "sub_systems/input_sub_system.h"
#include "sub_systems/renderer_context.h"
#include <algorithm>
#include <cstdint>

namespace Bess::UI {
    namespace {
        constexpr float kViewportInsetPx = 1.f;
        constexpr float kEdgePanStep = 10.f;

        bool isValidExtent(const glm::vec2 &size) {
            return size.x > kViewportInsetPx * 2.f &&
                   size.y > kViewportInsetPx * 2.f;
        }

        uint32_t positiveDimension(float value) {
            return value > 1.f ? static_cast<uint32_t>(value) : 1u;
        }

        Core::Renderer::TextureHandle textureHandle(
            const std::shared_ptr<Core::Renderer::ITexture> &texture) {
            return texture ? texture->getHandle() : 0;
        }
    } // namespace

    void SceneViewportPanel::updateScene(TimeMs ts) {
        (void)ts;
        if (!m_attachedScene) {
            return;
        }

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
            handleMouseMove(mouseMoveState.pos);
        }

        if (frameInputState.hasMouseBtnEvent) {
            releaseMouseButtonOutsideViewport(frameInputState.mouseBtnState);
        }

        if (m_pickingTexture && !m_attachedScene->getIsFirstFrame() &&
            !m_attachedScene->isDragging()) {
            updatePickingIds(mouseMoved);
        }

        applySceneCursor();
    }

    bool SceneViewportPanel::isInsideViewport(const glm::vec2 &pos) const {
        if (!hasRenderableViewport()) {
            return false;
        }

        return pos.x >= m_viewportPos.x &&
               pos.x < m_viewportPos.x + m_viewportSize.x &&
               pos.y >= m_viewportPos.y &&
               pos.y < m_viewportPos.y + m_viewportSize.y;
    }

    bool SceneViewportPanel::hasRenderableViewport() const {
        return isValidExtent(m_viewportSize);
    }

    void SceneViewportPanel::handleMouseMove(const glm::vec2 &mousePos) {
        auto window = Pages::MainPage::getInstance()->getParentWindow();
        if (!window) {
            return;
        }

        if (isInsideViewport(mousePos)) {
            window->setEnableCursor(true);
            return;
        }

        const bool isLeftPressed = m_attachedScene->isLeftMousePressed();

        if (isLeftPressed) {
            auto newPos = mousePos;
            auto vel = glm::vec2(0.f);

            if (mousePos.x < m_viewportPos.x) {
                vel.x = -kEdgePanStep;
            } else if (mousePos.x > m_viewportPos.x + m_viewportSize.x) {
                vel.x = kEdgePanStep;
            }

            if (mousePos.y < m_viewportPos.y) {
                vel.y = -kEdgePanStep;
            } else if (mousePos.y > m_viewportPos.y + m_viewportSize.y) {
                vel.y = kEdgePanStep;
            }

            newPos.x = std::clamp(newPos.x, m_viewportPos.x + kViewportInsetPx,
                                  m_viewportPos.x + m_viewportSize.x -
                                      kViewportInsetPx);
            newPos.y = std::clamp(newPos.y, m_viewportPos.y + kViewportInsetPx,
                                  m_viewportPos.y + m_viewportSize.y -
                                      kViewportInsetPx);

            m_attachedScene->panCamera(vel);

            window->setEnableCursor(false);
            window->setMousePos(newPos);

            m_attachedScene->onMouseMove({newPos.x, newPos.y});
        } else {
            window->setEnableCursor(true);
        }
    }

    void SceneViewportPanel::releaseMouseButtonOutsideViewport(
        const MouseButtonState &mouseBtnState) {
        if (mouseBtnState.action != MouseButtonAction::release ||
            isInsideViewport(mouseBtnState.pos)) {
            return;
        }

        // The scene is only updated while the viewport is active. Release
        // locally so drag/pan state cannot get stuck after leaving the panel.
        if (mouseBtnState.button == MouseButton::left &&
            m_attachedScene->isLeftMousePressed()) {
            m_attachedScene->onLeftMouse(false);
        } else if (mouseBtnState.button == MouseButton::middle &&
                   m_attachedScene->isMiddleMousePressed()) {
            m_attachedScene->onMiddleMouse(false);
        }
    }

    void SceneViewportPanel::applySceneCursor() const {
        if (!m_attachedScene) {
            return;
        }

        const auto &mousePos = m_attachedScene->getMousePos();
        if (mousePos.x < 0.f || mousePos.y < 0.f ||
            mousePos.x >= m_viewportSize.x || mousePos.y >= m_viewportSize.y) {
            return;
        }

        auto window = Pages::MainPage::getInstance()->getParentWindow();
        if (!window) {
            return;
        }

        switch (m_attachedScene->consumeCursorRequest()) {
        case Canvas::SceneCursor::inherit:
            break;
        case Canvas::SceneCursor::pointer:
            window->getui().setCursorPointer();
            break;
        case Canvas::SceneCursor::move:
            window->getui().setCursorMove();
            break;
        case Canvas::SceneCursor::normal:
            window->getui().setCursorNormal();
            break;
        }
    }

    void SceneViewportPanel::renderAttachedScene() {
        if (!m_attachedScene || !m_sceneTexture || !m_pickingTexture ||
            !hasRenderableViewport()) {
            return;
        }

        const auto sceneHandle = textureHandle(m_sceneTexture);
        const auto pickingHandle = textureHandle(m_pickingTexture);
        if (sceneHandle == 0 || pickingHandle == 0) {
            return;
        }

        const auto &appCtx = GAppContext::getInstance();
        const auto &renderCtx = appCtx.getSubSystem<RendererContext>();
        const auto &renderer = renderCtx->getRenderer();
        if (!renderer) {
            return;
        }

        renderer->beginFrame({
            .extent = {positiveDimension(m_viewportSize.x),
                       positiveDimension(m_viewportSize.y)},
            .clearColor = ViewportTheme::colors.background,
            .shouldClear = true,
            .targetTexture = sceneHandle,
            .pickingTexture = pickingHandle,
            .cameraTransform = m_attachedScene->getCameraTransformData(),
        });

        m_attachedScene->draw(renderer);

        renderer->endFrame();

        if (m_attachedScene->getIsFirstFrame()) {
            m_attachedScene->setIsFirstFrame(false);
        }
    }

    void SceneViewportPanel::updatePickingIds(bool mouseMoved) {
        if (!m_attachedScene || !m_pickingTexture) {
            m_pendingSelectionReadback.clear();
            return;
        }

        const auto &renderer = GAppContext::getInstance()
                                   .getSubSystem<RendererContext>()
                                   ->getRenderer();
        if (!renderer) {
            return;
        }

        Core::Renderer::PickingReadbackResult pickingResult;
        if (renderer->tryGetPickingIds(pickingResult) &&
            !pickingResult.empty()) {
            const bool isSelectionResult = m_pendingSelectionReadback.matches(
                pickingResult.x, pickingResult.y, pickingResult.width,
                pickingResult.height);

            if (isSelectionResult) {
                m_attachedScene->applySelectionReadback(pickingResult.ids);
                m_pendingSelectionReadback.clear();
                return;
            }

            if (!m_pendingSelectionReadback.active) {
                m_attachedScene->setPickingId(pickingResult.firstOrInvalid());
            }
        }

        if (m_pendingSelectionReadback.active) {
            return;
        }

        const auto pickingHandle = textureHandle(m_pickingTexture);
        if (pickingHandle == 0) {
            return;
        }

        const glm::vec2 mousePos = m_attachedScene->getMousePos();
        const glm::vec2 textureSize = m_pickingTexture->getSize();
        const uint32_t width = positiveDimension(textureSize.x);
        const uint32_t height = positiveDimension(textureSize.y);

        const auto &selectionRequest =
            m_attachedScene->getPickingReadbackRequest();
        if (selectionRequest.active) {
            if (selectionRequest.width == 0 || selectionRequest.height == 0) {
                m_attachedScene->clearPickingReadbackRequest();
                return;
            }

            m_pendingSelectionReadback = {
                .x = selectionRequest.x,
                .y = selectionRequest.y,
                .width = selectionRequest.width,
                .height = selectionRequest.height,
                .active = true,
            };

            renderer->requestPickingIds(
                {.texture = pickingHandle,
                 .x = m_pendingSelectionReadback.x,
                 .y = m_pendingSelectionReadback.y,
                 .width = m_pendingSelectionReadback.width,
                 .height = m_pendingSelectionReadback.height});
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

        renderer->requestPickingId(pickingHandle,
                                   static_cast<uint32_t>(mousePos.x),
                                   static_cast<uint32_t>(mousePos.y));
    }
} // namespace Bess::UI
