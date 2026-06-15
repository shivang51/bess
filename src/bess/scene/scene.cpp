#include "scene/scene.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "common/types.h"
#include "layers/components_layer.h"
#include "layers/grid_layer.h"
#include "layers/hover_layer.h"
#include "layers/interaction_layer.h"
#include "layers/overlay_layer.h"
#include "layers/scene_widgets_layer.h"
#include "layers/screen_space_overlay_layer.h"
#include "scene/scene_event_builder.h"
#include "scene/scene_events.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/widgets/scene_widgets.h"
#include "scene_event.h"
#include "scene_layer.h"
#include "scene_types.h"
#include "settings/viewport_theme.h"
#include "sub_systems/input_sub_system.h"
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
        ViewportTransform &viewportTransform,
        SceneInputState &inputState,
        PickingId &pickingId,
        const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer) {
        SceneLifecycleContext ctx;
        ctx.sceneState = &state;
        ctx.camera = camera;
        ctx.viewportTransform = &viewportTransform;
        ctx.inputState = &inputState;
        ctx.pickingId = &pickingId;
        ctx.renderer = renderer;
        return ctx;
    }

    SceneUpdateContext makeUpdateContext(SceneState &state) {
        SceneUpdateContext ctx;
        ctx.sceneState = &state;
        return ctx;
    }

    SceneVpUpdateContext
    makeVpUpdateContext(SceneState &state,
                        const std::shared_ptr<Camera> &camera,
                        const ViewportTransform &viewportTransform,
                        SceneInputState &inputState,
                        const PickingId &pickingId) {
        SceneVpUpdateContext ctx;
        ctx.sceneState = &state;
        ctx.camera = camera;
        ctx.viewportTransform = &viewportTransform;
        ctx.inputState = &inputState;
        ctx.pickingId = &pickingId;
        return ctx;
    }

    SceneEventContext
    makeEventContext(SceneState &state,
                     const std::shared_ptr<Camera> &camera,
                     const ViewportTransform &viewportTransform,
                     SceneInputState &inputState,
                     const PickingId &pickingId) {
        SceneEventContext ctx;
        ctx.sceneState = &state;
        ctx.camera = camera;
        ctx.viewportTransform = &viewportTransform;
        ctx.inputState = &inputState;
        ctx.pickingId = &pickingId;
        return ctx;
    }

    SceneRenderContext makeRenderContext(
        SceneState &state,
        const std::shared_ptr<Camera> &camera,
        const ViewportTransform &viewportTransform,
        SceneInputState &inputState,
        const PickingId &pickingId,
        const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer) {
        SceneRenderContext ctx;
        ctx.sceneState = &state;
        ctx.camera = camera;
        ctx.viewportTransform = &viewportTransform;
        ctx.inputState = &inputState;
        ctx.pickingId = &pickingId;
        ctx.renderer = renderer;
        return ctx;
    }

    Scene::Scene() {
        m_sceneLayers.push_back(std::make_unique<GridLayer>());
        m_sceneLayers.push_back(std::make_unique<ComponentsLayer>());
        m_sceneLayers.push_back(std::make_unique<HoverLayer>());
        m_sceneLayers.push_back(std::make_unique<OverlayLayer>());
        m_sceneLayers.push_back(std::make_unique<InteractionLayer>());
        m_sceneLayers.push_back(std::make_unique<SceneWidgetsLayer>());
        auto screenOverlayLayer = std::make_unique<ScreenSpaceOverlayLayer>();
        m_screenSpaceOverlayLayer = screenOverlayLayer.get();
        m_sceneLayers.push_back(std::move(screenOverlayLayer));
        reset();
    }

    Scene::~Scene() {
        destroy();
    }

    void Scene::destroy() {
        if (m_isDestroyed)
            return;

        SceneWidgets::clearScene(&m_state);

        ViewportTransform transform;
        PickingId pickingId = PickingId::invalid();
        SceneInputState inputState;
        auto ctx = makeLifecycleContext(
            m_state, m_camera, transform, inputState, pickingId, nullptr);

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

        ViewportTransform transform;
        PickingId pickingId = PickingId::invalid();
        const auto &renderCtx = appCtx.getSubSystem<RendererContext>();
        auto ctx = makeLifecycleContext(m_state,
                                        m_camera,
                                        transform,
                                        inputState,
                                        pickingId,
                                        renderCtx->getRenderer());

        for (auto &layer : m_sceneLayers) {
            layer->reset(ctx);
        }

        for (auto &layer : m_sceneLayers) {
            layer->init(ctx);
        }
    }

    void Scene::clear() {
        SceneWidgets::clearScene(&m_state);
        m_state.clear();
        m_compZCoord = 1.f + m_zIncrement;
    }

    void Scene::update(TimeMs ts, bool isFocused) {
        m_frameTimeStep = ts;

        auto ctx = makeUpdateContext(m_state);

        for (auto &layer : m_sceneLayers) {
            layer->update(ts, ctx);
        }
    }

    void Scene::viewportUpdate(TimeMs ts,
                               bool isFocused,
                               const std::shared_ptr<Camera> &camera,
                               const ViewportTransform &viewportTransform,
                               SceneInputState &inputState,
                               const PickingId &pickingId) {
        m_frameTimeStep = ts;

        std::vector<SceneEvent> events;
        auto inputSystem =
            GAppContext::getInstance().getSubSystem<InputSubSystem>();
        inputState.isCtrlPressed = inputSystem->isCtrlPressed();
        inputState.isShiftPressed = inputSystem->isShiftPressed();
        inputState.isAltPressed = inputSystem->isAltPressed();

        if (isFocused) {
            events = SceneEventBuilder::buildFrameEvents(
                *inputSystem, camera, viewportTransform);
        }

        auto ctx = makeVpUpdateContext(
            m_state, camera, viewportTransform, inputState, pickingId);

        for (auto &evt : events) {
            dispatchEvent(
                evt, camera, viewportTransform, pickingId, inputState);
        }

        for (auto &layer : m_sceneLayers) {
            layer->viewportUpdate(ts, ctx);
        }
    }

    void Scene::draw(const View2D &view) {
        auto ctx = makeRenderContext(m_state,
                                     view.camera,
                                     view.viewportTransform,
                                     view.inputState,
                                     view.pickingId,
                                     view.renderer);

        const auto &extent = view.viewportTransform.size;
        view.renderer->beginFrame({
            .extent = {(uint32_t)extent.x, (uint32_t)extent.y},
            .clearColor = ViewportTheme::colors.background,
            .shouldClear = true,
            .targetTexture = view.drawRenderTarget,
            .pickingTexture = view.pickingRenderTarget,
            .cameraTransform = glm::value_ptr(view.camera->getTransform()),
        });

        SceneWidgets::beginFrame(&m_state);
        for (auto &layer : m_sceneLayers) {
            layer->draw(ctx);
        }
        SceneWidgets::endFrame(&m_state);

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

    void Scene::onMouseMove(const glm::vec2 &pos,
                            const std::shared_ptr<Camera> &camera,
                            const ViewportTransform &viewportTransform,
                            const PickingId &pickingId,
                            SceneInputState &inputState) {
        if (!camera) {
            return;
        }

        const auto viewportMousePos =
            getViewportMousePos(pos, viewportTransform.pos);
        const auto scenePos = camera->toWorldPos(viewportMousePos);

        SceneEvent::Data data;
        data.mouseMove = {
            .pos = scenePos,
            .delta = viewportMousePos - inputState.mousePos,
            .viewportPos = viewportMousePos,
        };
        SceneEvent evt{
            .type = SceneEvent::Type::mouseMove,
            .data = data,
            .isCtrlPressed = inputState.isCtrlPressed,
            .isShiftPressed = inputState.isShiftPressed,
            .isAltPressed = inputState.isAltPressed,
        };
        dispatchEvent(evt, camera, viewportTransform, pickingId, inputState);
    }

    void Scene::onMiddleMouse(bool isPressed,
                              const std::shared_ptr<Camera> &camera,
                              const ViewportTransform &viewportTransform,
                              const PickingId &pickingId,
                              SceneInputState &inputState) {
        SceneEvent::Data data;
        data.mouseButton = {.button = MouseButton::middle,
                            .action = isPressed ? MouseButtonAction::press
                                                : MouseButtonAction::release,
                            .pos = toScenePos(inputState.mousePos, camera)};
        SceneEvent evt{.type = SceneEvent::Type::mouseButton, .data = data};
        dispatchEvent(evt, camera, viewportTransform, pickingId, inputState);
    }

    void Scene::onLeftMouse(bool isPressed,
                            const std::shared_ptr<Camera> &camera,
                            const ViewportTransform &viewportTransform,
                            const PickingId &pickingId,
                            SceneInputState &inputState) {
        SceneEvent::Data data;
        data.mouseButton = {.button = MouseButton::left,
                            .action = isPressed ? MouseButtonAction::press
                                                : MouseButtonAction::release,
                            .pos = toScenePos(inputState.mousePos, camera)};
        SceneEvent evt{.type = SceneEvent::Type::mouseButton,
                       .data = data,
                       .isCtrlPressed = inputState.isCtrlPressed,
                       .isShiftPressed = inputState.isShiftPressed,
                       .isAltPressed = inputState.isAltPressed};
        dispatchEvent(evt, camera, viewportTransform, pickingId, inputState);
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

    bool Scene::getIsSchematicView() const {
        return m_state.getIsSchematicView();
    }

    void Scene::setIsSchematicView(bool value) {
        m_state.setIsSchematicView(value);
    }

    void Scene::toggleSchematicView() {
        m_state.setIsSchematicView(!m_state.getIsSchematicView());
    }

    bool Scene::dispatchEvent(SceneEvent &evt,
                              const std::shared_ptr<Camera> &camera,
                              const ViewportTransform &viewportTransform,
                              const PickingId &pickingId,
                              SceneInputState &inputState) {
        if (evt.type == SceneEvent::Type::mouseMove && camera) {
            const auto &data = evt.data.mouseMove;
            m_state.setMousePos(data.pos);
            inputState.dMousePos =
                data.pos - camera->toWorldPos(inputState.mousePos);
            inputState.mousePos = data.viewportPos;
        }

        evt.pickingId = pickingId;

        auto ctx = makeEventContext(
            m_state, camera, viewportTransform, inputState, pickingId);

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
        camera->focusAtPoint(comp->getAbsolutePosition(m_state));
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
