#include "scene_viewport_panel.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "common/types.h"
#include "pages/main_page/main_page.h"
#include "scene.h"
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
        if (!m_attachedScene || !m_camera) {
            return;
        }

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

        const bool shouldProcessPicking =
            m_isHovered || m_inputState.pickingReadbackRequest.active ||
            m_pendingSelectionReadback.active;

        if (m_pickingTexture && shouldProcessPicking &&
            !m_attachedScene->getIsFirstFrame() && !m_inputState.isDragging) {
            updatePickingIds(mouseMoved && m_isHovered);
        }

        if (m_isHovered) {
            applySceneCursor();
        } else if (!hasMouseCapture()) {
            m_pickingId = PickingId::invalid();
            m_inputState.cursor = Canvas::SceneCursor::inherit;
        }
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

    bool SceneViewportPanel::hasMouseCapture() const {
        return m_inputState.isLeftMousePressed ||
               m_inputState.isMiddleMousePressed || m_inputState.isDragging ||
               m_inputState.selectionBox.draw;
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

        const bool isLeftPressed = m_inputState.isLeftMousePressed;

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

            newPos.x = std::clamp(newPos.x,
                                  m_viewportPos.x + kViewportInsetPx,
                                  m_viewportPos.x + m_viewportSize.x -
                                      kViewportInsetPx);
            newPos.y = std::clamp(newPos.y,
                                  m_viewportPos.y + kViewportInsetPx,
                                  m_viewportPos.y + m_viewportSize.y -
                                      kViewportInsetPx);

            m_camera->incrementPos(vel);

            window->setEnableCursor(false);
            window->setMousePos(newPos);

            m_attachedScene->onMouseMove({newPos.x, newPos.y},
                                         m_camera,
                                         {
                                             m_viewportPos,
                                             m_viewportSize,
                                         },
                                         m_pickingId,
                                         m_inputState,
                                         m_viewportId,
                                         m_isSchematicView);
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
            m_inputState.isLeftMousePressed) {
            m_attachedScene->onLeftMouse(false,
                                         m_camera,
                                         {
                                             m_viewportPos,
                                             m_viewportSize,
                                         },
                                         m_pickingId,
                                         m_inputState,
                                         m_viewportId,
                                         m_isSchematicView);
        } else if (mouseBtnState.button == MouseButton::middle &&
                   m_inputState.isMiddleMousePressed) {
            m_attachedScene->onMiddleMouse(false,
                                           m_camera,
                                           {
                                               m_viewportPos,
                                               m_viewportSize,
                                           },
                                           m_pickingId,
                                           m_inputState,
                                           m_viewportId,
                                           m_isSchematicView);
        }
    }

    void SceneViewportPanel::applySceneCursor() {
        if (!m_attachedScene) {
            return;
        }

        const auto &mousePos = m_inputState.mousePos;
        if (mousePos.x < 0.f || mousePos.y < 0.f ||
            mousePos.x >= m_viewportSize.x || mousePos.y >= m_viewportSize.y) {
            return;
        }

        auto window = Pages::MainPage::getInstance()->getParentWindow();
        if (!window) {
            return;
        }

        switch (m_inputState.cursor) {
        case Canvas::SceneCursor::inherit:
            break;
        case Canvas::SceneCursor::pointer:
            window->getui().setCursorPointer();
            break;
        case Canvas::SceneCursor::move:
            window->getui().setCursorMove();
            break;
        case Canvas::SceneCursor::text:
            window->getui().setCursorText();
            break;
        case Canvas::SceneCursor::normal:
            window->getui().setCursorNormal();
            break;
        }

        m_inputState.cursor = Canvas::SceneCursor::inherit;
    }

    void SceneViewportPanel::renderAttachedScene() {
        if (!m_attachedScene) {
            setAttachedScene(GAppContext::getInstance()
                                 .getSubSystem<Bess::ProjectContext>()
                                 ->getSubSystem<SceneDriver>()
                                 ->getActiveScene());
        }

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

        Canvas::View2D view{
            .camera = m_camera,
            .renderer = renderer,
            .drawRenderTarget = sceneHandle,
            .pickingRenderTarget = pickingHandle,
            .viewportTransform =
                {
                    .pos = m_viewportPos,
                    .size = m_viewportSize,
                },
            .pickingId = m_pickingId,
            .inputState = m_inputState,
            .viewportId = m_viewportId,
            .isSchematicMode = m_isSchematicView,
        };

        m_attachedScene->draw(view);

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
            const bool isSelectionResult =
                m_pendingSelectionReadback.matches(pickingResult.x,
                                                   pickingResult.y,
                                                   pickingResult.width,
                                                   pickingResult.height);

            if (isSelectionResult) {
                auto &sceneState = m_attachedScene->getState();

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

                m_pendingSelectionReadback.clear();
                m_inputState.pickingReadbackRequest = {};
                return;
            }

            if (!m_pendingSelectionReadback.active) {
                m_pickingId = pickingResult.firstOrInvalid();
            }
        }

        if (m_pendingSelectionReadback.active) {
            return;
        }

        const auto pickingHandle = textureHandle(m_pickingTexture);
        if (pickingHandle == 0) {
            return;
        }

        const glm::vec2 mousePos = m_inputState.mousePos;
        const glm::vec2 textureSize = m_pickingTexture->getSize();
        const uint32_t width = positiveDimension(textureSize.x);
        const uint32_t height = positiveDimension(textureSize.y);

        const auto &selectionRequest = m_inputState.pickingReadbackRequest;
        if (selectionRequest.active) {
            if (selectionRequest.width == 0 || selectionRequest.height == 0) {
                m_inputState.pickingReadbackRequest = {};
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
            m_inputState.pickingReadbackRequest = {};
            return;
        }

        if (!mouseMoved) {
            return;
        }

        if (mousePos.x < 0.f || mousePos.y < 0.f ||
            mousePos.x >= static_cast<float>(width) ||
            mousePos.y >= static_cast<float>(height)) {
            m_pickingId = PickingId::invalid();
            return;
        }

        renderer->requestPickingId(pickingHandle,
                                   static_cast<uint32_t>(mousePos.x),
                                   static_cast<uint32_t>(mousePos.y));
    }
} // namespace Bess::UI
