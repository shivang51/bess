#include "scene/scene.h"
#include "bess_uuid.h"
#include "common/log.h"
#include "component_catalog.h"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "events/application_event.h"
#include "ext/matrix_transform.hpp"
#include "fwd.hpp"
#include "gtc/type_ptr.hpp"
#include "pages/main_page/main_page_state.h"
#include "scene/artist/artist_manager.h"
#include "scene/artist/base_artist.h"
#include "scene/commands/add_command.h"
#include "scene/commands/connect_command.h"
#include "scene/commands/del_connection_command.h"
#include "scene/commands/delete_comp_command.h"
#include "scene/commands/set_input_command.h"
#include "scene/commands/update_entt_comp_command.h"
#include "scene/components/components.h"
#include "scene/components/non_sim_comp.h"
#include "scene/renderer/renderer.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include "types.h"
#include "ui/ui.h"
#include "ui/ui_main/ui_main.h"
#include <cstdint>
#include <memory>

using Renderer = Bess::Renderer2D::Renderer;

namespace Bess::Canvas {
    Scene::Scene() {
        Renderer2D::Renderer::init();
        reset();
    }

    Scene::~Scene() {}

    Scene &Scene::instance() {
        static Scene m_instance;
        return m_instance;
    }

    void Scene::reset() {
        clear();

        m_size = glm::vec2(800.f / 600.f, 1.f);
        m_camera = std::make_shared<Camera>(m_size.x, m_size.y);

        std::vector<Gl::FBAttachmentType> attachments = {Gl::FBAttachmentType::RGBA_RGBA,
                                                         Gl::FBAttachmentType::R32I_REDI,
                                                         Gl::FBAttachmentType::RGBA_RGBA,
                                                         Gl::FBAttachmentType::DEPTH32F_STENCIL8};
        m_msaaFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments, true);

        attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::DEPTH32F_STENCIL8};
        m_shadowFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments);

        attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::RGBA_RGBA};
        m_placeHolderFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments);

        attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::R32I_REDI};
        m_normalFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments);

        m_placeHolderTexture = std::make_shared<Gl::Texture>("assets/images/crosshairs_tilesheet_white.png");
        m_placeHolderSubTexture = std::make_shared<Gl::SubTexture>(m_placeHolderTexture, glm::vec2(5.f, 5.f),
                                                                   glm::vec2(64.f, 64.f), 5, glm::vec2(1.f, 1.f));

        m_artistManager = std::make_shared<ArtistManager>(this);
    }

    void Scene::clear() {
        m_registry.clear();
        m_compZCoord = m_zIncrement;
        m_lastCreatedComp = {};
        m_copiedComponents = {};
        m_drawMode = SceneDrawMode::none;
    }

    void Scene::update(const TFrameTime ts, const std::vector<ApplicationEvent> &events) {
        // doing it here so selection box does not interfere with selection
        if (m_selectInSelectionBox) {
            selectEntitesInArea(m_selectionBoxStart, m_selectionBoxEnd);
            m_selectInSelectionBox = false;
        }

        m_camera->update(ts);

        for (auto &event : events) {
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
                    if (!m_isLeftMousePressed)
                        break;
                    m_isLeftMousePressed = false;
                }
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
        for (auto &entt : view)
            m_registry.emplace_or_replace<Components::SelectedComponent>(entt);
        const auto connectionView = m_registry.view<Canvas::Components::ConnectionComponent>();
        for (auto &entt : connectionView)
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
            for (auto &entt : view) {
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

            auto __ = m_cmdManager.execute<Commands::DelConnectionCommand, std::string>(connToDel);
        } else if (mainPageState->isKeyPressed(GLFW_KEY_F)) {
            const auto view = m_registry.view<Components::IdComponent,
                                        Components::SelectedComponent,
                                        Components::TransformComponent>();

            // pick the first one to focus. if many are selected
            for (auto &ent : view) {
                const auto &transform = view.get<Components::TransformComponent>(ent);
                m_camera->focusAtPoint(glm::vec2(transform.position), true);
                break;
            }
        } else if (mainPageState->isKeyPressed(GLFW_KEY_TAB)) {
            toggleSchematicView();
        }
    }

    void Scene::renderWithCamera(const std::shared_ptr<Camera> &camera) {
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

        beginScene();
        drawScene(camera);
        endScene();
    }

    void Scene::render() {
        renderWithCamera(m_camera);
    }

    void Scene::drawScene(const std::shared_ptr<Camera> &camera) {

        // Set the appropriate artist mode
        m_artistManager->setSchematicMode(m_isSchematicView);
        const auto artist = m_artistManager->getCurrentArtist();

        // Grid
        Renderer2D::Renderer::begin(camera);
        Renderer::grid({0.f, 0.f, -2.f}, camera->getSpan(), -1,
                       {
                           .minorColor = ViewportTheme::colors.gridMinorColor,
                           .majorColor = ViewportTheme::colors.gridMajorColor,
                           .axisXColor = ViewportTheme::colors.gridAxisXColor,
                           .axisYColor = ViewportTheme::colors.gridAxisYColor,
                       });
        Renderer2D::Renderer::end();

        // Connections
        Renderer2D::Renderer::begin(camera);
        const auto connectionsView = m_registry.view<Components::ConnectionComponent>();
        for (const auto entity : connectionsView) {
            artist->drawConnectionEntity(entity);
        }

        if (m_drawMode == SceneDrawMode::connection) {
            drawConnection();
        }
        Renderer2D::Renderer::end();

        // Components
        Renderer2D::Renderer::begin(camera);
        const auto simCompView = m_registry.view<
            Components::SimulationComponent,
            Components::TagComponent,
            Components::SpriteComponent,
            Components::TransformComponent>();
        for (const auto entity : simCompView) {
            artist->drawSimEntity(
                entity,
                simCompView.get<Components::TagComponent>(entity),
                simCompView.get<Components::TransformComponent>(entity),
                simCompView.get<Components::SpriteComponent>(entity),
                simCompView.get<Components::SimulationComponent>(entity));
        }

        const auto nonSimCompView = m_registry.view<Components::NSComponent>();
        for (const auto entity : nonSimCompView) {
            artist->drawNonSimEntity(entity);
        }

        Renderer2D::Renderer::end();

        if (m_drawMode == SceneDrawMode::selectionBox) {
            Renderer2D::Renderer::begin(camera);
            drawSelectionBox();
            Renderer2D::Renderer::end();
        }
    }

    void Scene::drawSelectionBox() {
        const auto start = toScenePos(m_selectionBoxStart);
        const auto end = toScenePos(m_mousePos);

        auto size = end - start;
        const auto pos = start + size / 2.f;
        size = glm::abs(size);

        Renderer2D::QuadRenderProperties props;
        props.borderColor = ViewportTheme::colors.selectionBoxBorder;
        props.borderSize = glm::vec4(1.f);
        Renderer2D::Renderer::quad(glm::vec3(pos, 9.f), size, ViewportTheme::colors.selectionBoxFill, 0, props);
    }

    void Scene::drawConnection() {
        const auto connStartEntity = getEntityWithUuid(m_connectionStartEntity);
        if (!m_registry.valid(connStartEntity)) {
            m_drawMode = SceneDrawMode::none;
            return;
        }

        const auto artist = m_artistManager->getCurrentArtist();
        artist->drawGhostConnection(connStartEntity, toScenePos(m_mousePos));
    }

    UUID Scene::createSimEntity(const UUID &simEngineEntt,
                                const std::shared_ptr<const SimEngine::ComponentDefinition> &comp, const glm::vec2 &pos) {
        const auto state = SimEngine::SimulationEngine::instance().getComponentState(simEngineEntt);
        const std::vector<UUID> inputSlotIds(state.inputStates.size()), outputSlotIds(state.outputStates.size());
        const UUID uuid;
        return createSimEntity(simEngineEntt, comp, pos, uuid, inputSlotIds, outputSlotIds);
    }

    UUID Scene::createSimEntity(const UUID &simEngineEntt, const std::shared_ptr<const SimEngine::ComponentDefinition> &comp, const glm::vec2 &pos,
                                UUID uuid, const std::vector<UUID> &inputSlotIds, const std::vector<UUID> &outputSlotIds) {
        auto entity = m_registry.create();
        const auto &idComp = m_registry.emplace<Components::IdComponent>(entity, uuid);
        auto &transformComp = m_registry.emplace<Components::TransformComponent>(entity);
        auto &sprite = m_registry.emplace<Components::SpriteComponent>(entity);
        auto &tag = m_registry.emplace<Components::TagComponent>(entity);
        auto &simComp = m_registry.emplace<Components::SimulationComponent>(entity);

        if (comp->type == SimEngine::ComponentType::INPUT) {
            m_registry.emplace<Components::SimulationInputComponent>(entity);
        } else if (comp->type == SimEngine::ComponentType::OUTPUT) {
            m_registry.emplace<Components::SimulationOutputComponent>(entity);
        } else if (comp->type == SimEngine::ComponentType::STATE_MONITOR) {
            m_registry.emplace<Components::SimulationStateMonitor>(entity);
        }

        tag.name = comp->name;
        tag.type.simCompType = comp->type;
        tag.isSimComponent = true;

        transformComp.position = glm::vec3(pos, getNextZCoord());

        if (comp->type == SimEngine::ComponentType::INPUT || comp->type == SimEngine::ComponentType::OUTPUT) {
            const glm::vec4 ioCompColor = glm::vec4(0.2f, 0.2f, 0.4f, 0.6f);
            sprite.color = ioCompColor;
            sprite.borderRadius = glm::vec4(8.f);
        } else {
            sprite.color = ViewportTheme::colors.componentBG;
            sprite.borderRadius = glm::vec4(6.f);
            sprite.headerColor = ViewportTheme::getCompHeaderColor(comp->type);
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

        simComp.type = comp->type;

        transformComp.scale = BaseArtist::calcCompSize(entity, simComp, comp->name);

        BESS_INFO("[Scene] Created entity {}", (uint64_t)entity);
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

    UUID Scene::createSlotEntity(const Components::SlotType type, const UUID &parent, const unsigned int idx) {
        return createSlotEntity(UUID{}, type, parent, idx);
    }

    UUID Scene::createSlotEntity(UUID uuid, const Components::SlotType type, const UUID &parent, const unsigned int idx) {
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

    const UUID &Scene::getUuidOfEntity(const entt::entity ent) {
        const auto &idComp = m_registry.get<Components::IdComponent>(ent);
        return idComp.uuid;
    }

    entt::entity Scene::getSceneEntityFromSimUuid(const UUID &uuid) const {
        for (const auto ent : m_registry.view<Components::SimulationComponent>()) {
            auto &comp = m_registry.get<Components::SimulationComponent>(ent);
            if (comp.simEngineEntity == uuid)
                return ent;
        }

        BESS_ASSERT(false, "Failed to find a Simulation Component in scene with this mapped uuid");
        throw std::runtime_error("Failed to find a Simulation Component in scene with this mapped uuid");
    }

    void Scene::resize(const glm::vec2 &size) {
        m_size = UI::UIMain::state.viewportSize;
        m_msaaFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        m_normalFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        m_shadowFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        m_placeHolderFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        m_camera->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
    }

    glm::vec2 Scene::getViewportMousePos(const glm::vec2 &mousePos) {
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

    void Scene::dragConnectionSegment(const entt::entity ent) {
        const auto view = m_registry.view<Components::TransformComponent,
                                    Components::ConnectionSegmentComponent,
                                    Components::SlotComponent,
                                    Components::ConnectionComponent>();

        const glm::vec2 newPos = toScenePos(m_mousePos);

        auto &comp = view.get<Components::ConnectionSegmentComponent>(ent);
        const auto artist = m_artistManager->getCurrentArtist();

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

    void Scene::moveConnection(const entt::entity ent, glm::vec2 &dPos) {

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
        auto mousePos_ = m_mousePos;
        mousePos_.y = UI::UIMain::state.viewportSize.y - mousePos_.y;
        const int x = static_cast<int>(mousePos_.x);
        const int y = static_cast<int>(mousePos_.y);
        int32_t hoverId = m_normalFramebuffer->readIntFromColAttachment(1, x, y);
        m_registry.clear<Components::HoveredEntityComponent>();
        const auto e = (entt::entity)hoverId;
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

        // reading the hoverid
        if (!m_isDragging || m_drawMode == SceneDrawMode::connection) {
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
                for (auto &ent : view) {
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
                    const float snapSize = 5.f;
                    newPos = glm::vec2(glm::round(newPos / snapSize)) * snapSize;
                    transformComp.position = {newPos, transformComp.position.z};
                }

                const auto connectionView = m_registry.view<Components::SelectedComponent, Components::ConnectionComponent>();
                for (auto &ent : connectionView) {
                    moveConnection(ent, m_dMousePos);
                }

                m_isDragging = true;
            }
        } else if (m_isMiddleMousePressed) {
            m_camera->incrementPos(-m_dMousePos);
        }
    }

    bool Scene::isCursorInViewport(const glm::vec2 &pos) {
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

    void Scene::deleteConnection(const UUID &entityUuid) {
        const auto entity = getEntityWithUuid(entityUuid);
        const auto &connComp = m_registry.get<Components::ConnectionComponent>(entity);

        const auto &slotCompA = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComp.inputSlot));
        const auto &slotCompB = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComp.outputSlot));

        const auto &simCompA = m_registry.get<Components::SimulationComponent>(getEntityWithUuid(slotCompA.parentId));
        const auto &simCompB = m_registry.get<Components::SimulationComponent>(getEntityWithUuid(slotCompB.parentId));

        const auto pinTypeA = slotCompA.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;
        const auto pinTypeB = slotCompB.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;

        SimEngine::SimulationEngine::instance().deleteConnection(simCompA.simEngineEntity, pinTypeA, slotCompA.idx, simCompB.simEngineEntity, pinTypeB, slotCompB.idx);

        deleteConnectionFromScene(entityUuid);
    }

    UUID Scene::generateBasicConnection(const entt::entity inputSlot, const entt::entity outputSlot) {
        const auto connEntt = m_registry.create();
        auto &idComp = m_registry.emplace<Components::IdComponent>(connEntt);
        auto &connComp = m_registry.emplace<Components::ConnectionComponent>(connEntt);
        m_registry.emplace<Components::SpriteComponent>(connEntt);
        connComp.inputSlot = getUuidOfEntity(inputSlot);
        connComp.outputSlot = getUuidOfEntity(outputSlot);

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

        const auto view = m_registry.view<Components::SlotComponent, Components::TransformComponent>();
        auto &inpSlotComp = view.get<Components::SlotComponent>(inputSlot);
        auto &outSlotComp = view.get<Components::SlotComponent>(outputSlot);
        const auto &inpParentTransform = view.get<Components::TransformComponent>(getEntityWithUuid(inpSlotComp.parentId));
        const auto artist = m_artistManager->getCurrentArtist();
        const auto inputSlotPos = artist->getSlotPos(inpSlotComp, inpParentTransform);
        const auto &slotComp = view.get<Components::SlotComponent>(outputSlot);
        const auto &parentTransform = view.get<Components::TransformComponent>(getEntityWithUuid(slotComp.parentId));
        const auto outputSlotPos = artist->getSlotPos(slotComp, parentTransform);

        const auto dX = outputSlotPos.x - inputSlotPos.x;

        connSegComp1.pos = glm::vec2(0, inputSlotPos.y);
        connSegComp2.pos = glm::vec2(inputSlotPos.x + (dX * 0.8f), 0);
        connSegComp3.pos = glm::vec2(0, outputSlotPos.y);

        inpSlotComp.connections.emplace_back(idComp.uuid);
        outSlotComp.connections.emplace_back(idComp.uuid);

        return idComp.uuid;
    }

    UUID Scene::connectSlots(const UUID startSlot, const UUID endSlot) {
        return generateBasicConnection(getEntityWithUuid(startSlot),
                                       getEntityWithUuid(endSlot));
    }

    UUID Scene::connectComponents(const UUID compIdA, const int slotIdxA, const bool isAInput, const UUID compIdB, const int slotIdxB) {
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

    void Scene::onRightMouse(const bool isPressed) {
        if (!isPressed) {
            return;
        }

        if (!isEntityValid(m_hoveredEntity) && m_lastCreatedComp.componentDefinition != nullptr) {
            const auto def = m_lastCreatedComp.componentDefinition;
            const Commands::AddCommandData cmdData = {
                .def = def,
                .inputCount = m_lastCreatedComp.inputCount,
                .outputCount = m_lastCreatedComp.outputCount,
                .pos = toScenePos(m_mousePos),
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
                const bool currentState = SimEngine::SimulationEngine::instance().getDigitalPinState(simComp.simEngineEntity, SimEngine::PinType::output, 0);

                auto _ = m_cmdManager.execute<Commands::SetInputCommand, std::string>(m_hoveredEntity, !currentState);
            }
        }
    }

    void Scene::onMiddleMouse(const bool isPressed) {
        m_isMiddleMousePressed = isPressed;
    }

    void Scene::onLeftMouse(const bool isPressed) {
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

    void Scene::toggleSelectComponent(const entt::entity ent) {
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
            const auto type = simEngine.getComponentType(comp.simEngineEntity);
            compData.def = catalogInstance.getComponentDefinition(type);
            compData.inputCount = comp.inputSlots.size();
            compData.outputCount = comp.outputSlots.size();
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

            if (comp.isSimComp()) {
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

        m_copiedComponents.clear();
    }

    void Scene::selectEntitesInArea(const glm::vec2 &start, const glm::vec2 &end) {
        m_registry.clear<Components::SelectedComponent>();
        auto size = end - start;
        const glm::vec2 pos = {std::min(start.x, end.x), std::max(start.y, end.y)};
        size = glm::abs(size);
        const int w = (int)size.x;
        const int h = (int)size.y;
        int x = pos.x, y = UI::UIMain::state.viewportSize.y - pos.y;

        auto ids = m_normalFramebuffer->readIntsFromColAttachment(1, x, y, w, h);

        if (ids.size() == 0)
            return;

        std::set<int> uniqueIds(ids.begin(), ids.end());
        std::unordered_map<int, bool> selected = {};

        for (auto &id : uniqueIds) {
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
    }

    void Scene::onMouseWheel(const double x, const double y) {
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

    void Scene::beginScene() const {
        static constexpr int value = -1;
        m_msaaFramebuffer->bind();
        m_msaaFramebuffer->clearColorAttachment<GL_FLOAT>(0, glm::value_ptr(ViewportTheme::colors.background));
        m_msaaFramebuffer->clearColorAttachment<GL_INT>(1, &value);
        m_msaaFramebuffer->clearColorAttachment<GL_FLOAT>(2, glm::value_ptr(ViewportTheme::colors.background));

        Gl::FrameBuffer::clearDepthStencilBuf();
    }

    void Scene::endScene() const {
        Gl::FrameBuffer::unbindAll();

        for (int i = 0; i < 2; i++) {
            m_msaaFramebuffer->bindColorAttachmentForRead(i);
            m_normalFramebuffer->bindColorAttachmentForDraw(i);
            Gl::FrameBuffer::blitColorBuffer(m_size.x, m_size.y);
        }

        Gl::FrameBuffer::unbindAll();
    }

    void Scene::saveScenePNG(const std::string &path) const {
        m_normalFramebuffer->saveColorAttachment(0, path);
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

    void Scene::setZoom(const float value) const {
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

    void Scene::setZCoord(const float value) {
        m_compZCoord = value + m_zIncrement;
    }

    SimEngine::Commands::CommandsManager &Scene::getCmdManager() {
        return m_cmdManager;
    }

    unsigned int Scene::getTextureId() const {
        return m_normalFramebuffer->getColorBufferTexId(0);
    }

    std::shared_ptr<Camera> Scene::getCamera() {
        return m_camera;
    }

    void Scene::setLastCreatedComp(LastCreatedComponent comp) {
        m_lastCreatedComp = std::move(comp);
    }

    void Scene::setSceneMode(const SceneMode mode) {
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
        return m_artistManager;
    }
} // namespace Bess::Canvas
