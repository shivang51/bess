#include "ui/ui_main/scene_viewport_controller.h"

#include "GLFW/glfw3.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene_driver.h"
#include "bess_core/sub_systems/input_sub_system.h"
#include "common/logger.h"
#include "sub_systems/renderer_context.h"
#include "ui/ui_main/ui_main.h"
#include "window.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <unordered_set>

namespace Bess::UI {
    namespace {
        constexpr float kViewportInsetPx = 1.f;
        constexpr float kEdgePanStep = 10.f;
        constexpr float kMinViewportExtent = 2.f;

        [[nodiscard]] bool isValidExtent(const glm::vec2 &size) noexcept {
            return size.x > kMinViewportExtent && size.y > kMinViewportExtent &&
                   std::isfinite(size.x) && std::isfinite(size.y);
        }

        [[nodiscard]] uint32_t positiveDimension(float value) noexcept {
            return value > 1.f ? static_cast<uint32_t>(value) : 1u;
        }

        [[nodiscard]] std::shared_ptr<Window> appWindow() noexcept {
            try {
                return GAppContext::getInstance().getSubSystem<Window>();
            } catch (...) {
                return nullptr;
            }
        }

        [[nodiscard]] glm::vec2 windowToFramebufferScale(
            const std::shared_ptr<Window> &window) noexcept {
            if (window == nullptr) {
                return {1.f, 1.f};
            }

            int contentW = 0;
            int contentH = 0;
            int fbW = 0;
            int fbH = 0;
            if (auto *glfw = window->getGLFWHandle()) {
                glfwGetWindowSize(glfw, &contentW, &contentH);
                glfwGetFramebufferSize(glfw, &fbW, &fbH);
            }

            const float scaleX =
                contentW > 0 ? static_cast<float>(fbW) /
                                   static_cast<float>(contentW)
                             : 1.f;
            const float scaleY =
                contentH > 0 ? static_cast<float>(fbH) /
                                   static_cast<float>(contentH)
                             : 1.f;
            return {
                std::isfinite(scaleX) && scaleX > 0.f ? scaleX : 1.f,
                std::isfinite(scaleY) && scaleY > 0.f ? scaleY : 1.f,
            };
        }
    } // namespace

    SceneViewportController::SceneViewportController(std::string name)
        : m_name(std::move(name)) {
        static size_t viewportCounter = 0;
        m_viewportCtx = std::make_shared<Core::Viewport::ViewportContext>();
        m_viewportCtx->viewportId = viewportCounter++;
    }

    SceneViewportController::~SceneViewportController() {
        UIMain::unregisterSceneViewportController(this);
    }

    void SceneViewportController::onAttach(SceneView &view) {
        m_view = &view;
        try {
            UIMain::registerSceneViewportController(shared_from_this());
        } catch (const std::bad_weak_ptr &) {
        }
        m_geometryDirty = true;
        view.requestRender();
    }

    void SceneViewportController::onDetach(SceneView &) noexcept {
        m_view = nullptr;
        m_hovered = false;
        m_pointerInside = false;
        m_pendingSelectionReadback.clear();
        if (m_viewportCtx) {
            m_viewportCtx->isFocused = false;
            m_viewportCtx->inputCtx.resetCursorRequest();
        }
        UIMain::unregisterSceneViewportController(this);
    }

    void SceneViewportController::update(SceneViewUpdateContext &context) {
        m_bounds = context.bounds;
        applyGeometry(context.bounds, context.view.extent());
        processInteraction(context.deltaTime, !context.bounds.empty());
    }

    void SceneViewportController::render(SceneViewFrameContext &frame) {
        applyGeometry(frame.bounds, frame.extent);

        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();
        if (sceneDriver == nullptr || sceneDriver->getIsPaused()) {
            return;
        }

        syncAttachedScene();
        if (!m_attachedScene || !m_camera || !hasRenderableViewport()) {
            return;
        }
        if (frame.colorTarget == 0 || frame.pickingTarget == 0) {
            return;
        }

        auto renderer = GAppContext::getInstance()
                            .getSubSystem<RendererContext>()
                            ->getRenderer();
        if (renderer == nullptr) {
            return;
        }

        Canvas::View2D view{
            .camera = m_camera,
            .renderer = renderer,
            .drawRenderTarget = frame.colorTarget,
            .pickingRenderTarget = frame.pickingTarget,
            .viewportCtx = m_viewportCtx,
        };

        m_attachedScene->draw(view);
        if (m_attachedScene->getIsFirstFrame()) {
            m_attachedScene->setIsFirstFrame(false);
        }
    }

    UIEventReply SceneViewportController::onEvent(SceneView &view,
                                                  WidgetEventContext &context,
                                                  const UIEvent &event) {
        UIEventReply reply{};

        if (const auto *crossing = event.getIf<UIPointerCrossingEvent>()) {
            m_pointerInside = crossing->entered;
            m_hovered = crossing->entered || hasMouseCapture();
            if (!m_hovered && m_viewportCtx) {
                m_viewportCtx->inputCtx.pickingId = PickingId::invalid();
                m_viewportCtx->inputCtx.resetCursorRequest();
            }
            reply.handled = true;
            return reply;
        }

        if (const auto *focus = event.getIf<UIFocusChangedEvent>()) {
            if (m_viewportCtx) {
                m_viewportCtx->isFocused = focus->focused || m_hovered;
            }
            reply.handled = true;
            return reply;
        }

        if (event.getIf<UIPointerCancelEvent>() != nullptr) {
            if (m_attachedScene && m_camera && m_viewportCtx) {
                if (m_viewportCtx->inputCtx.isLeftMousePressed) {
                    m_attachedScene->onLeftMouse(false, m_camera, m_viewportCtx);
                }
                if (m_viewportCtx->inputCtx.isMiddleMousePressed) {
                    m_attachedScene->onMiddleMouse(
                        false, m_camera, m_viewportCtx);
                }
            }
            reply.releasePointer = true;
            reply.handled = true;
            return reply;
        }

        if (const auto *button = event.getIf<Input::MouseButtonEvent>()) {
            if (button->action == MouseButtonAction::press &&
                (button->button == MouseButton::left ||
                 button->button == MouseButton::middle)) {
                reply.capturePointer = true;
                reply.requestFocus = true;
                m_hovered = true;
            } else if (button->action == MouseButtonAction::release) {
                reply.releasePointer = !hasMouseCapture();
            }
            reply.handled = true;
            reply.stopPropagation = true;
            view.requestRender();
            return reply;
        }

        if (event.getIf<Input::MouseMoveEvent>() != nullptr ||
            event.getIf<Input::MouseWheelEvent>() != nullptr) {
            m_hovered = m_pointerInside || context.hovered || hasMouseCapture();
            reply.handled = true;
            reply.stopPropagation = true;
            return reply;
        }

        if (event.getIf<Input::KeyEvent>() != nullptr ||
            event.getIf<Input::TextInputEvent>() != nullptr) {
            if (isFocused() || context.focused) {
                reply.handled = true;
                reply.stopPropagation = true;
            }
            return reply;
        }

        return reply;
    }

    CursorIcon SceneViewportController::cursor(
        const SceneView &,
        const WidgetCursorContext &context) const noexcept {
        if ((!m_hovered && !context.captured) || m_viewportCtx == nullptr) {
            return CursorIcon::inherit;
        }

        const auto requested = m_viewportCtx->inputCtx.cursorRequest.cursor;
        if (requested == Core::Viewport::SceneCursor::inherit) {
            return CursorIcon::inherit;
        }
        return mapSceneCursor(requested);
    }

    const std::string &SceneViewportController::name() const noexcept {
        return m_name;
    }

    bool SceneViewportController::isHovered() const noexcept {
        return m_hovered;
    }

    bool SceneViewportController::isFocused() const noexcept {
        return m_hovered || (m_viewportCtx && m_viewportCtx->isFocused) ||
               hasMouseCapture();
    }

    bool SceneViewportController::isUsable() const noexcept {
        return m_view != nullptr && m_attachedScene != nullptr &&
               hasRenderableViewport();
    }

    bool SceneViewportController::isAttachedToScene(
        const std::shared_ptr<Canvas::Scene> &scene) const {
        return scene && m_attachedScene &&
               scene->getSceneId() == m_attachedScene->getSceneId();
    }

    void SceneViewportController::focusCameraOnSelected() {
        if (m_attachedScene && m_camera) {
            m_attachedScene->focusCameraOnSelected(m_camera);
        }
    }

    bool SceneViewportController::isSchematicMode() const noexcept {
        return m_viewportCtx &&
               m_viewportCtx->mode == Core::Viewport::ViewportMode::schematic;
    }

    bool SceneViewportController::toggleSchematicMode() {
        if (!m_viewportCtx) {
            return false;
        }
        m_viewportCtx->toggleSchematicMode();
        if (m_view != nullptr) {
            m_view->requestRender();
        }
        return isSchematicMode();
    }

    void SceneViewportController::updateAttachedSceneId(const UUID &sceneId) {
        if (m_viewportCtx) {
            m_viewportCtx->updateSceneId = sceneId;
        }
    }

    void SceneViewportController::onSceneAttached() {
        if (m_viewportCtx) {
            m_viewportCtx->reset();
        }
        m_pendingSelectionReadback.clear();
        m_geometryDirty = true;
        if (m_view != nullptr) {
            m_view->requestRender();
        }
    }

    void SceneViewportController::ensureCamera(const glm::vec2 &size) {
        if (!isValidExtent(size)) {
            return;
        }
        if (!m_camera) {
            m_camera = std::make_shared<Camera>(size.x, size.y);
            return;
        }
        if (m_geometryDirty) {
            m_camera->resize(size.x, size.y);
            m_geometryDirty = false;
        }
    }

    void SceneViewportController::syncAttachedScene() {
        auto projectCtx =
            GAppContext::getInstance().getSubSystem<Bess::ProjectContext>();
        if (projectCtx == nullptr) {
            return;
        }
        auto sceneDriver = projectCtx->getSubSystem<SceneDriver>();
        if (sceneDriver == nullptr) {
            return;
        }

        const auto activeScene = sceneDriver->getActiveScene();
        if (!m_attachedScene ||
            sceneDriver->getSceneWithId(m_attachedScene->getSceneId()) !=
                m_attachedScene) {
            setAttachedScene(activeScene);
        }

        if (m_viewportCtx && m_viewportCtx->updateSceneId != UUID::null) {
            setAttachedScene(
                sceneDriver->getSceneWithId(m_viewportCtx->updateSceneId));
            m_viewportCtx->updateSceneId = UUID::null;
        }

        if (!m_attachedScene) {
            setAttachedScene(activeScene);
        }

        if (m_nextSceneId != UUID::null) {
            sceneDriver->setActiveScene(m_nextSceneId);
            m_nextSceneId = UUID::null;
        }
    }

    void SceneViewportController::applyGeometry(
        const WidgetBounds &bounds, Core::Renderer::Renderer2DExtent extent) {
        if (!m_viewportCtx) {
            return;
        }

        const glm::vec2 pixelSize{
            static_cast<float>(std::max(1U, extent.width)),
            static_cast<float>(std::max(1U, extent.height)),
        };

        const auto window = appWindow();
        const glm::vec2 scale = windowToFramebufferScale(window);
        const glm::vec2 topLeftUi = bounds.topLeft();

        const glm::vec2 topLeftWindow{topLeftUi.x / scale.x,
                                      topLeftUi.y / scale.y};

        const bool sizeChanged =
            m_viewportCtx->transform.size != pixelSize ||
            m_viewportCtx->transform.inputScale != scale;
        const bool posChanged = m_viewportCtx->transform.pos != topLeftWindow;

        if (sizeChanged) {
            m_viewportCtx->transform.size = pixelSize;
            m_viewportCtx->transform.inputScale = scale;
            m_viewportCtx->isResized = true;
            m_geometryDirty = true;
            m_pendingSelectionReadback.clear();
            ensureCamera(pixelSize);
        } else if (m_geometryDirty) {
            ensureCamera(pixelSize);
        }

        if (posChanged || sizeChanged) {
            m_viewportCtx->transform.pos = topLeftWindow;
        }
    }

    bool SceneViewportController::hasRenderableViewport() const noexcept {
        return m_viewportCtx && isValidExtent(m_viewportCtx->transform.size);
    }

    bool SceneViewportController::hasMouseCapture() const noexcept {
        if (!m_viewportCtx) {
            return false;
        }
        const auto &inputCtx = m_viewportCtx->inputCtx;
        return inputCtx.isLeftMousePressed || inputCtx.isMiddleMousePressed ||
               inputCtx.isDragging || m_viewportCtx->selBoxCtx.draw;
    }

    void SceneViewportController::processInteraction(TimeMs ts,
                                                     bool effectivelyVisible) {
        syncAttachedScene();
        if (!m_attachedScene || !m_camera || !m_viewportCtx) {
            return;
        }

        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();
        if (sceneDriver == nullptr) {
            return;
        }

        ensureCamera(m_viewportCtx->transform.size);
        m_camera->update(ts);

        const bool focused = effectivelyVisible && isFocused();
        m_viewportCtx->isFocused = focused;

        if (focused && m_attachedScene != sceneDriver->getActiveScene()) {
            sceneDriver->setActiveScene(m_attachedScene->getSceneId());
            m_viewportCtx->inputCtx.pickingId = PickingId::invalid();
        }

        auto renderer = GAppContext::getInstance()
                            .getSubSystem<RendererContext>()
                            ->getRenderer();

        Canvas::ViewportUpdateContext ctx{
            .isFocused = focused,
            .camera = m_camera,
            .viewportCtx = m_viewportCtx,
            .renderer = renderer,
        };

        if (!sceneDriver->getIsPaused()) {
            m_attachedScene->viewportUpdate(ts, ctx);
        }

        auto inputSystem =
            GAppContext::getInstance().getSubSystem<InputSubSystem>();
        if (inputSystem == nullptr) {
            return;
        }

        const auto &frameInputState = inputSystem->getFrameInpState();
        bool mouseMoved = false;
        if (frameInputState.hasMouseMoved) {
            mouseMoved = true;
            handleEdgePan(inputSystem->getMouseMoveState().pos);
        }

        if (frameInputState.hasMouseBtnEvent) {
            releaseMouseButtonOutsideViewport(frameInputState.mouseBtnState);
        }

        const bool shouldProcessPicking =
            m_hovered || m_viewportCtx->pickingReadbackRequest.active ||
            m_pendingSelectionReadback.active;

        if (shouldProcessPicking && !m_attachedScene->getIsFirstFrame() &&
            !m_viewportCtx->inputCtx.isDragging) {
            updatePickingIds(mouseMoved && m_hovered);
        }

        if (!m_hovered && !hasMouseCapture()) {
            m_viewportCtx->inputCtx.pickingId = PickingId::invalid();
            m_viewportCtx->inputCtx.resetCursorRequest();
        }

        if (m_viewportCtx->isResized) {
            m_viewportCtx->isResized = false;
        }
    }

    void SceneViewportController::updatePickingIds(bool mouseMoved) {
        if (!m_attachedScene || m_view == nullptr) {
            m_pendingSelectionReadback.clear();
            return;
        }

        Core::Renderer::PickingReadbackResult pickingResult;
        if (m_view->tryGetPickingIds(pickingResult) && !pickingResult.empty()) {
            const bool isSelectionResult = m_pendingSelectionReadback.matches(
                pickingResult.x,
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
                    if (const auto comp =
                            sceneState.getComponentByPickingId(id)) {
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
            static_cast<void>(m_view->requestPickingIds(
                m_pendingSelectionReadback.x,
                m_pendingSelectionReadback.y,
                m_pendingSelectionReadback.width,
                m_pendingSelectionReadback.height));
            m_viewportCtx->pickingReadbackRequest = {};
            return;
        }

        if (!mouseMoved) {
            return;
        }

        const glm::vec2 mousePos = m_viewportCtx->inputCtx.mousePos;
        const uint32_t width =
            positiveDimension(m_viewportCtx->transform.size.x);
        const uint32_t height =
            positiveDimension(m_viewportCtx->transform.size.y);
        if (mousePos.x < 0.f || mousePos.y < 0.f ||
            mousePos.x >= static_cast<float>(width) ||
            mousePos.y >= static_cast<float>(height)) {
            m_viewportCtx->inputCtx.pickingId = PickingId::invalid();
            return;
        }

        static_cast<void>(m_view->requestPickingId(
            static_cast<uint32_t>(mousePos.x),
            static_cast<uint32_t>(mousePos.y)));
    }

    void SceneViewportController::handleEdgePan(const glm::vec2 &windowMouse) {
        if (!m_attachedScene || !m_camera || !m_viewportCtx) {
            return;
        }

        const auto window = appWindow();
        if (window == nullptr) {
            return;
        }

        if (isInsideViewportWindow(windowMouse)) {
            window->setEnableCursor(true);
            return;
        }

        if (!m_viewportCtx->inputCtx.isLeftMousePressed) {
            window->setEnableCursor(true);
            return;
        }

        const auto sizeWindow = windowSpaceSize();
        const auto &pos = m_viewportCtx->transform.pos;
        auto vel = glm::vec2(0.f);
        if (windowMouse.x < pos.x) {
            vel.x = -kEdgePanStep;
        } else if (windowMouse.x > pos.x + sizeWindow.x) {
            vel.x = kEdgePanStep;
        }
        if (windowMouse.y < pos.y) {
            vel.y = -kEdgePanStep;
        } else if (windowMouse.y > pos.y + sizeWindow.y) {
            vel.y = kEdgePanStep;
        }

        auto newPos = windowMouse;
        newPos.x = std::clamp(newPos.x,
                              pos.x + kViewportInsetPx,
                              pos.x + sizeWindow.x - kViewportInsetPx);
        newPos.y = std::clamp(newPos.y,
                              pos.y + kViewportInsetPx,
                              pos.y + sizeWindow.y - kViewportInsetPx);

        m_camera->incrementPos(vel);
        window->setEnableCursor(false);
        window->setMousePos(newPos);
        m_attachedScene->onMouseMove(newPos, m_camera, m_viewportCtx);
    }

    void SceneViewportController::releaseMouseButtonOutsideViewport(
        const Input::MouseButtonEvent &mouseBtnState) {
        if (!m_attachedScene || !m_camera || !m_viewportCtx) {
            return;
        }
        if (mouseBtnState.action != MouseButtonAction::release ||
            isInsideViewportWindow(mouseBtnState.pos)) {
            return;
        }

        if (mouseBtnState.button == MouseButton::left &&
            m_viewportCtx->inputCtx.isLeftMousePressed) {
            m_attachedScene->onLeftMouse(false, m_camera, m_viewportCtx);
        } else if (mouseBtnState.button == MouseButton::middle &&
                   m_viewportCtx->inputCtx.isMiddleMousePressed) {
            m_attachedScene->onMiddleMouse(false, m_camera, m_viewportCtx);
        }
    }

    bool SceneViewportController::isInsideViewportWindow(
        const glm::vec2 &windowPos) const noexcept {
        if (!hasRenderableViewport()) {
            return false;
        }
        const auto size = windowSpaceSize();
        const auto &pos = m_viewportCtx->transform.pos;
        return windowPos.x >= pos.x && windowPos.x < pos.x + size.x &&
               windowPos.y >= pos.y && windowPos.y < pos.y + size.y;
    }

    glm::vec2 SceneViewportController::windowSpaceSize() const noexcept {
        if (!m_viewportCtx) {
            return {};
        }
        const auto scale = m_viewportCtx->transform.inputScale;
        return {
            m_viewportCtx->transform.size.x /
                (scale.x > 0.f ? scale.x : 1.f),
            m_viewportCtx->transform.size.y /
                (scale.y > 0.f ? scale.y : 1.f),
        };
    }

    CursorIcon SceneViewportController::mapSceneCursor(
        Core::Viewport::SceneCursor cursor) noexcept {
        switch (cursor) {
        case Core::Viewport::SceneCursor::pointer:
            return CursorIcon::pointer;
        case Core::Viewport::SceneCursor::move:
            return CursorIcon::move;
        case Core::Viewport::SceneCursor::text:
            return CursorIcon::text;
        case Core::Viewport::SceneCursor::resizeHorizontal:
            return CursorIcon::resizeHorizontal;
        case Core::Viewport::SceneCursor::resizeVertical:
            return CursorIcon::resizeVertical;
        case Core::Viewport::SceneCursor::resizeDiagonalNWSE:
            return CursorIcon::resizeDiagonalNWSE;
        case Core::Viewport::SceneCursor::resizeDiagonalNESW:
            return CursorIcon::resizeDiagonalNESW;
        case Core::Viewport::SceneCursor::normal:
            return CursorIcon::arrow;
        case Core::Viewport::SceneCursor::inherit:
        default:
            return CursorIcon::inherit;
        }
    }

} // namespace Bess::UI
