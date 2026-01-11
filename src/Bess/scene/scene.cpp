#include "scene/scene.h"
#include "bess_uuid.h"
#include "commands/commands.h"
#include "common/log.h"
#include "component_catalog.h"
#include "event_dispatcher.h"
#include "events/application_event.h"
#include "events/scene_events.h"
#include "events/sim_engine_events.h"
#include "fwd.hpp"
#include "pages/main_page/main_page_state.h"
#include "plugin_manager.h"
#include "scene/commands/add_command.h"
#include "scene/components/non_sim_comp.h"
#include "scene/renderer/material_renderer.h"
#include "scene/scene_state/components/connection_scene_component.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/sim_scene_component.h"
#include "scene/scene_state/components/slot_scene_component.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include "ui/ui.h"
#include "ui/ui_main/ui_main.h"
#include "vulkan_core.h"
#include <cstdint>
#include <memory>
#include <ranges>
#include <utility>

namespace Bess::Canvas {
    Scene::Scene() {
        reset();

        auto &dispatcher = EventSystem::EventDispatcher::instance();
        dispatcher.sink<Events::SlotClickedEvent>().connect<&Scene::onSlotClicked>(this);
        dispatcher.sink<SimEngine::Events::CompDefOutputsResizedEvent>().connect<&Scene::onCompDefOutputsResized>(this);
    }

    Scene::~Scene() {
        destroy();
    }

    void Scene::destroy() {
        if (m_isDestroyed)
            return;

        BESS_INFO("[Scene] Destroying");
        m_copiedComponents.clear();
        m_viewport.reset();
        m_lastCreatedComp = {};
        m_cmdManager.clearStacks();
        m_isDestroyed = true;
        m_state.clear();
    }

    std::shared_ptr<Scene> Scene::instance() {
        static std::shared_ptr<Scene> m_instance = std::make_shared<Scene>();
        return m_instance;
    }

    void Scene::reset() {
        clear();

        m_size = glm::vec2(800.f, 600.f);

        auto &vkCore = Vulkan::VulkanCore::instance();
        m_viewport = std::make_shared<Viewport>(vkCore.getDevice(),
                                                vkCore.getSwapchain()->imageFormat(),
                                                vec2Extent2D(m_size));

        m_camera = m_viewport->getCamera();
        m_mousePos = {0.f, 0.f};
    }

    void Scene::clear() {
        m_state.clear();
        m_compZCoord = 1.f + m_zIncrement;
        m_lastCreatedComp = {};
        m_copiedComponents = {};
        m_drawMode = SceneDrawMode::none;
    }

    void Scene::update(TFrameTime ts, const std::vector<ApplicationEvent> &events) {
        m_frameTimeStep = ts;
        if (m_getIdsInSelBox) {
            if (selectEntitesInArea())
                m_getIdsInSelBox = false;
        }

        m_camera->update(ts);

        for (const auto &event : events) {
            switch (event.getType()) {
            case ApplicationEventType::MouseMove: {
                const auto data = event.getData<ApplicationEvent::MouseMoveData>();
                auto pos = getViewportMousePos(glm::vec2(data.x, data.y));
                if (!isCursorInViewport(pos)) {
                    m_isLeftMousePressed = false;
                    m_mousePos = pos;
                    break;
                }
                onMouseMove(pos);
            } break;
            case ApplicationEventType::MouseButton: {
                if (!isCursorInViewport(m_mousePos)) {
                    if (!m_isLeftMousePressed) {
                        setPickingId(PickingId::invalid());
                        break;
                    }
                    m_isLeftMousePressed = false;
                    setPickingId(PickingId::invalid());
                }
                m_viewport->waitForPickingResults(5'000'000);
                updatePickingId();
                const auto data = event.getData<ApplicationEvent::MouseButtonData>();
                if (data.button == MouseButton::left) {
                    onLeftMouse(data.pressed);
                } else if (data.button == MouseButton::right) {
                    onRightMouse(data.pressed);
                } else if (data.button == MouseButton::middle) {
                    onMiddleMouse(data.pressed);
                }
            } break;
            case ApplicationEventType::MouseWheel: {
                const auto data = event.getData<ApplicationEvent::MouseWheelData>();
                onMouseWheel(data.x, data.y);
            } break;
            case ApplicationEventType::KeyPress: {
                handleKeyboardShortcuts();
            } break;
            default:
                break;
            }
        }

        updateNetsFromSimEngine();
    }

    void Scene::updateNetsFromSimEngine() {
        auto &simEngine = SimEngine::SimulationEngine::instance();
        if (!simEngine.isNetUpdated()) {
            return;
        }

        const auto &nets = simEngine.getNetsMap();

        std::unordered_map<UUID, UUID> cache;

        for (const auto &id : m_state.getComponentsByType(SceneComponentType::simulation)) {
            const auto &simComp = m_state.getComponentByUuid<SimulationSceneComponent>(id);
            cache[simComp->getSimEngineId()] = id;
        }

        for (const auto &netInfo : nets) {
            const auto &netId = netInfo.first;
            const auto &componentsInNet = netInfo.second.getComponents();

            for (const auto &simEngineUuid : componentsInNet) {
                if (!cache.contains(simEngineUuid)) {
                    return;
                }

                const auto &simComp = m_state.getComponentByUuid<SimulationSceneComponent>(cache[simEngineUuid]);

                simComp->setNetId(netId);
            }
        }

        BESS_INFO("[Scene] Updated nets from Simulation Engine, total nets = {}", nets.size());
    }

    void Scene::selectAllEntities() {
        m_state.clearSelectedComponents();
        const auto &comps = m_state.getRootComponents();

        for (auto &id : comps) {
            m_state.addSelectedComponent(id);
        }
    }

    void Scene::renderWithViewport(const std::shared_ptr<Viewport> &viewport) {
        if (m_sceneMode == SceneMode::move) {
            UI::setCursorMove();
        }

        auto &inst = Bess::Vulkan::VulkanCore::instance();

        viewport->begin((int)inst.getCurrentFrameIdx(),
                        ViewportTheme::colors.background,
                        {0, PickingId::invalid().runtimeId});

        drawSceneToViewport(viewport);

        viewport->end();
        viewport->submit();
    }

    void Scene::render() {
        renderWithViewport(m_viewport);
    }

    void Scene::drawSceneToViewport(const std::shared_ptr<Viewport> &viewport) {
        // check if start frame does not happen again within same frame
        //
        {
            auto mousePos_ = m_mousePos;
            const auto &viewportSize = UI::UIMain::state.mainViewport.getViewportSize();
            mousePos_.y = viewportSize.y - mousePos_.y;
            int x = static_cast<int>(mousePos_.x);
            int y = static_cast<int>(mousePos_.y);
            if (m_selectInSelectionBox) {
                auto start = m_selectionBoxStart;
                auto end = m_selectionBoxEnd;
                auto size = end - start;
                const glm::vec2 pos = {std::min(start.x, end.x), std::max(start.y, end.y)};
                size = glm::abs(size);
                int w = (int)size.x;
                int h = (int)size.y;
                int x = (int)pos.x;
                int y = (int)(viewportSize.y - pos.y);
                m_viewport->setPickingCoord(x, y, w, h);
                m_selectInSelectionBox = false;
                m_getIdsInSelBox = true;
            } else {
                m_viewport->setPickingCoord(x, y);
            }
        }

        const auto &renderers = viewport->getRenderers();

        // Grid
        renderers.materialRenderer->drawGrid(
            glm::vec3(0.f, 0.f, 0.1f),
            m_camera->getSpan(),
            PickingId::invalid(),
            {
                .minorColor = ViewportTheme::colors.gridMinorColor,
                .majorColor = ViewportTheme::colors.gridMajorColor,
                .axisXColor = ViewportTheme::colors.gridAxisXColor,
                .axisYColor = ViewportTheme::colors.gridAxisYColor,
            },
            viewport->getCamera());

        if (m_connectionStartSlot != UUID::null) {
            const auto comp = m_state.getComponentByUuid<SlotSceneComponent>(m_connectionStartSlot);
            const auto &pos = m_state.getIsSchematicView()
                                  ? comp->getSchematicConnStartPos(m_state)
                                  : comp->getAbsolutePosition(m_state);
            const auto endPos = toScenePos(m_mousePos);

            drawGhostConnection(renderers.pathRenderer, glm::vec2(pos), endPos);
        }

        const auto &cam = viewport->getCamera();
        const auto &span = (cam->getSpan() / 2.f) + 200.f;
        const auto &camPos = cam->getPos();

        for (const auto &compId : m_state.getRootComponents()) {
            const auto comp = m_state.getComponentByUuid(compId);

            const auto &transform = comp->getTransform();
            const auto x = transform.position.x - camPos.x;
            const auto y = transform.position.y - camPos.y;

            // skipping if outside camera
            if (x < -span.x || x > span.x || y < -span.y || y > span.y)
                continue;

            if (m_state.getIsSchematicView()) {
                comp->drawSchematic(m_state,
                                    renderers.materialRenderer,
                                    renderers.pathRenderer);
            } else {
                comp->draw(m_state,
                           renderers.materialRenderer,
                           renderers.pathRenderer);
            }
        }

        if (m_drawMode == SceneDrawMode::selectionBox) {
            drawSelectionBox();
        }

        drawScratchContent(m_frameTimeStep, viewport);
    }

    void Scene::drawSelectionBox() {
        const auto start = toScenePos(m_selectionBoxStart);
        const auto end = toScenePos(m_mousePos);

        auto size = end - start;
        const auto pos = start + size / 2.f;
        size = glm::abs(size);

        Renderer::QuadRenderProperties props;
        props.borderColor = ViewportTheme::colors.selectionBoxBorder;
        props.borderSize = glm::vec4(1.f);

        auto renderer = m_viewport->getRenderers().materialRenderer;
        renderer->drawQuad(glm::vec3(pos, 7.f), size, ViewportTheme::colors.selectionBoxFill, -1, props);
    }

    UUID Scene::createSimEntity(const UUID &simEngineId,
                                const std::shared_ptr<SimEngine::ComponentDefinition> &def,
                                const glm::vec2 &pos) {
        auto sceneComp = SimulationSceneComponent::createNewAndRegister(m_state, simEngineId);
        sceneComp->setPosition(glm::vec3(getSnappedPos(pos), getNextZCoord()));

        const auto &pluginManger = Plugins::PluginManager::getInstance();
        if (pluginManger.hasSceneComponentType(def->getHash())) {
            auto hook = pluginManger.createSceneComponentInstance(def->getHash());
            sceneComp->setDrawHook(hook);
        }

        m_lastCreatedComp = {.componentDefinition = def, .set = true};
        return sceneComp->getUuid();
    }

    UUID Scene::createNonSimEntity(const Canvas::Components::NSComponent &comp, const glm::vec2 &pos) {
        return UUID::null;
        // using namespace Canvas::Components;
        // auto entity = m_registry.create();
        // const auto &idComp = m_registry.emplace<Components::IdComponent>(entity);
        // auto &nonSimComp = m_registry.emplace<Components::NSComponent>(entity);
        // auto &tag = m_registry.emplace<Components::TagComponent>(entity);
        // auto &transformComp = m_registry.emplace<Components::TransformComponent>(entity);
        //
        // tag.name = comp.name;
        // tag.type.nsCompType = comp.type;
        // nonSimComp.type = comp.type;
        //
        // transformComp.position = glm::vec3(pos, getNextZCoord());
        // transformComp.scale = glm::vec2(0.f, 0.f);
        //
        // switch (comp.type) {
        // case Components::NSComponentType::text: {
        //     auto &textComp = m_registry.emplace<Components::TextNodeComponent>(entity);
        //     textComp.text = "New Text";
        //     textComp.fontSize = 20.f;
        //     textComp.color = ViewportTheme::colors.text;
        // } break;
        // default:
        //     break;
        // }
        // BESS_INFO("[Scene] Created Non simulation entity {}", (uint64_t)entity);
        // setLastCreatedComp({.nsComponent = comp});
        // Events::EventDispatcher::instance().trigger(Events::ComponentCreatedEvent{idComp.uuid, false});
        // return idComp.uuid;
    }

    void Scene::deleteSceneEntity(const UUID &entUuid) {
        const auto ids = m_state.removeComponent(entUuid);
        BESS_INFO("[Scene] Deleted entity {}", (uint64_t)entUuid);
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
        m_size = UI::UIMain::state.mainViewport.getViewportSize();
        m_viewport->resize(vec2Extent2D(m_size));
        m_camera->resize(m_size.x, m_size.y);
    }

    glm::vec2 Scene::getViewportMousePos(const glm::vec2 &mousePos) const {
        const auto &viewportPos = UI::UIMain::state.mainViewport.getViewportPos();
        auto x = mousePos.x - viewportPos.x;
        auto y = mousePos.y - viewportPos.y;
        return {x, y};
    }

    glm::vec2 Scene::toScenePos(const glm::vec2 &mousePos) const {
        glm::vec2 pos = mousePos;

        const auto cameraPos = m_camera->getPos();
        glm::mat4 tansform = glm::translate(glm::mat4(1.f), glm::vec3(cameraPos.x, cameraPos.y, 0.f));
        const auto zoom = m_camera->getZoom();
        tansform = glm::scale(tansform, glm::vec3(1.f / zoom, 1.f / zoom, 1.f));

        pos = glm::vec2(tansform * glm::vec4(pos.x, pos.y, 0.f, 1.f));
        auto span = m_camera->getSpan() / 2.f;
        pos -= glm::vec2({span.x, span.y});
        return pos;
    }

    uint64_t decodeGpuHoverValue(const glm::uvec2 &encodedId) {
        uint64_t id = static_cast<uint64_t>(encodedId.x);
        id |= (static_cast<uint64_t>(encodedId.y) << 32);
        return id;
    }

    void Scene::updatePickingId() {
        m_viewport->tryUpdatePickingResults();

        const auto &ids = m_viewport->getPickingIdsResult();
        const uint64_t hoverValue = (ids.empty())
                                        ? PickingId::invalid()
                                        : decodeGpuHoverValue(ids[0]);

        setPickingId(PickingId::fromUint64(hoverValue));
    }

    void Scene::onMouseMove(const glm::vec2 &pos) {
        m_dMousePos = toScenePos(pos) - toScenePos(m_mousePos);
        m_mousePos = pos;
        {
            auto mousePos_ = m_mousePos;
            const auto &viewportSize = UI::UIMain::state.mainViewport.getViewportSize();
            mousePos_.y = viewportSize.y - mousePos_.y;
            int x = static_cast<int>(mousePos_.x);
            int y = static_cast<int>(mousePos_.y);
            m_viewport->setPickingCoord(x, y);
        }

        if (!m_isLeftMousePressed) {
            if (m_viewport->waitForPickingResults(1000000)) {
                updatePickingId();
            }

            // dispatch hover event
            if (m_pickingId.isValid() && m_pickingId == m_prevPickingId) {
                auto comp = m_state.getComponentByPickingId(m_pickingId);
                if (comp) {
                    comp->onMouseHovered({toScenePos(m_mousePos), m_pickingId.info});
                } else {
                    BESS_WARN("Picking id was valid but comp not found, {}", m_pickingId.runtimeId);
                }
            }
        }

        if (m_isLeftMousePressed && m_drawMode == SceneDrawMode::none) {

            if (m_pickingId.isValid()) {
                const auto &selectedComps = m_state.getSelectedComponents();
                for (const auto &compId : selectedComps | std::ranges::views::keys) {
                    std::shared_ptr<SceneComponent> comp = m_state.getComponentByUuid(compId);
                    if (comp && comp->isDraggable()) {
                        comp->cast<SimulationSceneComponent>()->onMouseDragged({toScenePos(m_mousePos),
                                                                                m_dMousePos,
                                                                                m_pickingId.info,
                                                                                selectedComps.size() > 1});
                        m_isDragging = true;
                    }
                }
            } else {
                m_drawMode = SceneDrawMode::selectionBox;
                m_selectionBoxStart = m_mousePos;
            }
        } else if (m_isMiddleMousePressed) {
            m_camera->incrementPos(-m_dMousePos);
        }
    }

    bool Scene::isCursorInViewport(const glm::vec2 &pos) const {
        const auto &viewportSize = UI::UIMain::state.mainViewport.getViewportSize();
        const auto viewportPos = UI::UIMain::state.mainViewport.getViewportPos();
        return pos.x >= 1.f &&
               pos.x < viewportSize.x - 1.f &&
               pos.y >= 1.f &&
               pos.y < viewportSize.y - 1.f;
    }

    void Scene::onRightMouse(bool isPressed) {
        if (!isPressed) {
            return;
        }

        if (!m_pickingId.isValid()) {
            const bool isShiftPressed = Pages::MainPageState::getInstance()->isKeyPressed(GLFW_KEY_LEFT_SHIFT) ||
                                        Pages::MainPageState::getInstance()->isKeyPressed(GLFW_KEY_RIGHT_SHIFT);

            if (isShiftPressed) {
                const auto def = m_lastCreatedComp.componentDefinition;
                const Commands::AddCommandData cmdData = {
                    .def = def,
                    .nsComp = m_lastCreatedComp.nsComponent,
                    .inputCount = def->getInputSlotsInfo().count,
                    .outputCount = def->getOutputSlotsInfo().count,
                    .pos = getSnappedPos(toScenePos(m_mousePos)),
                };

                const auto res = m_cmdManager.execute<Commands::AddCommand, std::vector<UUID>>(std::vector{cmdData});
                if (!res.has_value()) {
                    BESS_ERROR("[Scene][OnRightMouse] Failed to execute AddCommand");
                }
            }
        }
    }

    void Scene::onMiddleMouse(bool isPressed) {
        m_isMiddleMousePressed = isPressed;
    }

    void Scene::onLeftMouse(bool isPressed) {
        m_isLeftMousePressed = isPressed;
        if (!isPressed) {

            // if left ctrl is not pressed and multiple entities are selected,
            // then we only deselect othere on mouse release,
            // so that drag can work properly
            size_t selSize = m_state.getSelectedComponents().size();
            bool isCtrlPressed = Pages::MainPageState::getInstance()->isKeyPressed(
                GLFW_KEY_LEFT_CONTROL);
            if (selSize > 1 && !m_isDragging &&
                !isCtrlPressed && m_state.isComponentSelected(m_pickingId)) {
                m_state.clearSelectedComponents();
                m_state.addSelectedComponent(m_pickingId);
            }

            if (m_drawMode == SceneDrawMode::selectionBox) {
                m_drawMode = SceneDrawMode::none;
                m_selectInSelectionBox = true;
                m_selectionBoxEnd = m_mousePos;
            } else if (m_isDragging) {
                m_isDragging = false;
                for (const auto &compId : m_state.getSelectedComponents() | std::ranges::views::keys) {
                    std::shared_ptr<SceneComponent> comp = m_state.getComponentByUuid(compId);
                    if (comp && comp->isDraggable()) {
                        comp->cast<SimulationSceneComponent>()->onMouseDragEnd();
                    }
                }
            } else {
                if (m_pickingId.isValid()) {
                    auto comp = m_state.getComponentByPickingId(m_pickingId);
                    comp->onMouseButton({toScenePos(m_mousePos),
                                         Events::MouseButton::left,
                                         Events::MouseClickAction::release,
                                         m_pickingId.info,
                                         &m_state});
                }
            }

            return;
        }

        if (m_pickingId.isValid()) {
            auto comp = m_state.getComponentByPickingId(m_pickingId);
            comp->onMouseButton({toScenePos(m_mousePos),
                                 Events::MouseButton::left,
                                 Events::MouseClickAction::press,
                                 m_pickingId.info,
                                 &m_state});

            const bool isCtrlPressed = Pages::MainPageState::getInstance()->isKeyPressed(GLFW_KEY_LEFT_CONTROL);
            if (isCtrlPressed) {
                if (m_state.isComponentSelected(m_pickingId))
                    m_state.removeSelectedComponent(m_pickingId);
                else
                    m_state.addSelectedComponent(m_pickingId);
            } else {
                size_t selSize = m_state.getSelectedComponents().size();
                if (selSize < 2 || !comp->isSelected()) {
                    m_state.clearSelectedComponents();
                    m_state.addSelectedComponent(m_pickingId);
                }
            }
        } else {
            m_state.clearSelectedComponents();
            m_connectionStartSlot = UUID::null;
            m_drawMode = SceneDrawMode::none;
        }
    }

    void Scene::copySelectedComponents() {
        m_copiedComponents.clear();

        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto &catalogInstance = SimEngine::ComponentCatalog::instance();

        const auto &selComponents = m_state.getSelectedComponents() | std::views::keys;

        for (const auto &selId : selComponents) {
            const auto comp = m_state.getComponentByUuid(selId);
            const bool isSimComp = comp->getType() == SceneComponentType::simulation;
            const bool isNonSimComp = comp->getType() == SceneComponentType::nonSimulation;
            if (isSimComp) {
                const auto casted = comp->cast<SimulationSceneComponent>();
                CopiedComponent compData{};
                compData.def = simEngine.getComponentDefinition(casted->getSimEngineId());
                compData.inputCount = (int)casted->getInputSlotsCount();
                compData.outputCount = (int)casted->getOutputSlotsCount();
                m_copiedComponents.emplace_back(compData);
            } else if (isNonSimComp) {
                // const auto nsCompView = m_registry.view<Components::SelectedComponent, Components::NSComponent>();
                // for (const auto entt : nsCompView) {
                //     const auto &comp = nsCompView.get<Components::NSComponent>(entt);
                //     CopiedComponent compData{};
                //     compData.nsComp = comp;
                //     m_copiedComponents.emplace_back(compData);
                // }
            }
        }
    }

    void Scene::generateCopiedComponents() {
        auto pos = getCameraPos();

        std::vector<Commands::AddCommandData> cmdData = {};
        Commands::AddCommandData data;

        for (auto &comp : m_copiedComponents) {
            data = {};

            if (comp.nsComp.type == Canvas::Components::NSComponentType::EMPTY) {
                data.def = comp.def;
                data.inputCount = comp.inputCount;
                data.outputCount = comp.outputCount;
            } else {
                data.nsComp = comp.nsComp;
            }

            data.pos = pos;
            cmdData.emplace_back(data);
            pos += glm::vec2(50.f, 50.f);
        }

        const auto res = m_cmdManager.execute<Canvas::Commands::AddCommand,
                                              std::vector<UUID>>(std::vector{cmdData});

        if (!res.has_value()) {
            BESS_ERROR("Failed to execute AddCommand");
        }
    }

    bool Scene::selectEntitesInArea() {
        if (!m_viewport->tryUpdatePickingResults())
            return false;

        const std::vector<glm::uvec2> rawIds = m_viewport->getPickingIdsResult();

        if (rawIds.size() == 0)
            return false;

        std::set<PickingId> ids;
        for (const auto &id : rawIds) {
            ids.insert(PickingId::fromUint64(decodeGpuHoverValue(id)));
        }

        m_state.clearSelectedComponents();
        for (const auto &id : ids) {
            auto comp = m_state.getComponentByPickingId(id);
            if (comp == nullptr)
                continue;
            m_state.addSelectedComponent(id);
        }

        return true;
    }

    void Scene::onMouseWheel(double x, double y) {
        if (!isCursorInViewport(m_mousePos))
            return;

        const auto mainPageState = Pages::MainPageState::getInstance();
        if (mainPageState->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
            const float delta = static_cast<float>(y) * 0.1f;
            m_camera->incrementZoomToPoint(toScenePos(m_mousePos), delta);
        } else {
            glm::vec2 dPos = {x, y};
            dPos *= 10 / m_camera->getZoom() * -1;
            m_camera->incrementPos(dPos);
        }
    }

    void Scene::saveScenePNG(const std::string &path) const {
        // TODO: Implement Vulkan scene save to PNG
        // This will require reading from Vulkan framebuffer and saving to file
        BESS_WARN("Scene save to PNG not yet implemented for Vulkan");
    }

    const glm::vec2 &Scene::getCameraPos() const {
        return m_camera->getPos();
    }

    const glm::vec2 &Scene::getMousePos() const {
        return m_mousePos;
    }

    glm::vec2 Scene::getSceneMousePos() {
        return toScenePos(m_mousePos);
    }

    float Scene::getCameraZoom() const {
        return m_camera->getZoom();
    }

    void Scene::setZoom(float value) const {
        m_camera->setZoom(value);
    }

    float Scene::getNextZCoord() {
        const float z = m_compZCoord;
        m_compZCoord += m_zIncrement;
        return z;
    }

    void Scene::setZCoord(float value) {
        m_compZCoord = value + m_zIncrement;
    }

    SimEngine::Commands::CommandsManager &Scene::getCmdManager() {
        return m_cmdManager;
    }

    uint64_t Scene::getTextureId() const {
        return m_viewport->getViewportTexture();
    }

    std::shared_ptr<Camera> Scene::getCamera() {
        return m_camera;
    }

    void Scene::setSceneMode(SceneMode mode) {
        m_sceneMode = mode;
    }

    SceneMode Scene::getSceneMode() const {
        return m_sceneMode;
    }

    bool *Scene::getIsSchematicViewPtr() {
        return &m_state.getIsSchematicView();
    }

    void Scene::toggleSchematicView() {
        m_state.setIsSchematicView(!m_state.getIsSchematicView());
    }

    VkExtent2D Scene::vec2Extent2D(const glm::vec2 &vec) {
        return {(uint32_t)vec.x, (uint32_t)vec.y};
    }

    glm::vec2 Scene::getSnappedPos(const glm::vec2 &pos) const {
        constexpr float snapSize = 5.f;
        return glm::vec2(glm::round(pos / snapSize)) * snapSize;
    }

    void Scene::drawScratchContent(TFrameTime ts, const std::shared_ptr<Viewport> &viewport) {
    }

    bool Scene::isHoveredEntityValid() {
        return m_pickingId.isValid();
    }

    void Scene::onSlotClicked(const Events::SlotClickedEvent &e) {
        if (e.action != Events::MouseClickAction::press)
            return;

        auto startSlot = m_state.getComponentByUuid<SlotSceneComponent>(m_connectionStartSlot);
        auto endSlot = m_state.getComponentByUuid<SlotSceneComponent>(e.slotUuid);

        if (m_connectionStartSlot == UUID::null) {
            m_connectionStartSlot = e.slotUuid;
        } else {
            auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();

            if (startSlot->getSlotType() == SlotType::inputsResize ||
                startSlot->getSlotType() == SlotType::outputsResize) {
                const auto startParent = m_state.getComponentByUuid<SimulationSceneComponent>(startSlot->getParentComponent());
                const auto &simEngineId = startParent->getSimEngineId();
                const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(simEngineId);

                std::shared_ptr<SlotSceneComponent> newSlot = std::make_shared<SlotSceneComponent>();

                if (startSlot->getSlotType() == SlotType::inputsResize) {
                    const auto newSize = digitalComp->incrementInputCount();
                    // we check against -1 because the resize slot is also there
                    if (newSize == startParent->getInputSlotsCount() - 1) {
                        BESS_WARN("[Scene] Failed to resize input slots for component {}", (uint64_t)startParent->getUuid());
                        m_connectionStartSlot = UUID::null;
                        return;
                    }

                    newSlot->setSlotType(SlotType::digitalInput);
                    newSlot->setIndex((int)newSize - 1);
                    startParent->addInputSlot(newSlot->getUuid());
                } else {
                    const auto newSize = digitalComp->incrementOutputCount();
                    // we check against -1 because the resize slot is also there
                    if (newSize == startParent->getOutputSlotsCount() - 1) {
                        BESS_WARN("[Scene] Failed to resize output slots for component {}, returnedNewSize = {}",
                                  (uint64_t)startParent->getUuid(),
                                  newSize);
                        m_connectionStartSlot = UUID::null;
                        return;
                    }

                    newSlot->setSlotType(SlotType::digitalOutput);
                    newSlot->setIndex((int)newSize - 1);
                    startParent->addOutputSlot(newSlot->getUuid());
                }

                m_state.addComponent<SlotSceneComponent>(newSlot);
                m_state.attachChild(startParent->getUuid(), newSlot->getUuid());

                startParent->setScaleDirty();
                startSlot = newSlot;

                BESS_INFO("[Scene] Resized component {}, new slot index = {}, new total slots = {}",
                          (uint64_t)startParent->getUuid(),
                          newSlot->getIndex(),
                          startSlot->getSlotType() == SlotType::digitalInput
                              ? startParent->getInputSlotsCount()
                              : startParent->getOutputSlotsCount());
            }

            if (endSlot->getSlotType() == SlotType::inputsResize ||
                endSlot->getSlotType() == SlotType::outputsResize) {
                const auto endParent = m_state.getComponentByUuid<SimulationSceneComponent>(endSlot->getParentComponent());

                const auto &simEngineId = endParent->getSimEngineId();

                const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(simEngineId);

                std::shared_ptr<SlotSceneComponent> newSlot = std::make_shared<SlotSceneComponent>();

                if (endSlot->getSlotType() == SlotType::inputsResize) {
                    const auto newSize = digitalComp->incrementInputCount();
                    // we check against -1 because the resize slot is also there
                    if (newSize == endParent->getInputSlotsCount() - 1) {
                        BESS_WARN("[Scene] Failed to resize input slots for component {}", (uint64_t)endParent->getUuid());
                        m_connectionStartSlot = UUID::null;
                        return;
                    }

                    newSlot->setSlotType(SlotType::digitalInput);
                    newSlot->setIndex((int)newSize - 1);
                    endParent->addInputSlot(newSlot->getUuid());
                } else {
                    const auto newSize = digitalComp->incrementOutputCount();
                    // we check against -1 because the resize slot is also there
                    if (newSize == endParent->getOutputSlotsCount() - 1) {
                        BESS_WARN("[Scene] Failed to resize output slots for component {}", (uint64_t)endParent->getUuid());
                        m_connectionStartSlot = UUID::null;
                        return;
                    }

                    newSlot->setSlotType(SlotType::digitalOutput);
                    newSlot->setIndex((int)newSize - 1);
                    endParent->addOutputSlot(newSlot->getUuid());
                }

                m_state.addComponent<SlotSceneComponent>(newSlot);
                m_state.attachChild(endParent->getUuid(), newSlot->getUuid());

                endParent->setScaleDirty();
                endSlot = newSlot;

                BESS_INFO("[Scene] Resized component {}, new slot index = {}, new total slots = {}",
                          (uint64_t)endParent->getUuid(),
                          newSlot->getIndex(),
                          endSlot->getSlotType() == SlotType::digitalInput
                              ? endParent->getInputSlotsCount()
                              : endParent->getOutputSlotsCount());
            }

            const auto startParent = m_state.getComponentByUuid<SimulationSceneComponent>(startSlot->getParentComponent());
            const auto endParent = m_state.getComponentByUuid<SimulationSceneComponent>(endSlot->getParentComponent());

            const auto startPinType = startSlot->getSlotType() == SlotType::digitalInput
                                          ? SimEngine::SlotType::digitalInput
                                          : SimEngine::SlotType::digitalOutput;

            const auto endPinType = endSlot->getSlotType() == SlotType::digitalInput
                                        ? SimEngine::SlotType::digitalInput
                                        : SimEngine::SlotType::digitalOutput;

            const auto res = cmdMngr.execute<SimEngine::Commands::ConnectCommand,
                                             std::string>(startParent->getSimEngineId(), startSlot->getIndex(), startPinType,
                                                          endParent->getSimEngineId(), endSlot->getIndex(), endPinType);

            if (!res.has_value()) {
                BESS_WARN("[Scene] Failed to create connection between slots, {}", res.error());
                m_connectionStartSlot = UUID::null;
                return;
            }

            auto conn = std::make_shared<ConnectionSceneComponent>();
            m_state.addComponent<ConnectionSceneComponent>(conn);

            conn->setStartEndSlots(startSlot->getUuid(), endSlot->getUuid());
            startSlot->addConnection(conn->getUuid());
            endSlot->addConnection(conn->getUuid());

            BESS_INFO("[Scene] Created connection between slots {} and {}",
                      (uint64_t)startSlot->getUuid(),
                      (uint64_t)endSlot->getUuid());

            m_connectionStartSlot = UUID::null;
        }
    }

    void Scene::drawGhostConnection(const std::shared_ptr<PathRenderer> &pathRenderer,
                                    const glm::vec2 &startPos,
                                    const glm::vec2 &endPos) {
        auto midX = (startPos.x + endPos.x) / 2.f;

        pathRenderer->beginPathMode(glm::vec3(startPos.x, startPos.y, 0.8f),
                                    2.f,
                                    ViewportTheme::colors.ghostWire,
                                    PickingId::invalid());

        pathRenderer->pathLineTo(glm::vec3(midX, startPos.y, 0.8f),
                                 2.f,
                                 ViewportTheme::colors.ghostWire,
                                 PickingId::invalid());

        pathRenderer->pathLineTo(glm::vec3(midX, endPos.y, 0.8f),
                                 2.f,
                                 ViewportTheme::colors.ghostWire,
                                 PickingId::invalid());

        pathRenderer->pathLineTo(glm::vec3(endPos, 0.8f),
                                 2.f,
                                 ViewportTheme::colors.ghostWire,
                                 PickingId::invalid());

        pathRenderer->endPathMode(false, false, glm::vec4(1.f), true, true);
    }

    const SceneState &Scene::getState() const {
        return m_state;
    }

    SceneState &Scene::getState() {
        return m_state;
    }

    void Scene::setPickingId(const PickingId &pickingId) {
        m_prevPickingId = m_pickingId;
        m_pickingId = pickingId;

        if (m_pickingId != m_prevPickingId) {
            if (m_prevPickingId.isValid()) {
                const auto prevComp = m_state.getComponentByPickingId(m_prevPickingId);
                if (prevComp) {
                    prevComp->onMouseLeave({toScenePos(m_mousePos), m_prevPickingId.info});
                } else {
                    BESS_WARN("[Scene] Previous PickingId is valid but no component found for id {}",
                              (uint64_t)m_prevPickingId);
                }
            }

            if (m_pickingId.isValid()) {
                const auto currComp = m_state.getComponentByPickingId(m_pickingId);

                if (currComp) {
                    currComp->onMouseEnter({toScenePos(m_mousePos), m_pickingId.info});
                } else {
                    BESS_WARN("[Scene] PickingId is valid but no component found for id {}",
                              (uint64_t)m_pickingId);
                }
            }
        }
    }

    void Scene::onCompDefOutputsResized(const SimEngine::Events::CompDefOutputsResizedEvent &e) {
        const auto &parent = m_state.getComponentBySimEngineId<SimulationSceneComponent>(e.componentId);
        BESS_ASSERT(parent, std::format("[Scene] Component with simEngineId {} not found in scene state",
                                        (uint64_t)e.componentId));
        const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(e.componentId);
        const auto &outSlotsInfo = digitalComp->definition->getOutputSlotsInfo();

        std::shared_ptr<SlotSceneComponent> newSlot = std::make_shared<SlotSceneComponent>();
        newSlot->setSlotType(SlotType::digitalOutput);
        newSlot->setIndex((int)outSlotsInfo.count - 1);
        parent->addOutputSlot(newSlot->getUuid(), outSlotsInfo.isResizeable);
        m_state.addComponent<SlotSceneComponent>(newSlot);
        m_state.attachChild(parent->getUuid(), newSlot->getUuid());
    }
} // namespace Bess::Canvas
