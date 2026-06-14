#include "scene/scene.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
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
#include "sub_systems/input_sub_system.h"
#include "sub_systems/renderer_context.h"
#include <cstdint>
#include <gtc/type_ptr.hpp>
#include <memory>
#include <ranges>
#include <unordered_set>
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

    SceneUpdateContext makeUpdateContext(SceneState &state,
                                         ViewportTransform &viewportTransform,
                                         SceneInputState &inputState,
                                         PickingId &pickingId) {
        SceneUpdateContext ctx;
        ctx.sceneState = &state;
        ctx.camera = nullptr;
        ctx.viewportTransform = &viewportTransform;
        ctx.inputState = &inputState;
        ctx.pickingId = &pickingId;
        return ctx;
    }

    SceneEventContext makeEventContext(SceneState &state,
                                       const std::shared_ptr<Camera> &camera,
                                       ViewportTransform &viewportTransform,
                                       SceneInputState &inputState,
                                       PickingId &pickingId) {
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
        ViewportTransform &viewportTransform,
        SceneInputState &inputState,
        PickingId &pickingId,
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

        auto ctx = makeLifecycleContext(m_state,
                                        m_camera,
                                        m_viewportTransform,
                                        m_inputState,
                                        m_pickingId,
                                        nullptr);
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

        m_inputState.mousePos = {0.f, 0.f};

        const auto &appCtx = Bess::GAppContext::getInstance();

        const auto &renderCtx = appCtx.getSubSystem<RendererContext>();
        auto ctx = makeLifecycleContext(m_state,
                                        m_camera,
                                        m_viewportTransform,
                                        m_inputState,
                                        m_pickingId,
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
        m_inputState.reset();
        m_pickingId = PickingId::invalid();
    }

    void Scene::update(TimeMs ts, bool isFocused) {
        m_frameTimeStep = ts;

        std::vector<SceneEvent> events;
        if (isFocused) {
            auto inputSystem =
                GAppContext::getInstance().getSubSystem<InputSubSystem>();
            m_inputState.isCtrlPressed = inputSystem->isCtrlPressed();
            m_inputState.isShiftPressed = inputSystem->isShiftPressed();
            m_inputState.isAltPressed = inputSystem->isAltPressed();
            events = SceneEventBuilder::buildFrameEvents(
                *inputSystem, m_camera, m_viewportTransform);
        }

        m_camera->update(ts);

        auto ctx = makeUpdateContext(
            m_state, m_viewportTransform, m_inputState, m_pickingId);

        for (auto &evt : events) {
            dispatchEvent(evt);
        }

        for (auto &layer : m_sceneLayers) {
            layer->update(ts, ctx);
        }

        const auto &rootComps = m_state.getRootComponents();

        for (const auto &compId : rootComps) {
            auto comp = m_state.getComponentByUuid(compId);
            if (comp) {
                comp->update(ts, m_state);
            }
        }
    }

    void
    Scene::draw(const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer) {
        auto ctx = makeRenderContext(m_state,
                                     m_camera,
                                     m_viewportTransform,
                                     m_inputState,
                                     m_pickingId,
                                     renderer);

        SceneWidgets::beginFrame(&m_state);
        for (auto &layer : m_sceneLayers) {
            layer->draw(ctx);
        }
        SceneWidgets::endFrame(&m_state);
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

    glm::vec2 Scene::getViewportMousePos(const glm::vec2 &mousePos) const {
        const auto &viewportPos = m_viewportTransform.pos;
        auto x = mousePos.x - viewportPos.x;
        auto y = mousePos.y - viewportPos.y;
        return {x, y};
    }

    glm::vec2 Scene::toScenePos(const glm::vec2 &mousePos) const {
        return m_camera->toWorldPos(mousePos);
    }

    void Scene::onMouseMove(const glm::vec2 &pos) {
        const auto viewportMousePos = getViewportMousePos(pos);
        const auto scenePos = m_camera->toWorldPos(viewportMousePos);

        SceneEvent::Data data;
        data.mouseMove = {
            .pos = scenePos,
            .delta = viewportMousePos - m_inputState.mousePos,
            .viewportPos = viewportMousePos,
        };
        SceneEvent evt{
            .type = SceneEvent::Type::mouseMove,
            .data = data,
            .isCtrlPressed = m_inputState.isCtrlPressed,
            .isShiftPressed = m_inputState.isShiftPressed,
            .isAltPressed = m_inputState.isAltPressed,
        };
        dispatchEvent(evt);
    }

    void Scene::onMiddleMouse(bool isPressed) {
        SceneEvent::Data data;
        data.mouseButton = {.button = MouseButton::middle,
                            .action = isPressed ? MouseButtonAction::press
                                                : MouseButtonAction::release,
                            .pos = toScenePos(m_inputState.mousePos)};
        SceneEvent evt{.type = SceneEvent::Type::mouseButton, .data = data};
        dispatchEvent(evt);
    }

    void Scene::onLeftMouse(bool isPressed) {
        SceneEvent::Data data;
        data.mouseButton = {.button = MouseButton::left,
                            .action = isPressed ? MouseButtonAction::press
                                                : MouseButtonAction::release,
                            .pos = toScenePos(m_inputState.mousePos)};
        SceneEvent evt{.type = SceneEvent::Type::mouseButton,
                       .data = data,
                       .isCtrlPressed = m_inputState.isCtrlPressed,
                       .isShiftPressed = m_inputState.isShiftPressed,
                       .isAltPressed = m_inputState.isAltPressed};
        dispatchEvent(evt);
    }

    const glm::vec2 &Scene::getCameraPos() const {
        return m_camera->getPos();
    }

    bool Scene::isDragging() const {
        return m_inputState.isDragging;
    }

    bool Scene::isLeftMousePressed() const {
        return m_inputState.isLeftMousePressed;
    }

    bool Scene::isMiddleMousePressed() const {
        return m_inputState.isMiddleMousePressed;
    }

    SceneCursor Scene::consumeCursorRequest() {
        const auto cursor = m_inputState.cursor;
        m_inputState.cursor = SceneCursor::inherit;
        return cursor;
    }

    const glm::vec2 &Scene::getMousePos() const {
        return m_inputState.mousePos;
    }

    glm::vec2 Scene::getSceneMousePos() {
        return toScenePos(m_inputState.mousePos);
    }

    float Scene::getCameraZoom() const {
        return m_camera->getZoom();
    }

    void Scene::setZoom(float value) const {
        m_camera->setZoom(value);
    }

    const glm::mat4 &Scene::getCameraTransform() const {
        return m_camera->getTransform();
    }

    float *Scene::getCameraTransformData() const {
        return const_cast<float *>(glm::value_ptr(m_camera->getTransform()));
    }

    void Scene::resizeCamera(const glm::vec2 &size) const {
        m_camera->resize(size.x, size.y);
    }

    void Scene::panCamera(const glm::vec2 &delta) const {
        m_camera->incrementPos(delta);
    }

    void Scene::focusCameraAt(const glm::vec2 &pos, bool smooth) const {
        m_camera->focusAtPoint(pos, smooth);
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

    bool Scene::isHoveredEntityValid() {
        return m_pickingId.isValid();
    }

    void Scene::setPickingId(const PickingId &value) {
        m_pickingId = value;
    }

    const PickingReadbackRequest &Scene::getPickingReadbackRequest() const {
        return m_inputState.pickingReadbackRequest;
    }

    bool Scene::dispatchEvent(SceneEvent &evt) {
        if (evt.type == SceneEvent::Type::mouseMove && m_camera) {
            const auto &data = evt.data.mouseMove;
            m_state.setMousePos(data.pos);
            m_inputState.dMousePos =
                data.pos - m_camera->toWorldPos(m_inputState.mousePos);
            m_inputState.mousePos = data.viewportPos;
        }

        evt.pickingId = m_pickingId;

        auto ctx = makeEventContext(
            m_state, m_camera, m_viewportTransform, m_inputState, m_pickingId);

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

    void Scene::applySelectionReadback(const std::vector<PickingId> &ids) {
        m_state.clearSelectedComponents();

        std::unordered_set<uint32_t> selectedRuntimeIds;
        for (const auto &id : ids) {
            if (!id.isValid() || !id.isSelectable()) {
                continue;
            }
            if (!selectedRuntimeIds.insert(id.runtimeId).second) {
                continue;
            }

            const auto comp = m_state.getComponentByPickingId(id);
            if (comp) {
                m_state.addSelectedComponent(comp->getUuid());
            }
        }

        clearPickingReadbackRequest();
    }

    void Scene::clearPickingReadbackRequest() {
        m_inputState.pickingReadbackRequest = {};
    }

    const SceneState &Scene::getState() const {
        return m_state;
    }

    SceneState &Scene::getState() {
        return m_state;
    }

    void Scene::updateViewportTransform(const ViewportTransform &transform) {
        m_viewportTransform = transform;
    }

    const ViewportTransform &Scene::getViewportTransform() const {
        return m_viewportTransform;
    }

    void Scene::focusCameraOnSelected() {

        const auto &selectedComps =
            m_state.getSelectedComponents() | std::ranges::views::keys;

        if (selectedComps.empty()) {
            return;
        }

        const auto &comp = m_state.getComponentByUuid(*selectedComps.begin());
        m_camera->focusAtPoint(comp->getAbsolutePosition(m_state));
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
