#include "scene_viewport_panel.h"
#include "bess_core/g_app_context.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene_driver.h"
#include "bess_core/sub_systems/input_sub_system.h"
#include "common/helpers.h"
#include "common/types.h"
#include "imgui_internal.h"
#include "pages/main_page/main_page.h"
#include "project_session/project_session.h"
#include "sub_systems/renderer_context.h"
#include "ui/icons/CodIcons_Remapped.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"
#include "ui/project_api.h"
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

    bool SceneViewportPanel::isSchematicMode() const {
        return m_viewportCtx->mode == Core::Viewport::ViewportMode::schematic;
    }

    bool SceneViewportPanel::toggleSchematicMode() {
        m_viewportCtx->mode = isSchematicMode()
                                  ? Core::Viewport::ViewportMode::normal
                                  : Core::Viewport::ViewportMode::schematic;
        return m_viewportCtx->mode == Core::Viewport::ViewportMode::schematic;
    }

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

        auto &inpCtx = m_viewportCtx->inputCtx;
        const bool shouldProcessPicking =
            m_isHovered || m_viewportCtx->pickingReadbackRequest.active ||
            m_pendingSelectionReadback.active;

        if (m_pickingTexture && shouldProcessPicking &&
            !m_attachedScene->getIsFirstFrame() && !inpCtx.isDragging) {
            updatePickingIds(mouseMoved && m_isHovered);
        }

        if (m_isHovered) {
            applySceneCursor();
        } else if (!hasMouseCapture()) {
            inpCtx.pickingId = PickingId::invalid();
            inpCtx.resetCursorRequest();
        }
    }

    bool SceneViewportPanel::isInsideViewport(const glm::vec2 &pos) const {
        if (!hasRenderableViewport()) {
            return false;
        }

        const auto &vpSize = m_viewportCtx->transform.size;
        const auto &vpPos = m_viewportCtx->transform.pos;

        return pos.x >= vpPos.x && pos.x < vpPos.x + vpSize.x &&
               pos.y >= vpPos.y && pos.y < vpPos.y + vpSize.y;
    }

    bool SceneViewportPanel::hasRenderableViewport() const {
        return isValidExtent(m_viewportCtx->transform.size) && m_sceneTexture &&
               m_pickingTexture;
    }

    bool SceneViewportPanel::hasMouseCapture() const {
        const auto &inputCtx = m_viewportCtx->inputCtx;
        return inputCtx.isLeftMousePressed || inputCtx.isMiddleMousePressed ||
               inputCtx.isDragging || m_viewportCtx->selBoxCtx.draw;
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

        const auto &inpCtx = m_viewportCtx->inputCtx;
        const bool isLeftPressed = inpCtx.isLeftMousePressed;

        if (isLeftPressed) {
            auto newPos = mousePos;
            auto vel = glm::vec2(0.f);

            if (mousePos.x < m_viewportCtx->transform.pos.x) {
                vel.x = -kEdgePanStep;
            } else if (mousePos.x > m_viewportCtx->transform.pos.x +
                                        m_viewportCtx->transform.size.x) {
                vel.x = kEdgePanStep;
            }

            if (mousePos.y < m_viewportCtx->transform.pos.y) {
                vel.y = -kEdgePanStep;
            } else if (mousePos.y > m_viewportCtx->transform.pos.y +
                                        m_viewportCtx->transform.size.y) {
                vel.y = kEdgePanStep;
            }

            newPos.x = std::clamp(
                newPos.x,
                m_viewportCtx->transform.pos.x + kViewportInsetPx,
                m_viewportCtx->transform.pos.x +
                    m_viewportCtx->transform.size.x - kViewportInsetPx);
            newPos.y = std::clamp(
                newPos.y,
                m_viewportCtx->transform.pos.y + kViewportInsetPx,
                m_viewportCtx->transform.pos.y +
                    m_viewportCtx->transform.size.y - kViewportInsetPx);

            m_camera->incrementPos(vel);

            window->setEnableCursor(false);
            window->setMousePos(newPos);

            m_attachedScene->onMouseMove(
                {newPos.x, newPos.y}, m_camera, m_viewportCtx);
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
            m_viewportCtx->inputCtx.isLeftMousePressed) {
            m_attachedScene->onLeftMouse(false, m_camera, m_viewportCtx);
        } else if (mouseBtnState.button == MouseButton::middle &&
                   m_viewportCtx->inputCtx.isMiddleMousePressed) {
            m_attachedScene->onMiddleMouse(false, m_camera, m_viewportCtx);
        }
    }

    void SceneViewportPanel::applySceneCursor() {
        if (!m_attachedScene) {
            return;
        }

        const auto &mousePos = m_viewportCtx->inputCtx.mousePos;
        if (mousePos.x < 0.f || mousePos.y < 0.f ||
            mousePos.x >= m_viewportCtx->transform.size.x ||
            mousePos.y >= m_viewportCtx->transform.size.y) {
            return;
        }

        auto window = Pages::MainPage::getInstance()->getParentWindow();
        if (!window) {
            return;
        }

        auto &inputCtx = m_viewportCtx->inputCtx;
        const auto requestedCursor = inputCtx.cursorRequest.cursor;
        if (requestedCursor == Core::Viewport::SceneCursor::inherit) {
            return;
        }

        if (requestedCursor == inputCtx.lastAppliedCursor) {
            inputCtx.resetCursorRequest();
            return;
        }

        switch (requestedCursor) {
        case Core::Viewport::SceneCursor::inherit:
            break;
        case Core::Viewport::SceneCursor::pointer:
            window->getui().setCursorPointer();
            break;
        case Core::Viewport::SceneCursor::move:
            window->getui().setCursorMove();
            break;
        case Core::Viewport::SceneCursor::text:
            window->getui().setCursorText();
            break;
        case Core::Viewport::SceneCursor::resizeHorizontal:
            window->getui().setCursorResizeHorizontal();
            break;
        case Core::Viewport::SceneCursor::resizeVertical:
            window->getui().setCursorResizeVertical();
            break;
        case Core::Viewport::SceneCursor::resizeDiagonalNWSE:
            window->getui().setCursorResizeDiagonalNWSE();
            break;
        case Core::Viewport::SceneCursor::resizeDiagonalNESW:
            window->getui().setCursorResizeDiagonalNESW();
            break;
        case Core::Viewport::SceneCursor::normal:
            window->getui().setCursorNormal();
            break;
        }

        inputCtx.lastAppliedCursor = requestedCursor;
        inputCtx.resetCursorRequest();
    }

    void SceneViewportPanel::renderAttachedScene() {
        const auto sceneDriver = GAppContext::getInstance()
                                     .getSubSystem<Bess::ProjectSession>()
                                     ->getSubSystem<SceneDriver>();
        if (!m_attachedScene ||
            sceneDriver->getSceneWithId(m_attachedScene->getSceneId()) !=
                m_attachedScene) {
            setAttachedScene(sceneDriver->getActiveScene());
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

        bool isSchemMode = isSchematicMode();

        Canvas::View2D view{
            .camera = m_camera,
            .renderer = renderer,
            .drawRenderTarget = sceneHandle,
            .pickingRenderTarget = pickingHandle,
            .viewportCtx = m_viewportCtx,
            .simEngine = appCtx.getSubSystem<ProjectSession>()
                             ->getSubSystem<SimEngine::SimulationEngine>(),
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
                m_viewportCtx->pickingReadbackRequest = {};
                return;
            }

            if (!m_pendingSelectionReadback.active) {
                m_viewportCtx->inputCtx.pickingId =
                    pickingResult.firstOrInvalid();
            }
        }

        if (m_pendingSelectionReadback.active) {
            return;
        }

        const auto pickingHandle = textureHandle(m_pickingTexture);
        if (pickingHandle == 0) {
            return;
        }

        const glm::vec2 mousePos = m_viewportCtx->inputCtx.mousePos;
        const glm::vec2 textureSize = m_pickingTexture->getSize();
        const uint32_t width = positiveDimension(textureSize.x);
        const uint32_t height = positiveDimension(textureSize.y);

        const auto &selectionRequest = m_viewportCtx->pickingReadbackRequest;
        if (selectionRequest.active) {
            if (selectionRequest.width == 0 || selectionRequest.height == 0) {
                m_viewportCtx->pickingReadbackRequest = {};
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
            m_viewportCtx->pickingReadbackRequest = {};
            return;
        }

        if (!mouseMoved) {
            return;
        }

        if (mousePos.x < 0.f || mousePos.y < 0.f ||
            mousePos.x >= static_cast<float>(width) ||
            mousePos.y >= static_cast<float>(height)) {
            m_viewportCtx->inputCtx.pickingId = PickingId::invalid();
            return;
        }

        renderer->requestPickingId(pickingHandle,
                                   static_cast<uint32_t>(mousePos.x),
                                   static_cast<uint32_t>(mousePos.y));
    }
} // namespace Bess::UI
