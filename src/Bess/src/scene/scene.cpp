#include "scene/scene.h"
#include "bess_uuid.h"
#include "common/log.h"
#include "component_catalog.h"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "events/application_event.h"
#include "pages/main_page/main_page_state.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include "types.h"
#include "ui/ui.h"
#include "ui/ui_main/ui_main.h"
#include "vulkan_core.h"
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

namespace Bess::Canvas {
    Scene::Scene() {
        reset();
    }

    Scene::~Scene() {
        destroy();
    }

    void Scene::destroy() {
        if (m_isDestroyed)
            return;

        BESS_INFO("[Scene] Destroying");
        m_viewport.reset();

        m_isDestroyed = true;
    }

    std::shared_ptr<Scene> Scene::instance() {
        static std::shared_ptr<Scene> m_instance = std::make_shared<Scene>();
        return m_instance;
    }

    void Scene::reset() {
        clear();

        m_size = glm::vec2(800.f, 600.f);

        auto &vkCore = Vulkan::VulkanCore::instance();
        m_viewport = std::make_shared<Viewport>(vkCore.getDevice(), vkCore.getSwapchain()->imageFormat(), vec2Extent2D(m_size));

        m_camera = m_viewport->getCamera();
        m_mousePos = {0.f, 0.f};
    }

    void Scene::clear() {
        m_registry.clear();
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
                        m_registry.clear<Components::HoveredEntityComponent>();
                        m_hoveredEntity = UUID::null;
                        break;
                    }
                    m_isLeftMousePressed = false;
                    m_registry.clear<Components::HoveredEntityComponent>();
                    m_hoveredEntity = UUID::null;
                }
                m_viewport->waitForPickingResults(5'000'000);
                updateHoveredId();
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
    }

    void Scene::selectAllEntities() {
        const auto view = m_registry.view<Canvas::Components::SimulationComponent>();
        for (const auto &entt : view)
            m_registry.emplace_or_replace<Components::SelectedComponent>(entt);
        const auto connectionView = m_registry.view<Canvas::Components::ConnectionComponent>();
        for (const auto &entt : connectionView)
            m_registry.emplace_or_replace<Components::SelectedComponent>(entt);
    }

    void Scene::handleKeyboardShortcuts() {
        const auto mainPageState = Pages::MainPageState::getInstance();
        if (mainPageState->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
            if (mainPageState->isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
                if (mainPageState->isKeyPressed(GLFW_KEY_Z)) {
                    m_cmdManager.redo();
                }
            } else if (mainPageState->isKeyPressed(GLFW_KEY_A)) { // ctrl-a select all components
                selectAllEntities();
            } else if (mainPageState->isKeyPressed(GLFW_KEY_C)) { // ctrl-c copy selected components
                copySelectedComponents();
            } else if (mainPageState->isKeyPressed(GLFW_KEY_V)) { // ctrl-v generate copied components
                generateCopiedComponents();
            } else if (mainPageState->isKeyPressed(GLFW_KEY_Z)) {
                m_cmdManager.undo();
            }
        } else if (mainPageState->isKeyPressed(GLFW_KEY_DELETE)) {
            const auto view = m_registry.view<Components::IdComponent, Components::SelectedComponent>();

            std::vector<UUID> entitesToDel = {};
            std::vector<entt::entity> connEntitesToDel = {};
            for (const auto &entt : view) {
                if (!m_registry.valid(entt))
                    continue;

                if (m_registry.all_of<Components::ConnectionComponent>(entt)) {
                    connEntitesToDel.emplace_back(entt);
                } else {
                    entitesToDel.emplace_back(getUuidOfEntity(entt));
                }
            }

            auto _ = m_cmdManager.execute<Commands::DeleteCompCommand, std::string>(entitesToDel);

            std::vector<UUID> connToDel = {};
            for (const auto ent : connEntitesToDel) {
                if (!m_registry.valid(ent))
                    continue;

                connToDel.emplace_back(getUuidOfEntity(ent));
            }

            _ = m_cmdManager.execute<Commands::DelConnectionCommand, std::string>(connToDel);
        } else if (mainPageState->isKeyPressed(GLFW_KEY_F)) {
            const auto view = m_registry.view<Components::IdComponent,
                                              Components::SelectedComponent,
                                              Components::TransformComponent>();

            // pick the first one to focus. if many are selected
            for (const auto &ent : view) {
                const auto &transform = view.get<Components::TransformComponent>(ent);
                m_camera->focusAtPoint(glm::vec2(transform.position), true);
                break;
            }
        } else if (mainPageState->isKeyPressed(GLFW_KEY_TAB)) {
            toggleSchematicView();
        }
    }

    void Scene::renderWithViewport(const std::shared_ptr<Viewport> &viewport) {
        const auto hoveredEntity = getEntityWithUuid(m_hoveredEntity);
        switch (m_sceneMode) {
        case SceneMode::general: {
            if (m_registry.valid(hoveredEntity) && m_registry.any_of<Components::SlotComponent, Components::ConnectionSegmentComponent>(hoveredEntity)) {
                UI::setCursorPointer();
            }

        } break;
        case SceneMode::move: {
            UI::setCursorMove();
        } break;
        }

        auto &inst = Bess::Vulkan::VulkanCore::instance();

        auto artistManager = viewport->getArtistManager();
        artistManager->setSchematicMode(m_isSchematicView);
        viewport->begin((int)inst.getCurrentFrameIdx(), ViewportTheme::colors.background, -1);
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
            mousePos_.y = UI::UIMain::state.viewportSize.y - mousePos_.y;
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
                int y = (int)(UI::UIMain::state.viewportSize.y - pos.y);
                m_viewport->setPickingCoord(x, y, w, h);
                m_selectInSelectionBox = false;
                m_getIdsInSelBox = true;
            } else {
                m_viewport->setPickingCoord(x, y);
            }
        }

        auto artistManager = viewport->getArtistManager();
        artistManager->setSchematicMode(m_isSchematicView);
        const auto artist = artistManager->getCurrentArtist();

        // Grid
        artist->getMaterialRenderer()->drawGrid(
            glm::vec3(0.f, 0.f, 0.1f),
            m_camera->getSpan(),
            -1,
            {
                .minorColor = ViewportTheme::colors.gridMinorColor,
                .majorColor = ViewportTheme::colors.gridMajorColor,
                .axisXColor = ViewportTheme::colors.gridAxisXColor,
                .axisYColor = ViewportTheme::colors.gridAxisYColor,
            },
            viewport->getCamera());

        // Connections
        const auto connectionsView = m_registry.view<Components::ConnectionComponent>();
        for (const auto entity : connectionsView) {
            artist->drawConnectionEntity(entity);
        }

        if (m_drawMode == SceneDrawMode::connection) {
            drawConnection();
        }

        { // Components
            const auto simCompView = m_registry.view<
                Components::SimulationComponent,
                Components::TagComponent,
                Components::SpriteComponent,
                Components::TransformComponent>();
            const auto &cam = viewport->getCamera();
            const auto &span = (cam->getSpan() / 2.f) + 200.f;
            const auto &camPos = cam->getPos();

            float x = 0.f, y = 0.f;
            for (const auto entity : simCompView) {
                const auto &transform = simCompView.get<Components::TransformComponent>(entity);

                x = transform.position.x - camPos.x;
                y = transform.position.y - camPos.y;

                // skipping if outside camera
                if (x < -span.x || x > span.x || y < -span.y || y > span.y)
                    continue;

                artist->drawSimEntity(
                    entity,
                    simCompView.get<Components::TagComponent>(entity),
                    transform,
                    simCompView.get<Components::SpriteComponent>(entity),
                    simCompView.get<Components::SimulationComponent>(entity));
            }
        }
        const auto nonSimCompView = m_registry.view<Components::NSComponent>();
        for (const auto entity : nonSimCompView) {
            artist->drawNonSimEntity(entity);
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

        auto renderer = m_viewport->getArtistManager()->getCurrentArtist()->getMaterialRenderer();
        renderer->drawQuad(glm::vec3(pos, 7.f), size, ViewportTheme::colors.selectionBoxFill, -1, props);
    }

    void Scene::drawConnection() {
        const auto connStartEntity = getEntityWithUuid(m_connectionStartEntity);
        if (!m_registry.valid(connStartEntity)) {
            m_drawMode = SceneDrawMode::none;
            return;
        }

        const auto artist = m_viewport->getArtistManager()->getCurrentArtist();
        artist->drawGhostConnection(connStartEntity, toScenePos(m_mousePos));
    }

    UUID Scene::createSimEntity(const UUID &simEngineEntt,
                                const SimEngine::ComponentDefinition &comp, const glm::vec2 &pos) {
        const auto state = SimEngine::SimulationEngine::instance().getComponentState(simEngineEntt);
        const std::vector<UUID> inputSlotIds(state.inputStates.size());
        const std::vector<UUID> outputSlotIds(state.outputStates.size());
        const UUID uuid;
        return createSimEntity(simEngineEntt, comp, pos, uuid, inputSlotIds, outputSlotIds);
    }

    UUID Scene::createSimEntity(const UUID &simEngineEntt, const SimEngine::ComponentDefinition &comp, const glm::vec2 &pos,
                                UUID uuid, const std::vector<UUID> &inputSlotIds, const std::vector<UUID> &outputSlotIds) {
        auto entity = m_registry.create();
        const auto &catalog = SimEngine::ComponentCatalog::instance();

        const auto &idComp = m_registry.emplace<Components::IdComponent>(entity, uuid);
        auto &transformComp = m_registry.emplace<Components::TransformComponent>(entity);
        auto &sprite = m_registry.emplace<Components::SpriteComponent>(entity);
        auto &tag = m_registry.emplace<Components::TagComponent>(entity);
        auto &simComp = m_registry.emplace<Components::SimulationComponent>(entity);

        bool isInput = catalog.isSpecialCompDef(comp.getHash(), SimEngine::ComponentCatalog::SpecialType::input);
        bool isOutput = catalog.isSpecialCompDef(comp.getHash(), SimEngine::ComponentCatalog::SpecialType::output);

        if (isInput) {
            m_registry.emplace<Components::SimulationInputComponent>(entity);
        } else if (isOutput) {
            m_registry.emplace<Components::SimulationOutputComponent>(entity);
        } else if (catalog.isSpecialCompDef(comp.getHash(), SimEngine::ComponentCatalog::SpecialType::stateMonitor)) {
            m_registry.emplace<Components::SimulationStateMonitor>(entity);
        }

        tag.name = comp.name;
        tag.type.simCompHash = comp.getHash();
        tag.isSimComponent = true;

        transformComp.position = glm::vec3(getSnappedPos(pos), getNextZCoord());

        if (isInput || isOutput) {
            const glm::vec4 ioCompColor = glm::vec4(0.2f, 0.2f, 0.4f, 0.6f);
            sprite.color = ioCompColor;
            sprite.borderRadius = glm::vec4(8.f);
        } else {
            sprite.color = ViewportTheme::colors.componentBG;
            sprite.borderRadius = glm::vec4(6.f);
            sprite.headerColor = ViewportTheme::getCompHeaderColor(comp.category);
        }

        sprite.borderSize = glm::vec4(1.f);
        sprite.borderColor = ViewportTheme::colors.componentBorder;

        simComp.simEngineEntity = simEngineEntt;

        const auto state = SimEngine::SimulationEngine::instance().getComponentState(simEngineEntt);

        for (int i = 0; i < state.inputStates.size(); i++) {
            simComp.inputSlots.emplace_back(createSlotEntity(inputSlotIds[i], Components::SlotType::digitalInput, idComp.uuid, i));
        }

        for (int i = 0; i < state.outputStates.size(); i++) {
            simComp.outputSlots.emplace_back(createSlotEntity(outputSlotIds[i], Components::SlotType::digitalOutput, idComp.uuid, i));
        }

        simComp.defHash = comp.getHash();

        transformComp.scale = m_viewport->getArtistManager()->getCurrentArtist()->calcCompSize(entity, simComp, comp.name);

        BESS_INFO("[Scene] Created entity {}", (uint64_t)entity);

        setLastCreatedComp({.componentDefinition = comp});
        return idComp.uuid;
    }

    UUID Scene::createNonSimEntity(const Canvas::Components::NSComponent &comp, const glm::vec2 &pos) {
        using namespace Canvas::Components;
        auto entity = m_registry.create();
        const auto &idComp = m_registry.emplace<Components::IdComponent>(entity);
        auto &nonSimComp = m_registry.emplace<Components::NSComponent>(entity);
        auto &tag = m_registry.emplace<Components::TagComponent>(entity);
        auto &transformComp = m_registry.emplace<Components::TransformComponent>(entity);

        tag.name = comp.name;
        tag.type.nsCompType = comp.type;
        nonSimComp.type = comp.type;

        transformComp.position = glm::vec3(pos, getNextZCoord());
        transformComp.scale = glm::vec2(0.f, 0.f);

        switch (comp.type) {
            {
            case Components::NSComponentType::text: {
                auto &textComp = m_registry.emplace<Components::TextNodeComponent>(entity);
                textComp.text = "New Text";
                textComp.fontSize = 20.f;
                textComp.color = ViewportTheme::colors.text;
            } break;
            default:
                break;
            }
        }
        BESS_INFO("[Scene] Created Non simulation entity {}", (uint64_t)entity);
        setLastCreatedComp({.nsComponent = comp});
        return idComp.uuid;
    }

    void Scene::deleteSceneEntity(const UUID &entUuid) {
        auto ent = getEntityWithUuid(entUuid);
        auto hoveredEntity = getEntityWithUuid(m_hoveredEntity);
        if (ent == hoveredEntity)
            hoveredEntity = entt::null;

        if (m_registry.all_of<Components::SimulationComponent>(ent)) {
            const auto &simComp = m_registry.get<Components::SimulationComponent>(ent);

            // take care of slots
            for (auto slot : simComp.inputSlots) {
                auto &slotComp = m_registry.get<Components::SlotComponent>(getEntityWithUuid(slot));
                for (auto &conn : slotComp.connections) {
                    deleteConnectionFromScene(conn);
                }

                m_registry.destroy(getEntityWithUuid(slot));
                m_uuidToEntt.erase(slot);
            }

            for (auto slot : simComp.outputSlots) {
                auto &slotComp = m_registry.get<Components::SlotComponent>(getEntityWithUuid(slot));
                for (auto &conn : slotComp.connections) {
                    deleteConnectionFromScene(conn);
                }

                m_registry.destroy(getEntityWithUuid(slot));
                m_uuidToEntt.erase(slot);
            }
        }

        // remove from registry
        m_registry.destroy(ent);
        m_uuidToEntt.erase(entUuid);
        BESS_INFO("[Scene] Deleted entity {}", (uint64_t)ent);
    }

    // void Scene::deleteEntity(const UUID &entUuid) {
    //     auto ent = getEntityWithUuid(entUuid);
    //     if (m_registry.all_of<Components::SimulationComponent>(ent)) {
    //         auto &simComp = m_registry.get<Components::SimulationComponent>(ent);
    //         // remove from simlation engine
    //         SimEngine::SimulationEngine::instance().deleteComponent(simComp.simEngineEntity);
    //     }
    //
    //     deleteSceneEntity(entUuid);
    // }

    UUID Scene::createSlotEntity(Components::SlotType type, const UUID &parent, unsigned int idx) {
        return createSlotEntity(UUID{}, type, parent, idx);
    }

    UUID Scene::createSlotEntity(UUID uuid, Components::SlotType type, const UUID &parent, unsigned int idx) {
        const auto entity = m_registry.create();
        const auto &idComp = m_registry.emplace<Components::IdComponent>(entity, uuid);
        auto &transform = m_registry.emplace<Components::TransformComponent>(entity);
        auto &sprite = m_registry.emplace<Components::SpriteComponent>(entity);
        auto &slot = m_registry.emplace<Components::SlotComponent>(entity);

        transform.scale = glm::vec2(20.f);
        sprite.color = ViewportTheme::colors.stateLow;
        sprite.borderColor = ViewportTheme::colors.stateLow;
        sprite.borderSize = glm::vec4(10.f);

        slot.parentId = parent;
        slot.slotType = type;
        slot.idx = idx;

        return idComp.uuid;
    }

    const glm::vec2 &Scene::getSize() const {
        return m_size;
    }

    entt::entity Scene::getEntityWithUuid(const UUID &uuid) {
        if (m_uuidToEntt.contains(uuid)) {
            return m_uuidToEntt.at(uuid);
        }
        const auto view = m_registry.view<Components::IdComponent>();
        for (const auto &ent : view) {
            const auto &idComp = view.get<Components::IdComponent>(ent);
            if (idComp.uuid == uuid) {
                m_uuidToEntt[uuid] = ent;
                return ent;
            }
        }
        return entt::null;
    }

    const UUID &Scene::getUuidOfEntity(entt::entity ent) const {
        const auto &idComp = m_registry.get<Components::IdComponent>(ent);
        return idComp.uuid;
    }

    entt::entity Scene::getSceneEntityFromSimUuid(const UUID &uuid) const {
        for (const auto ent : m_registry.view<Components::SimulationComponent>()) {
            const auto &comp = m_registry.get<Components::SimulationComponent>(ent);
            if (comp.simEngineEntity == uuid)
                return ent;
        }

        BESS_ASSERT(false, "Failed to find a Simulation Component in scene with this mapped uuid");
        throw std::runtime_error("Failed to find a Simulation Component in scene with this mapped uuid");
    }

    void Scene::resize(const glm::vec2 &size) {
        m_size = UI::UIMain::state.viewportSize;
        m_viewport->resize(vec2Extent2D(m_size));
        m_camera->resize(m_size.x, m_size.y);
    }

    glm::vec2 Scene::getViewportMousePos(const glm::vec2 &mousePos) const {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
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

    void Scene::dragConnectionSegment(entt::entity ent) {
        const auto view = m_registry.view<Components::TransformComponent,
                                          Components::ConnectionSegmentComponent,
                                          Components::SlotComponent,
                                          Components::ConnectionComponent>();

        const glm::vec2 newPos = toScenePos(m_mousePos);

        auto &comp = view.get<Components::ConnectionSegmentComponent>(ent);
        const auto artist = m_viewport->getArtistManager()->getCurrentArtist();

        if (comp.isHead() || comp.isTail()) {
            const auto newEntt = m_registry.create();
            const auto &idComp = m_registry.emplace<Components::IdComponent>(newEntt);
            auto &compNew = m_registry.emplace<Components::ConnectionSegmentComponent>(newEntt);
            compNew.parent = comp.parent;

            glm::vec2 slotPos;

            if (comp.isHead()) {
                comp.prev = idComp.uuid;
                compNew.next = getUuidOfEntity(ent);
                auto &connComponent = view.get<Components::ConnectionComponent>(getEntityWithUuid(comp.parent));
                const auto &slotComponent = view.get<Components::SlotComponent>(getEntityWithUuid(connComponent.inputSlot));
                const auto &parentTransform = view.get<Components::TransformComponent>(getEntityWithUuid(slotComponent.parentId));
                connComponent.segmentHead = idComp.uuid;
                slotPos = artist->getSlotPos(slotComponent, parentTransform);
            } else {
                comp.next = idComp.uuid;
                compNew.prev = getUuidOfEntity(ent);
                const auto &connComponent = m_registry.get<Components::ConnectionComponent>(getEntityWithUuid(comp.parent));
                const auto &slotComponent = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComponent.outputSlot));
                const auto &parentTransform = view.get<Components::TransformComponent>(getEntityWithUuid(slotComponent.parentId));
                slotPos = artist->getSlotPos(slotComponent, parentTransform);
            }

            if (comp.pos.x == 0) // if current is vertical
                compNew.pos.x = slotPos.x;
            else
                compNew.pos.y = slotPos.y;
        }

        if (comp.pos.x == 0) {
            comp.pos.y = newPos.y;
        } else if (comp.pos.y == 0) {
            comp.pos.x = newPos.x;
        }
    }

    void Scene::moveConnection(entt::entity ent, glm::vec2 &dPos) {

        const auto &connComp = m_registry.get<Components::ConnectionComponent>(ent);
        auto seg = connComp.segmentHead;

        while (seg != UUID::null) {
            const auto segEnt = getEntityWithUuid(seg);
            auto &segComp = m_registry.get<Components::ConnectionSegmentComponent>(segEnt);

            if (!m_dragStartConnSeg.contains(getUuidOfEntity(segEnt))) {
                m_dragStartConnSeg[getUuidOfEntity(segEnt)] = segComp;
            }

            if (segComp.isHead() || segComp.isTail()) {
                seg = segComp.next;
                continue;
            }

            if (segComp.pos.y == 0) {
                segComp.pos.x += dPos.x;
            } else {
                segComp.pos.y += dPos.y;
            }

            seg = segComp.next;
        }
    }

    void Scene::updateHoveredId() {
        m_viewport->tryUpdatePickingResults();

        const auto &ids = m_viewport->getPickingIdsResult();
        const int32_t hoverId = (ids.empty()) ? -1 : ids[0];
        const entt::entity e = (entt::entity)hoverId;

        m_registry.clear<Components::HoveredEntityComponent>();
        if (m_registry.valid(e)) {
            m_registry.emplace<Components::HoveredEntityComponent>(e);
            m_hoveredEntity = getUuidOfEntity(e);
        } else {
            m_hoveredEntity = UUID::null;
        }
    }

    void Scene::onMouseMove(const glm::vec2 &pos) {
        m_dMousePos = toScenePos(pos) - toScenePos(m_mousePos);
        m_mousePos = pos;

        {
            auto mousePos_ = m_mousePos;
            mousePos_.y = UI::UIMain::state.viewportSize.y - mousePos_.y;
            int x = static_cast<int>(mousePos_.x);
            int y = static_cast<int>(mousePos_.y);
            m_viewport->setPickingCoord(x, y);
        }

        if (!m_isDragging || m_drawMode == SceneDrawMode::connection) {
            m_viewport->waitForPickingResults(1'000'000);
            updateHoveredId();
        }

        if (m_isLeftMousePressed && m_drawMode == SceneDrawMode::none) {
            const auto hoveredEntity = getEntityWithUuid(m_hoveredEntity);
            const auto selectedComponentsSize = m_registry.view<Components::SelectedComponent>()->size();
            if (!m_registry.valid(hoveredEntity) && selectedComponentsSize == 0) {
                m_drawMode = SceneDrawMode::selectionBox;
                m_selectionBoxStart = m_mousePos;
            } else if (selectedComponentsSize == 1 && m_registry.valid(hoveredEntity) && m_registry.all_of<Components::ConnectionSegmentComponent>(hoveredEntity)) {
                dragConnectionSegment(hoveredEntity);
                m_isDragging = true;
            } else {
                const auto view = m_registry.view<Components::SelectedComponent, Components::TransformComponent>();
                for (const auto &ent : view) {
                    auto &transformComp = view.get<Components::TransformComponent>(ent);
                    if (!m_dragStartTransforms.contains(getUuidOfEntity(ent))) {
                        m_dragStartTransforms[getUuidOfEntity(ent)] = transformComp;
                    }
                    glm::vec2 newPos = toScenePos(m_mousePos);
                    if (!m_dragOffsets.contains(ent)) {
                        const auto offset = newPos - glm::vec2(transformComp.position);
                        m_dragOffsets[ent] = offset;
                    }
                    newPos -= m_dragOffsets[ent];
                    newPos = getSnappedPos(newPos);
                    transformComp.position = {newPos, transformComp.position.z};
                }

                const auto connectionView = m_registry.view<Components::SelectedComponent, Components::ConnectionComponent>();
                for (const auto &ent : connectionView) {
                    moveConnection(ent, m_dMousePos);
                }

                m_isDragging = true;
            }
        } else if (m_isMiddleMousePressed) {
            m_camera->incrementPos(-m_dMousePos);
        }
    }

    bool Scene::isCursorInViewport(const glm::vec2 &pos) const {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        const auto &viewportSize = UI::UIMain::state.viewportSize;
        return pos.x >= 5.f &&
               pos.x < viewportSize.x - 5.f &&
               pos.y >= 5.f &&
               pos.y < viewportSize.y - 5.f;
    }

    void Scene::removeConnectionEntt(const entt::entity ent) {
        m_hoveredEntity = UUID::null;
        const auto &connComp = m_registry.get<Components::ConnectionComponent>(ent);

        auto segEntt = connComp.segmentHead;
        while (segEntt != UUID::null) {
            const auto connSegComp = m_registry.get<
                Components::ConnectionSegmentComponent>(getEntityWithUuid(segEntt));
            m_registry.destroy(getEntityWithUuid(segEntt));
            m_uuidToEntt.erase(segEntt);
            segEntt = connSegComp.next;
        }

        m_registry.destroy(ent);

        BESS_INFO("[Scene] Deleted connection entity");
    }

    void Scene::deleteConnectionFromScene(const UUID &uuid) {
        if (m_hoveredEntity == uuid)
            m_hoveredEntity = UUID::null;

        const auto entity = getEntityWithUuid(uuid);
        const auto &connComp = m_registry.get<Components::ConnectionComponent>(entity);

        auto &slotCompA = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComp.inputSlot));
        auto &slotCompB = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComp.outputSlot));

        removeConnectionEntt(entity);
        m_uuidToEntt.erase(uuid);

        slotCompA.connections.erase(std::ranges::remove(slotCompA.connections, uuid).begin(), slotCompA.connections.end());
        slotCompB.connections.erase(std::ranges::remove(slotCompB.connections, uuid).begin(), slotCompB.connections.end());
        BESS_INFO("[Scene] Removed connection from scene");
    }

    void Scene::deleteConnection(const UUID &entUuid) {
        const auto entity = getEntityWithUuid(entUuid);
        const auto &connComp = m_registry.get<Components::ConnectionComponent>(entity);

        const auto &slotCompA = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComp.inputSlot));
        const auto &slotCompB = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComp.outputSlot));

        const auto &simCompA = m_registry.get<Components::SimulationComponent>(getEntityWithUuid(slotCompA.parentId));
        const auto &simCompB = m_registry.get<Components::SimulationComponent>(getEntityWithUuid(slotCompB.parentId));

        const auto pinTypeA = slotCompA.slotType == Components::SlotType::digitalInput
                                  ? SimEngine::PinType::input
                                  : SimEngine::PinType::output;
        const auto pinTypeB = slotCompB.slotType == Components::SlotType::digitalInput
                                  ? SimEngine::PinType::input
                                  : SimEngine::PinType::output;

        SimEngine::SimulationEngine::instance().deleteConnection(simCompA.simEngineEntity, pinTypeA,
                                                                 (int)slotCompA.idx, simCompB.simEngineEntity,
                                                                 pinTypeB, (int)slotCompB.idx);

        deleteConnectionFromScene(entUuid);
    }

    UUID Scene::generateBasicConnection(entt::entity inputSlot, entt::entity outputSlot) {
        const auto connEntt = m_registry.create();
        auto &idComp = m_registry.emplace<Components::IdComponent>(connEntt);
        auto &connComp = m_registry.emplace<Components::ConnectionComponent>(connEntt);
        m_registry.emplace<Components::SpriteComponent>(connEntt);
        connComp.inputSlot = getUuidOfEntity(inputSlot);
        connComp.outputSlot = getUuidOfEntity(outputSlot);

        const auto view = m_registry.view<Components::SlotComponent, Components::TransformComponent>();
        auto &inpSlotComp = view.get<Components::SlotComponent>(inputSlot);
        auto &outSlotComp = view.get<Components::SlotComponent>(outputSlot);

        inpSlotComp.connections.emplace_back(idComp.uuid);
        outSlotComp.connections.emplace_back(idComp.uuid);

        const auto &inpParentTransform = view.get<Components::TransformComponent>(getEntityWithUuid(inpSlotComp.parentId));
        const auto artist = m_viewport->getArtistManager()->getCurrentArtist();
        const auto inputSlotPos = artist->getSlotPos(inpSlotComp, inpParentTransform);
        const auto &slotComp = view.get<Components::SlotComponent>(outputSlot);
        const auto &parentTransform = view.get<Components::TransformComponent>(getEntityWithUuid(slotComp.parentId));
        const auto outputSlotPos = artist->getSlotPos(slotComp, parentTransform);

        const auto dX = inputSlotPos.x - outputSlotPos.x;
        auto offset = dX * 0.25f;

        const auto connSeg1 = m_registry.create();
        const auto connSeg2 = m_registry.create();
        const auto connSeg3 = m_registry.create();

        const auto &idComp1 = m_registry.emplace<Components::IdComponent>(connSeg1);
        const auto &idComp2 = m_registry.emplace<Components::IdComponent>(connSeg2);
        const auto &idComp3 = m_registry.emplace<Components::IdComponent>(connSeg3);

        connComp.segmentHead = idComp1.uuid;

        auto &connSegComp1 = m_registry.emplace<Components::ConnectionSegmentComponent>(connSeg1);
        auto &connSegComp2 = m_registry.emplace<Components::ConnectionSegmentComponent>(connSeg2);
        auto &connSegComp3 = m_registry.emplace<Components::ConnectionSegmentComponent>(connSeg3);

        connSegComp1.parent = idComp.uuid;
        connSegComp2.parent = idComp.uuid;
        connSegComp3.parent = idComp.uuid;

        connSegComp1.next = idComp2.uuid;
        connSegComp2.next = idComp3.uuid;
        connSegComp2.prev = idComp1.uuid;
        connSegComp3.prev = idComp2.uuid;

        /// flow goes from input slot to output slot

        if (dX < 0.f) {
            const auto connSeg4 = m_registry.create();
            const auto &idComp4 = m_registry.emplace<Components::IdComponent>(connSeg4);
            auto &connSegComp4 = m_registry.emplace<Components::ConnectionSegmentComponent>(connSeg4);
            connSegComp4.parent = idComp.uuid;

            const auto connSeg5 = m_registry.create();
            const auto &idComp5 = m_registry.emplace<Components::IdComponent>(connSeg5);
            auto &connSegComp5 = m_registry.emplace<Components::ConnectionSegmentComponent>(connSeg5);
            connSegComp5.parent = idComp.uuid;

            connSegComp3.next = idComp4.uuid;
            connSegComp4.prev = idComp3.uuid;

            connSegComp4.next = idComp5.uuid;
            connSegComp5.prev = idComp4.uuid;

            const float offsetY = (inputSlotPos.y - outputSlotPos.y) * 0.25f;
            offset = 24.f;
            connSegComp1.pos = glm::vec2(0, inputSlotPos.y);
            connSegComp2.pos = glm::vec2(inputSlotPos.x - offset, 0);
            connSegComp3.pos = glm::vec2(0, outputSlotPos.y + offsetY);
            connSegComp4.pos = glm::vec2(outputSlotPos.x + offset, 0);
            connSegComp5.pos = glm::vec2(0, outputSlotPos.y);
        } else {
            if (std::abs(offset) < 24.f) {
                offset = dX * 0.5f;
            }
            connSegComp1.pos = glm::vec2(0, inputSlotPos.y + 20.f);
            connSegComp2.pos = glm::vec2(outputSlotPos.x + offset, 0);
            connSegComp3.pos = glm::vec2(0, outputSlotPos.y + 20.f);
        }

        return idComp.uuid;
    }

    UUID Scene::connectSlots(UUID startSlot, UUID endSlot) {
        return generateBasicConnection(getEntityWithUuid(startSlot),
                                       getEntityWithUuid(endSlot));
    }

    UUID Scene::connectComponents(UUID compIdA, int slotIdxA, bool isAInput, UUID compIdB, int slotIdxB) {
        const auto entA = getEntityWithUuid(compIdA);
        const auto &simComp = m_registry.get<Components::SimulationComponent>(entA);
        UUID slotA = isAInput ? simComp.inputSlots[slotIdxA] : simComp.outputSlots[slotIdxA];
        const auto entB = getEntityWithUuid(compIdB);
        const auto &simCompB = m_registry.get<Components::SimulationComponent>(entB);
        UUID slotB = !isAInput ? simCompB.inputSlots[slotIdxB] : simCompB.outputSlots[slotIdxB];

        const auto _ = m_cmdManager.execute<Commands::ConnectCommand, UUID>(
            slotA, slotB);
        return _.value();
    }

    bool Scene::isEntityValid(const UUID &uuid) {
        return uuid != UUID::null && m_registry.valid(getEntityWithUuid(uuid));
    }

    void Scene::onRightMouse(bool isPressed) {
        if (!isPressed) {
            return;
        }

        if (!isEntityValid(m_hoveredEntity) && m_lastCreatedComp.set) {
            const auto def = m_lastCreatedComp.componentDefinition;
            const Commands::AddCommandData cmdData = {
                .def = def,
                .nsComp = m_lastCreatedComp.nsComponent,
                .inputCount = def.inputCount,
                .outputCount = def.outputCount,
                .pos = getSnappedPos(toScenePos(m_mousePos)),
            };

            const auto res = m_cmdManager.execute<Canvas::Commands::AddCommand, std::vector<UUID>>(std::vector{cmdData});
            if (!res.has_value()) {
                BESS_ERROR("Failed to execute AddCommand");
            }
        }

        if (isEntityValid(m_hoveredEntity)) {
            const auto hoveredEntity = getEntityWithUuid(m_hoveredEntity);
            if (m_registry.all_of<Components::SimulationInputComponent>(hoveredEntity)) {
                const auto &simComp = m_registry.get<Components::SimulationComponent>(hoveredEntity);
                const auto currentState = SimEngine::SimulationEngine::instance().getDigitalPinState(simComp.simEngineEntity, SimEngine::PinType::output, 0);

                const auto newState = currentState.state == SimEngine::LogicState::high ? SimEngine::LogicState::low : SimEngine::LogicState::high;

                auto _ = m_cmdManager.execute<Commands::SetInputCommand, std::string>(m_hoveredEntity, newState);
            }
        }
    }

    void Scene::onMiddleMouse(bool isPressed) {
        m_isMiddleMousePressed = isPressed;
    }

    void Scene::onLeftMouse(bool isPressed) {
        m_isLeftMousePressed = isPressed;
        if (!isPressed) {
            if (m_drawMode == SceneDrawMode::selectionBox) {
                m_drawMode = SceneDrawMode::none;
                m_selectInSelectionBox = true;
                m_selectionBoxEnd = m_mousePos;
            }
            if (m_isDragging) {
                m_isDragging = false;
                m_dragOffsets.clear();
                std::unique_ptr<Commands::UpdateEnttComponentsCommand> cmd =
                    std::make_unique<Commands::UpdateEnttComponentsCommand>(m_registry);
                for (auto &[uuid, comp] : m_dragStartTransforms) {
                    cmd->addUpdate<Components::TransformComponent>(uuid, comp, true);
                }

                for (auto &[uuid, comp] : m_dragStartConnSeg) {
                    cmd->addUpdate<Components::ConnectionSegmentComponent>(uuid, comp, true);
                }

                m_cmdManager.execute(std::move(cmd));
                m_dragStartTransforms.clear();
                m_dragStartConnSeg.clear();
            }
            return;
        }
        const auto hoveredEntity = getEntityWithUuid(m_hoveredEntity);

        // toggeling selection of hovered entity on click
        if (m_registry.valid(hoveredEntity)) {
            if (m_registry.all_of<Components::SlotComponent>(hoveredEntity)) {
                m_registry.clear<Components::SelectedComponent>();
                if (m_drawMode == SceneDrawMode::none) {
                    m_connectionStartEntity = m_hoveredEntity;
                    m_drawMode = SceneDrawMode::connection;
                } else if (m_drawMode == SceneDrawMode::connection) {
                    m_drawMode = SceneDrawMode::none;
                    auto _ = m_cmdManager.execute<Commands::ConnectCommand, UUID>(
                        m_connectionStartEntity, m_hoveredEntity);
                }
            } else {
                if (m_sceneMode != SceneMode::move &&
                    !Pages::MainPageState::getInstance()->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
                    m_registry.clear<Components::SelectedComponent>();
                }

                if (m_registry.all_of<Components::ConnectionSegmentComponent>(hoveredEntity)) {
                    const auto &segComp = m_registry.get<Components::ConnectionSegmentComponent>(hoveredEntity);
                    toggleSelectComponent(segComp.parent);
                } else {
                    toggleSelectComponent(hoveredEntity);
                }
            }
        } else if (m_sceneMode != SceneMode::move) { // deselecting all when clicking outside
            m_registry.clear<Components::SelectedComponent>();
            m_drawMode = SceneDrawMode::none;
        }
    }

    void Scene::toggleSelectComponent(const UUID &uuid) {
        const auto ent = getEntityWithUuid(uuid);
        toggleSelectComponent(ent);
    }

    void Scene::toggleSelectComponent(entt::entity ent) {
        const bool isSelected = m_registry.all_of<Canvas::Components::SelectedComponent>(ent);
        if (isSelected)
            m_registry.remove<Canvas::Components::SelectedComponent>(ent);
        else
            m_registry.emplace<Canvas::Components::SelectedComponent>(ent);
    }

    void Scene::copySelectedComponents() {
        m_copiedComponents.clear();

        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto &catalogInstance = SimEngine::ComponentCatalog::instance();

        const auto view = m_registry.view<Components::SelectedComponent, Components::SimulationComponent>();
        for (const auto entt : view) {
            auto &comp = view.get<Components::SimulationComponent>(entt);
            CopiedComponent compData{};
            compData.def = simEngine.getComponentDefinition(comp.simEngineEntity);
            compData.inputCount = (int)comp.inputSlots.size();
            compData.outputCount = (int)comp.outputSlots.size();
            m_copiedComponents.emplace_back(compData);
        }

        const auto nsCompView = m_registry.view<Components::SelectedComponent, Components::NSComponent>();
        for (const auto entt : nsCompView) {
            const auto &comp = nsCompView.get<Components::NSComponent>(entt);
            CopiedComponent compData{};
            compData.nsComp = comp;
            m_copiedComponents.emplace_back(compData);
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
        m_registry.clear<Components::SelectedComponent>();

        if (!m_viewport->tryUpdatePickingResults())
            return false;

        std::vector<int> ids = m_viewport->getPickingIdsResult();

        if (ids.size() == 0)
            return false;

        std::set<int> uniqueIds(ids.begin(), ids.end());
        std::unordered_map<int, bool> selected = {};

        for (const auto &id : uniqueIds) {
            auto entt = (entt::entity)id;
            if (!m_registry.valid(entt))
                continue;

            bool isConnection = false;
            if (const auto *segComp = m_registry.try_get<Components::ConnectionSegmentComponent>(entt)) {
                entt = getEntityWithUuid(segComp->parent);
                isConnection = true;
            }

            if (selected.contains(id))
                continue;

            if (isConnection || m_registry.any_of<Components::SimulationComponent, Components::NSComponent>(entt)) {
                m_registry.emplace_or_replace<Components::SelectedComponent>(entt);
            }

            selected[id] = true;
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

    entt::registry &Scene::getEnttRegistry() {
        return m_registry;
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

    void Scene::setLastCreatedComp(LastCreatedComponent comp) {
        m_lastCreatedComp = std::move(comp);
        m_lastCreatedComp.set = true;
    }

    void Scene::setSceneMode(SceneMode mode) {
        m_sceneMode = mode;
    }

    SceneMode Scene::getSceneMode() const {
        return m_sceneMode;
    }

    bool *Scene::getIsSchematicViewPtr() {
        return &m_isSchematicView;
    }

    void Scene::toggleSchematicView() {
        m_isSchematicView = !m_isSchematicView;
    }

    std::shared_ptr<ArtistManager> Scene::getArtistManager() {
        return m_viewport->getArtistManager();
    }

    VkExtent2D Scene::vec2Extent2D(const glm::vec2 &vec) {
        return {(uint32_t)vec.x, (uint32_t)vec.y};
    }

    glm::vec2 Scene::getSnappedPos(const glm::vec2 &pos) const {
        constexpr float snapSize = 5.f;
        return glm::vec2(glm::round(pos / snapSize)) * snapSize;
    }

    void Scene::drawScratchContent(TFrameTime ts, const std::shared_ptr<Viewport> &viewport) {
        return;
        static float elapsed = 0.f;
        auto renderer = viewport->getArtistManager()->getCurrentArtist()->getPathRenderer();

        std::vector<glm::vec3> curveAPoints;
        std::vector<glm::vec3> curveBPoints;

        constexpr float curve2Offset = std::numbers::pi_v<float>;
        constexpr float step = 0.1f;
        constexpr float amplitude = 50.f;
        constexpr float rungSpacing = std::numbers::pi_v<float> / 8.f;
        constexpr float lengthRad = std::numbers::pi_v<float> * 6.f;
        constexpr float wavelength = 50.f;
        constexpr float lengthPx = wavelength * lengthRad;

        static const glm::mat4 transform = glm::rotate(glm::mat4(1.f), glm::radians(-45.f), {0.f, 0.f, 1.f});

        renderer->beginPathMode({0.f, 0.f, 1.f}, 2.f, glm::vec4(1.f), -1);

        std::vector<glm::vec3> curveA, curveB;
        for (float i = 0; i <= lengthRad; i += step) {
            float x = wavelength * i;
            float phase = i - elapsed;
            float yA = std::sin(phase) * amplitude;
            float yB = std::sin(phase + curve2Offset) * amplitude;
            curveA.emplace_back(x, -yA, 1.f);
            curveB.emplace_back(x, -yB, 1.f);
        }

        for (float phi = 0; phi <= lengthRad; phi += rungSpacing) {
            float phase = phi + elapsed;
            float yA = std::sin(phi) * amplitude;
            float yB = std::sin(phi + curve2Offset) * amplitude;

            float x = std::fmod((phi + elapsed), lengthRad);
            x *= 50.f;

            glm::vec3 aPos = {x, -yA, 0.9f};
            glm::vec3 bPos = {x, -yB, 0.9f};

            renderer->pathMoveTo(glm::vec3(transform * glm::vec4({aPos, 1.f})));
            renderer->pathLineTo(glm::vec3(transform * glm::vec4({bPos, 1.f})), 2.f, glm::vec4(1.f), -1);
        }

        for (int i = 0; i < curveA.size(); i++) {
            if (i == 0)
                renderer->pathMoveTo(glm::vec3(transform * glm::vec4({curveA[i], 1.f})));
            else
                renderer->pathLineTo(glm::vec3(transform * glm::vec4({curveA[i], 1.f})), 2.f, {1.f, 1.f, 1.f, 1.f}, -1);
        }
        for (int i = 0; i < curveB.size(); i++) {
            if (i == 0)
                renderer->pathMoveTo(glm::vec3(transform * glm::vec4({curveB[i], 1.f})));
            else
                renderer->pathLineTo(glm::vec3(transform * glm::vec4({curveB[i], 1.f})), 2.f, {1.f, 1.f, 1.f, 1.f}, -1);
        }

        renderer->endPathMode();

        elapsed += (float)ts.count() * 0.001f;
    }

    bool Scene::isEntityHovered(const entt::entity &ent) const {
        return getUuidOfEntity(ent) == m_hoveredEntity;
    }
} // namespace Bess::Canvas
