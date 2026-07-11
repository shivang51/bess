#include "bess_core/scene/scene.h"
#include "bess_core/scene/layers/components_layer.h"
#include "bess_core/scene/layers/grid_layer.h"
#include "bess_core/scene/layers/hover_layer.h"
#include "bess_core/scene/layers/interaction_layer.h"
#include "bess_core/scene/layers/overlay_layer.h"
#include "bess_core/scene/layers/scene_widgets_layer.h"
#include "bess_core/scene/layers/scratch_layer.h"
#include "bess_core/scene/layers/screen_space_overlay_layer.h"
#include "bess_core/scene/layers/ui_components_layer.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_event_builder.h"
#include "bess_core/scene/scene_events.h"
#include "bess_core/scene/scene_layer.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_types.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "bess_core/settings/viewport_theme.h"
#include "bess_core/sub_systems/input_sub_system.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "common/types.h"
#include "sub_systems/renderer_context.h"
#include <cstdint>
#include <gtc/type_ptr.hpp>
#include <memory>
#include <ranges>
#include <utility>

namespace Bess::Canvas {
    SceneLifecycleContext makeLifecycleContext(
        SceneState &state,
        const std::shared_ptr<Camera> &camera,
        const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
        const std::shared_ptr<Core::Viewport::ViewportContext> &viewportCtx) {
        SceneLifecycleContext ctx;
        ctx.sceneState = &state;
        ctx.camera = camera;
        ctx.renderer = renderer;
        ctx.viewportCtx = viewportCtx;
        ctx.sceneWidgetsState =
            SceneWidgets::getState(viewportCtx.get(), &state);
        return ctx;
    }

    SceneUpdateContext
    makeUpdateContext(SceneState &state,
                      const std::shared_ptr<Camera> &camera) {
        SceneUpdateContext ctx;
        ctx.sceneState = &state;
        ctx.camera = camera;
        return ctx;
    }

    SceneVpUpdateContext makeVpUpdateContext(
        SceneState &state,
        const std::shared_ptr<Camera> &camera,
        const std::shared_ptr<Core::Viewport::ViewportContext> &viewportCtx,
        const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer) {
        SceneVpUpdateContext ctx;
        ctx.sceneState = &state;
        ctx.camera = camera;
        ctx.viewportCtx = viewportCtx;
        ctx.renderer = renderer;
        ctx.sceneWidgetsState =
            SceneWidgets::getState(viewportCtx.get(), &state);
        return ctx;
    }

    SceneEventContext makeEventContext(
        SceneState &state,
        const std::shared_ptr<Camera> &camera,
        const std::shared_ptr<Core::Viewport::ViewportContext> &viewportCtx) {
        SceneEventContext ctx;
        ctx.sceneState = &state;
        ctx.camera = camera;
        ctx.viewportCtx = viewportCtx;
        ctx.sceneWidgetsState =
            SceneWidgets::getState(viewportCtx.get(), &state);
        return ctx;
    }

    SceneRenderContext makeRenderContext(
        SceneState &state,
        const std::shared_ptr<Camera> &camera,
        const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
        const std::shared_ptr<Core::Viewport::ViewportContext> &viewportCtx) {
        SceneRenderContext ctx;
        ctx.sceneState = &state;
        ctx.camera = camera;
        ctx.viewportCtx = viewportCtx;
        ctx.renderer = renderer;
        ctx.sceneWidgetsState =
            SceneWidgets::getState(viewportCtx.get(), &state);
        return ctx;
    }

    Scene::Scene() : Scene(true) {
    }

    Scene::Scene(bool initializeLayers) {
        if (!initializeLayers) {
            clear();
            m_size = glm::vec2(800.f, 600.f);
            m_camera = std::make_shared<Camera>(m_size.x, m_size.y);
            return;
        }

        m_sceneLayers.push_back(std::make_unique<GridLayer>());
        m_sceneLayers.push_back(std::make_unique<ComponentsLayer>());
        m_sceneLayers.push_back(std::make_unique<HoverLayer>());
        m_sceneLayers.push_back(std::make_unique<OverlayLayer>());
        m_sceneLayers.push_back(std::make_unique<InteractionLayer>());
        m_sceneLayers.push_back(std::make_unique<UIComponentsLayer>());
        m_sceneLayers.push_back(std::make_unique<SceneWidgetsLayer>());
        m_sceneLayers.push_back(std::make_unique<ScratchLayer>());
        auto screenOverlayLayer = std::make_unique<ScreenSpaceOverlayLayer>();
        m_screenSpaceOverlayLayer = screenOverlayLayer.get();

#ifdef BESS_DEBUG
        m_sceneLayers.push_back(std::move(screenOverlayLayer));
#endif

        reset();
    }

    Scene::~Scene() {
        destroy();
    }

    void Scene::destroy() {
        if (m_isDestroyed)
            return;

        auto ctx = makeLifecycleContext(m_state, m_camera, nullptr, nullptr);

        BESS_INFO("[Scene] Destroying {}", (uint64_t)m_state.getSceneId());

        for (auto &layer : m_sceneLayers) {
            layer->destroy(ctx);
        }

        m_sceneLayers.clear();
        m_screenSpaceOverlayLayer = nullptr;
        m_isDestroyed = true;
        m_state.clear();
    }

    void Scene::reset() {
        clear();

        m_size = glm::vec2(800.f, 600.f);
        m_camera = std::make_shared<Camera>(m_size.x, m_size.y);

        SceneInputState inputState;
        inputState.mousePos = {0.f, 0.f};

        const auto &appCtx = Bess::GAppContext::getInstance();

        const auto &rendererCtx = appCtx.getSubSystem<RendererContext>();

        auto ctx = makeLifecycleContext(
            m_state, m_camera, rendererCtx->getRenderer(), nullptr);

        for (auto &layer : m_sceneLayers) {
            layer->reset(ctx);
        }

        for (auto &layer : m_sceneLayers) {
            layer->init(ctx);
        }
    }

    void Scene::clear() {
        m_state.clear();
        m_compZCoord = 1.f + m_zIncrement;
    }

    void Scene::update(TimeMs ts, bool isFocused) {
        m_frameTimeStep = ts;

        auto ctx = makeUpdateContext(m_state, m_camera);

        for (auto &layer : m_sceneLayers) {
            layer->update(ts, ctx);
        }
    }

    void Scene::viewportUpdate(TimeMs ts, const ViewportUpdateContext &ctx) {
        m_frameTimeStep = ts;

        std::vector<SceneEvent> events;
        auto inputSystem =
            GAppContext::getInstance().getSubSystem<InputSubSystem>();
        auto &inputState = ctx.viewportCtx->inputCtx;
        inputState.isCtrlPressed = inputSystem->isCtrlPressed();
        inputState.isShiftPressed = inputSystem->isShiftPressed();
        inputState.isAltPressed = inputSystem->isAltPressed();

        if (ctx.isFocused) {
            events = SceneEventBuilder::buildFrameEvents(
                *inputSystem, ctx.camera, ctx.viewportCtx->transform);
        }

        for (auto &evt : events) {
            dispatchEvent(evt, ctx.camera, ctx.viewportCtx);
        }

        auto updateCtx = makeVpUpdateContext(
            m_state, ctx.camera, ctx.viewportCtx, ctx.renderer);

        for (auto &layer : m_sceneLayers) {
            layer->viewportUpdate(ts, updateCtx);
        }
    }

    void Scene::draw(const View2D &view) {
        BESS_ASSERT(view.viewportCtx,
                    "Viewport context is required to draw the scene");
        BESS_ASSERT(view.camera, "Camera is required to draw the scene");
        BESS_ASSERT(view.renderer, "Renderer is required to draw the scene");
        BESS_ASSERT(view.drawRenderTarget,
                    "Draw render target is required to draw the scene");
        BESS_ASSERT(view.pickingRenderTarget,
                    "Picking render target is required to draw the scene");

        auto ctx = makeRenderContext(
            m_state, view.camera, view.renderer, view.viewportCtx);

        const auto &extent = view.viewportCtx->transform.size;
        view.renderer->beginFrame({
            .extent = {(uint32_t)extent.x, (uint32_t)extent.y},
            .clearColor = ViewportTheme::colors.background,
            .shouldClear = true,
            .targetTexture = view.drawRenderTarget,
            .pickingTexture = view.pickingRenderTarget,
            .cameraTransform = glm::value_ptr(view.camera->getTransform()),
        });

        SceneWidgets::beginFrame(ctx.sceneWidgetsState);
        for (auto &layer : m_sceneLayers) {
            layer->draw(ctx);
        }
        SceneWidgets::endFrame(ctx.sceneWidgetsState);

        ctx.renderer->endFrame();
    }

    void
    Scene::addScreenOverlayDrawCallback(ScreenOverlayDrawCallback callback) {
        if (m_screenSpaceOverlayLayer) {
            m_screenSpaceOverlayLayer->addDrawCallback(std::move(callback));
        }
    }

    void Scene::clearScreenOverlayDrawCallbacks() {
        if (m_screenSpaceOverlayLayer) {
            m_screenSpaceOverlayLayer->clearDrawCallbacks();
        }
    }

    void Scene::selectAllEntities() {
        m_state.clearSelectedComponents();
        const auto &comps = m_state.getRootComponents();

        for (auto &id : comps) {
            m_state.addSelectedComponent(id);
        }
    }

    bool Scene::deleteSceneEntity(const UUID &entUuid) {
        const auto ids = m_state.removeComponent(entUuid);
        BESS_INFO("[Scene] Deleted entity {}", (uint64_t)entUuid);
        return !ids.empty();
    }

    void Scene::deleteSelectedSceneEntities() {
        const auto selectedComps = m_state.getSelectedComponents();
        std::vector<UUID> entsToDelete;
        entsToDelete.reserve(selectedComps.size());
        for (const auto &comp : selectedComps) {
            entsToDelete.emplace_back(comp.first);
        }

        for (const auto &entUuid : entsToDelete) {
            deleteSceneEntity(entUuid);
        }
    }

    const glm::vec2 &Scene::getSize() const {
        return m_size;
    }

    void Scene::resize(const glm::vec2 &size) {
        m_size = size;
    }

    glm::vec2 Scene::getViewportMousePos(const glm::vec2 &mousePos,
                                         const glm::vec2 &viewportPos) const {
        auto x = mousePos.x - viewportPos.x;
        auto y = mousePos.y - viewportPos.y;
        return {x, y};
    }

    glm::vec2 Scene::toScenePos(const glm::vec2 &mousePos,
                                const std::shared_ptr<Camera> &camera) const {
        return camera ? camera->toWorldPos(mousePos) : glm::vec2{0.f};
    }

    void Scene::onMouseMove(
        const glm::vec2 &pos,
        const std::shared_ptr<Camera> &camera,
        const std::shared_ptr<Core::Viewport::ViewportContext> &viewportCtx) {
        if (!camera) {
            return;
        }

        const auto viewportMousePos =
            getViewportMousePos(pos, viewportCtx->transform.pos);
        const auto scenePos = camera->toWorldPos(viewportMousePos);

        SceneEvent::Data data;
        data.mouseMove = {
            .pos = scenePos,
            .delta = viewportMousePos - viewportCtx->inputCtx.mousePos,
            .viewportPos = viewportMousePos,
        };
        SceneEvent evt{
            .type = SceneEvent::Type::mouseMove,
            .data = data,
            .isCtrlPressed = viewportCtx->inputCtx.isCtrlPressed,
            .isShiftPressed = viewportCtx->inputCtx.isShiftPressed,
            .isAltPressed = viewportCtx->inputCtx.isAltPressed,
        };
        dispatchEvent(evt, camera, viewportCtx);
    }

    void Scene::onMiddleMouse(
        bool isPressed,
        const std::shared_ptr<Camera> &camera,
        const std::shared_ptr<Core::Viewport::ViewportContext> &viewportCtx) {
        SceneEvent::Data data;
        data.mouseButton = {
            .button = MouseButton::middle,
            .action = isPressed ? MouseButtonAction::press
                                : MouseButtonAction::release,
            .pos = toScenePos(viewportCtx->inputCtx.mousePos, camera)};
        SceneEvent evt{.type = SceneEvent::Type::mouseButton, .data = data};
        dispatchEvent(evt, camera, viewportCtx);
    }

    void Scene::onLeftMouse(
        bool isPressed,
        const std::shared_ptr<Camera> &camera,
        const std::shared_ptr<Core::Viewport::ViewportContext> &viewportCtx) {

        SceneEvent::Data data;
        data.mouseButton = {
            .button = MouseButton::left,
            .action = isPressed ? MouseButtonAction::press
                                : MouseButtonAction::release,
            .pos = toScenePos(viewportCtx->inputCtx.mousePos, camera)};

        SceneEvent evt{
            .type = SceneEvent::Type::mouseButton,
            .data = data,
            .isCtrlPressed = viewportCtx->inputCtx.isCtrlPressed,
            .isShiftPressed = viewportCtx->inputCtx.isShiftPressed,
            .isAltPressed = viewportCtx->inputCtx.isAltPressed,
        };

        dispatchEvent(evt, camera, viewportCtx);
    }

    float Scene::getNextZCoord() {
        const float z = m_compZCoord;
        m_compZCoord += m_zIncrement;
        return z;
    }

    void Scene::setZCoord(float value) {
        m_compZCoord = value + m_zIncrement;
    }

    void Scene::setSceneMode(SceneMode mode) {
        m_sceneMode = mode;
    }

    SceneMode Scene::getSceneMode() const {
        return m_sceneMode;
    }

    bool Scene::dispatchEvent(
        SceneEvent &evt,
        const std::shared_ptr<Camera> &camera,
        const std::shared_ptr<Core::Viewport::ViewportContext> &viewportCtx) {

        if (evt.type == SceneEvent::Type::mouseMove && camera) {
            const auto &data = evt.data.mouseMove;
            m_state.setMousePos(data.pos);
            viewportCtx->inputCtx.dMousePos =
                data.pos - camera->toWorldPos(viewportCtx->inputCtx.mousePos);
            viewportCtx->inputCtx.mousePos = data.viewportPos;
        }

        evt.pickingId = viewportCtx->inputCtx.pickingId;

        auto ctx = makeEventContext(m_state, camera, viewportCtx);

        bool wasHandled = false;
        bool wasConsumed = false;
        for (auto &layer : std::ranges::reverse_view(m_sceneLayers)) {
            if (wasConsumed && !layer->shouldReceiveConsumedEvent(evt)) {
                continue;
            }

            const auto result = layer->handleEvent(evt, ctx);
            if (result == EventResult::Ignored) {
                continue;
            }

            wasHandled = true;
            if (result == EventResult::Consumed) {
                wasConsumed = true;
            }
        }

        return wasHandled;
    }

    const SceneState &Scene::getState() const {
        return m_state;
    }

    SceneState &Scene::getState() {
        return m_state;
    }

    void Scene::focusCameraOnSelected(const std::shared_ptr<Camera> &camera) {
        if (!camera) {
            return;
        }

        const auto &selectedComps =
            m_state.getSelectedComponents() | std::ranges::views::keys;

        if (selectedComps.empty()) {
            return;
        }

        const auto &comp = m_state.getComponentByUuid(*selectedComps.begin());
        camera->focusAtPoint(comp->getAbsolutePosition(m_state, false));
    }

    void Scene::addComponent(const std::shared_ptr<SceneComponent> &comp,
                             bool setZ) {
        if (setZ) {
            auto pos = comp->getTransform().position;
            pos.z = getNextZCoord();
            comp->setPosition(
                pos); // doing it like this so, change cb is called
        }
        m_state.addComponent(comp);
    }

    const UUID &Scene::getSceneId() const {
        return m_state.getSceneId();
    }

} // namespace Bess::Canvas
