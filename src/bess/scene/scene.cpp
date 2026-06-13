#include "scene/scene.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "layers/components_layer.h"
#include "layers/grid_layer.h"
#include "layers/interaction_layer.h"
#include "layers/overlay_layer.h"
#include "scene/scene_events.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene_event.h"
#include "scene_layer.h"
#include "sub_systems/input_sub_system.h"
#include "sub_systems/renderer_context.h"
#include <cstdint>
#include <memory>
#include <ranges>
#include <unordered_set>
#include <utility>

namespace Bess::Canvas {
    std::vector<SceneEvent> formEvents(const std::shared_ptr<Camera> &camera,
                                       const ViewportTransform &vpTransform);

    Scene::Scene() {
        m_sceneLayers.push_back(std::make_unique<GridLayer>());
        m_sceneLayers.push_back(std::make_unique<ComponentsLayer>());
        m_sceneLayers.push_back(std::make_unique<OverlayLayer>());
        m_sceneLayers.push_back(std::make_unique<InteractionLayer>());
        reset();
    }

    Scene::~Scene() { destroy(); }

    void Scene::destroy() {
        if (m_isDestroyed)
            return;

        SceneContext ctx{
            .sceneState = &m_state,
            .camera = m_camera,
            .renderer = nullptr,
            .viewportTransform = &m_viewportTransform,
            .selBoxContext = &m_selBoxContext,
            .pickingReadbackRequest = &m_pickingReadbackRequest,
            .pickingId = &m_pickingId,
            .mousePos = &m_mousePos,
            .dMousePos = &m_dMousePos,
            .isLeftMousePressed = &m_isLeftMousePressed,
            .isMiddleMousePressed = &m_isMiddleMousePressed,
            .isDragging = &m_isDragging,
            .drawMode = &m_drawMode,
        };
        BESS_INFO("[Scene] Destroying {}", (uint64_t)m_state.getSceneId());
        for (auto &layer : m_sceneLayers) {
            layer->destroy(ctx);
        }

        m_sceneLayers.clear();
        m_isDestroyed = true;
        m_state.clear();
    }

    void Scene::reset() {
        clear();

        m_size = glm::vec2(800.f, 600.f);
        m_camera = std::make_shared<Camera>(m_size.x, m_size.y);

        m_mousePos = {0.f, 0.f};

        const auto &appCtx = Bess::GAppContext::getInstance();

        const auto &renderCtx = appCtx.getSubSystem<RendererContext>();
        SceneContext ctx{
            .sceneState = &m_state,
            .camera = m_camera,
            .renderer = renderCtx->getRenderer(),
            .viewportTransform = &m_viewportTransform,
            .selBoxContext = &m_selBoxContext,
            .pickingReadbackRequest = &m_pickingReadbackRequest,
            .pickingId = &m_pickingId,
            .mousePos = &m_mousePos,
            .dMousePos = &m_dMousePos,
            .isLeftMousePressed = &m_isLeftMousePressed,
            .isMiddleMousePressed = &m_isMiddleMousePressed,
            .isDragging = &m_isDragging,
            .drawMode = &m_drawMode,
        };

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
        m_drawMode = SceneDrawMode::none;
        m_selBoxContext = {};
        m_pickingReadbackRequest = {};
        m_pickingId = PickingId::invalid();
        m_prevPickingId = PickingId::invalid();
        m_isDragging = false;
        m_isLeftMousePressed = false;
        m_isMiddleMousePressed = false;
    }

    void Scene::processEvents() {
        const auto events = formEvents(m_camera, m_viewportTransform);
        for (auto evt : events) {
            dispatchEvent(evt);
        }
    }

    std::vector<SceneEvent> formEvents(const std::shared_ptr<Camera> &camera,
                                       const ViewportTransform &vpTransform) {
        std::vector<SceneEvent> events;

        auto &appCtx = Bess::GAppContext::getInstance();
        auto inputSystem = appCtx.getSubSystem<InputSubSystem>();

        bool isCtrlPressed = inputSystem->isCtrlPressed();
        bool isShiftPressed = inputSystem->isShiftPressed();
        bool isAltPressed = inputSystem->isAltPressed();

        const auto &frameInputState = inputSystem->getFrameInpState();

        auto toVpPos = [&vpTransform](const glm::vec2 &pos) -> glm::vec2 {
            auto x = pos.x - vpTransform.pos.x;
            auto y = pos.y - vpTransform.pos.y;
            return {x, y};
        };

        if (frameInputState.hasMouseMoved) {
            const auto &mouseMoveState = inputSystem->getMouseMoveState();

            SceneEvent::Data eventData;
            const auto viewportPos = toVpPos(mouseMoveState.pos);
            eventData.mouseMove = {
                .pos = camera->toWorldPos(viewportPos),
                .delta = mouseMoveState.delta,
                .viewportPos = viewportPos,
            };

            events.emplace_back(SceneEvent{
                .type = SceneEvent::Type::mouseMove,
                .data = eventData,
                .isCtrlPressed = isCtrlPressed,
                .isShiftPressed = isShiftPressed,
                .isAltPressed = isAltPressed,
            });
        }

        if (frameInputState.hasMouseWheelScrolled) {
            const auto &mouseWheelState = inputSystem->getMouseWheelState();
            SceneEvent::Data eventData;
            const auto viewportPos = toVpPos(mouseWheelState.pos);
            eventData.mouseWheel = {
                .pos = camera->toWorldPos(viewportPos),
                .delta = mouseWheelState.offset,
                .viewportPos = viewportPos,
            };

            events.emplace_back(SceneEvent{
                .type = SceneEvent::Type::mouseWheel,
                .data = eventData,
                .isCtrlPressed = isCtrlPressed,
                .isShiftPressed = isShiftPressed,
                .isAltPressed = isAltPressed,
            });
        }

        if (frameInputState.hasMouseBtnEvent) {
            const auto &mouseBtnState = frameInputState.mouseBtnState;
            SceneEvent::Data data;

            data.mouseButton = {
                .button = mouseBtnState.button,
                .action = mouseBtnState.action,
                .pos = camera->toWorldPos(toVpPos(mouseBtnState.pos)),
            };

            events.emplace_back(SceneEvent{
                .type = SceneEvent::Type::mouseButton,
                .data = data,
                .isCtrlPressed = isCtrlPressed,
                .isShiftPressed = isShiftPressed,
                .isAltPressed = isAltPressed,
            });
        };

        if (frameInputState.hasKeyEvent) {
            const auto &keyState = frameInputState.keyState;
            SceneEvent::Data data;
            data.keyPress = {
                .keycode = keyState.key,
                .action = keyState.action,
            };

            events.emplace_back(SceneEvent{
                .type = SceneEvent::Type::key,
                .data = data,
                .isCtrlPressed = isCtrlPressed,
                .isShiftPressed = isShiftPressed,
                .isAltPressed = isAltPressed,
            });
        }

        return events;
    }

    void Scene::update(TimeMs ts, bool isFocused) {
        m_frameTimeStep = ts;

        std::vector<SceneEvent> events;
        if (isFocused) {
            auto inputSystem =
                GAppContext::getInstance().getSubSystem<InputSubSystem>();
            m_isCtrlPressed = inputSystem->isCtrlPressed();
            m_isShiftPressed = inputSystem->isShiftPressed();
            events = formEvents(m_camera, m_viewportTransform);
        }

        m_camera->update(ts);

        SceneContext ctx{
            .sceneState = &m_state,
            .camera = m_camera,
            .renderer = nullptr,
            .viewportTransform = &m_viewportTransform,
            .selBoxContext = &m_selBoxContext,
            .pickingReadbackRequest = &m_pickingReadbackRequest,
            .pickingId = &m_pickingId,
            .mousePos = &m_mousePos,
            .dMousePos = &m_dMousePos,
            .isLeftMousePressed = &m_isLeftMousePressed,
            .isMiddleMousePressed = &m_isMiddleMousePressed,
            .isDragging = &m_isDragging,
            .drawMode = &m_drawMode,
        };

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
        SceneContext ctx{
            .sceneState = &m_state,
            .camera = m_camera,
            .renderer = renderer,
            .viewportTransform = &m_viewportTransform,
            .selBoxContext = &m_selBoxContext,
            .pickingReadbackRequest = &m_pickingReadbackRequest,
            .pickingId = &m_pickingId,
            .mousePos = &m_mousePos,
            .dMousePos = &m_dMousePos,
            .isLeftMousePressed = &m_isLeftMousePressed,
            .isMiddleMousePressed = &m_isMiddleMousePressed,
            .isDragging = &m_isDragging,
            .drawMode = &m_drawMode,
        };

        for (auto &layer : m_sceneLayers) {
            layer->draw(ctx);
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

    const glm::vec2 &Scene::getSize() const { return m_size; }

    void Scene::resize(const glm::vec2 &size) { m_size = size; }

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
            .delta = viewportMousePos - m_mousePos,
            .viewportPos = viewportMousePos,
        };
        SceneEvent evt{
            .type = SceneEvent::Type::mouseMove,
            .data = data,
            .isCtrlPressed = m_isCtrlPressed,
            .isShiftPressed = m_isShiftPressed,
        };
        dispatchEvent(evt);
    }

    void Scene::onRightMouse(bool isPressed) {
        SceneEvent::Data data;
        data.mouseButton = {.button = MouseButton::right,
                            .action = isPressed ? MouseButtonAction::press
                                                : MouseButtonAction::release,
                            .pos = toScenePos(m_mousePos)};
        SceneEvent evt{.type = SceneEvent::Type::mouseButton, .data = data};
        dispatchEvent(evt);
    }

    void Scene::onMiddleMouse(bool isPressed) {
        SceneEvent::Data data;
        data.mouseButton = {.button = MouseButton::middle,
                            .action = isPressed ? MouseButtonAction::press
                                                : MouseButtonAction::release,
                            .pos = toScenePos(m_mousePos)};
        SceneEvent evt{.type = SceneEvent::Type::mouseButton, .data = data};
        dispatchEvent(evt);
    }

    void Scene::onLeftMouse(bool isPressed) {
        SceneEvent::Data data;
        data.mouseButton = {.button = MouseButton::left,
                            .action = isPressed ? MouseButtonAction::press
                                                : MouseButtonAction::release,
                            .pos = toScenePos(m_mousePos)};
        SceneEvent evt{.type = SceneEvent::Type::mouseButton,
                       .data = data,
                       .isCtrlPressed = m_isCtrlPressed,
                       .isShiftPressed = m_isShiftPressed};
        dispatchEvent(evt);
    }

    const glm::vec2 &Scene::getCameraPos() const { return m_camera->getPos(); }

    bool Scene::isDragging() const { return m_isDragging; }

    const glm::vec2 &Scene::getMousePos() const { return m_mousePos; }

    glm::vec2 Scene::getSceneMousePos() { return toScenePos(m_mousePos); }

    float Scene::getCameraZoom() const { return m_camera->getZoom(); }

    void Scene::setZoom(float value) const { m_camera->setZoom(value); }

    float Scene::getNextZCoord() {
        const float z = m_compZCoord;
        m_compZCoord += m_zIncrement;
        return z;
    }

    void Scene::setZCoord(float value) { m_compZCoord = value + m_zIncrement; }

    void Scene::setSceneMode(SceneMode mode) { m_sceneMode = mode; }

    SceneMode Scene::getSceneMode() const { return m_sceneMode; }

    bool *Scene::getIsSchematicViewPtr() {
        return &m_state.getIsSchematicView();
    }

    void Scene::toggleSchematicView() {
        m_state.setIsSchematicView(!m_state.getIsSchematicView());
    }

    void Scene::onPrePickingIdChange(const PickingId &newId) {
        if (m_isDragging)
            return;

        // if (m_pickingId.isValid() && m_pickingId != newId) {
        //     const auto prevComp =
        //     m_state.getComponentByPickingId(m_pickingId); if (prevComp) {
        //         prevComp->onMouseLeave(
        //             {toScenePos(m_mousePos), m_pickingId.info});
        //     } else {
        //         BESS_WARN("[Scene] PickingId is valid but no component found
        //         "
        //                   "for id {}",
        //                   (uint64_t)m_pickingId);
        //     }
        // }
        //
        m_prevPickingId = m_pickingId;
    }

    void Scene::onPickingIdChange() {
        if (m_isDragging)
            return;

        // if (m_pickingId.isValid()) {
        //     const auto currComp =
        //     m_state.getComponentByPickingId(m_pickingId);
        //
        //     if (currComp) {
        //         currComp->onMouseEnter(
        //             {toScenePos(m_mousePos), m_pickingId.info});
        //     } else {
        //         BESS_WARN("[Scene] PickingId is valid but no component found
        //         "
        //                   "for id {}",
        //                   (uint64_t)m_pickingId);
        //     }
        // }
    }

    bool Scene::isHoveredEntityValid() { return m_pickingId.isValid(); }

    bool Scene::dispatchEvent(SceneEvent &evt) {
        evt.pickingId = m_pickingId;

        SceneContext ctx{
            .sceneState = &m_state,
            .camera = m_camera,
            .renderer = nullptr,
            .viewportTransform = &m_viewportTransform,
            .selBoxContext = &m_selBoxContext,
            .pickingReadbackRequest = &m_pickingReadbackRequest,
            .pickingId = &m_pickingId,
            .mousePos = &m_mousePos,
            .dMousePos = &m_dMousePos,
            .isLeftMousePressed = &m_isLeftMousePressed,
            .isMiddleMousePressed = &m_isMiddleMousePressed,
            .isDragging = &m_isDragging,
            .drawMode = &m_drawMode,
        };

        for (auto &layer : std::ranges::reverse_view(m_sceneLayers)) {
            if (layer->handleEvent(evt, ctx)) {
                return true;
            }
        }

        return evt.handled;
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

    void Scene::clearPickingReadbackRequest() { m_pickingReadbackRequest = {}; }

    const SceneState &Scene::getState() const { return m_state; }

    SceneState &Scene::getState() { return m_state; }

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

    const UUID &Scene::getSceneId() const { return m_state.getSceneId(); }

} // namespace Bess::Canvas
