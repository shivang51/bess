#include "scene/scene.h"
#include "GLFW/glfw3.h"
#include "common/log.h"
#include "component_catalog.h"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "events/application_event.h"
#include "ext/matrix_transform.hpp"
#include "ext/vector_float2.hpp"
#include "ext/vector_float3.hpp"
#include "ext/vector_float4.hpp"
#include "gtc/type_ptr.hpp"
#include "pages/main_page/main_page_state.h"
#include "scene/artist.h"
#include "scene/renderer/renderer.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include "ui/ui.h"
#include "ui/ui_main/ui_main.h"
#include <cstdint>

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
        m_msaaFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments);

        attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::DEPTH32F_STENCIL8};
        m_shadowFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments);

        attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::RGBA_RGBA};
        m_placeHolderFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments);

        attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::R32I_REDI};
        m_normalFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments);

        m_placeHolderTexture = std::make_shared<Gl::Texture>("assets/images/crosshairs_tilesheet_white.png");
        m_placeHolderSubTexture = std::make_shared<Gl::SubTexture>(m_placeHolderTexture, glm::vec2(5.f, 5.f),
                                                                   glm::vec2(64.f, 64.f), 5, glm::vec2(1.f, 1.f));

        Artist::sceneRef = this;
    }

    void Scene::clear() {
        m_registry.clear();
        m_compZCoord = m_zIncrement;
        m_lastCreatedComp = {};
        m_copiedComponents = {};
        m_drawMode = SceneDrawMode::none;
    }

    void Scene::update(const std::vector<ApplicationEvent> &events) {
        // doing it here so selection box does not interfere with selection
        if (m_selectInSelectionBox) {
            selectEntitesInArea(m_selectionBoxStart, m_selectionBoxEnd);
            m_selectInSelectionBox = false;
        }

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
        auto view = m_registry.view<Canvas::Components::SimulationComponent>();
        for (auto &entt : view)
            m_registry.emplace_or_replace<Components::SelectedComponent>(entt);
        auto connectionView = m_registry.view<Canvas::Components::ConnectionComponent>();
        for (auto &entt : connectionView)
            m_registry.emplace_or_replace<Components::SelectedComponent>(entt);
    }

    void Scene::handleKeyboardShortcuts() {
        auto mainPageState = Pages::MainPageState::getInstance();
        if (mainPageState->isKeyPressed(GLFW_KEY_DELETE)) {
            auto view = m_registry.view<Components::IdComponent, Components::SelectedComponent>();
            for (auto &entt : view) {
                if (m_registry.all_of<Components::ConnectionComponent>(entt)) {
                    deleteConnection(getUuidOfEntity(entt));
                } else {
                    deleteEntity(getUuidOfEntity(entt));
                }
            }
        }

        if (mainPageState->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
            if (mainPageState->isKeyPressed(GLFW_KEY_A)) { // ctrl-a select all components
                selectAllEntities();
            } else if (mainPageState->isKeyPressed(GLFW_KEY_C)) { // ctrl-c copy selected components
                copySelectedComponents();
            } else if (mainPageState->isKeyPressed(GLFW_KEY_V)) { // ctrl-v generate copied components
                generateCopiedComponents();
            }
        }
    }

    void Scene::render() {
        auto hoveredEntity = getEntityWithUuid(m_hoveredEntity);

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

        Renderer2D::Renderer::begin(m_camera);
        Renderer::grid({0.f, 0.f, -2.f}, m_camera->getSpan(), -1, ViewportTheme::gridColor);
        Renderer2D::Renderer::end();

        Renderer2D::Renderer::begin(m_camera);
        if (m_drawMode == SceneDrawMode::connection) {
            drawConnection();
        }

        // draw connections
        auto connectionsView = m_registry.view<Components::ConnectionComponent>();
        for (auto entity : connectionsView) {
            Artist::drawConnectionEntity(entity);
        }

        // draw sim entities
        auto view = m_registry.view<Components::TagComponent>();
        for (auto entity : view) {
            Artist::drawEntity(entity);
        }

        Renderer2D::Renderer::end();

        if (m_drawMode == SceneDrawMode::selectionBox) {
            Renderer2D::Renderer::begin(m_camera);
            drawSelectionBox();
            Renderer2D::Renderer::end();
        }

        endScene();
    }

    void Scene::drawSelectionBox() {
        auto start = getNVPMousePos(m_selectionBoxStart);
        auto end = getNVPMousePos(m_mousePos);

        auto size = end - start;
        auto pos = start + size / 2.f;
        size = glm::abs(size);

        Renderer2D::QuadRenderProperties props;
        props.borderColor = ViewportTheme::selectionBoxBorderColor;
        props.borderSize = glm::vec4(1.f);
        Renderer2D::Renderer::quad(glm::vec3(pos, 9.f), size, ViewportTheme::selectionBoxFillColor, 0, props);
    }

    void Scene::drawConnection() {
        auto connStartEntity = getEntityWithUuid(m_connectionStartEntity);
        if (!m_registry.valid(connStartEntity)) {
            m_drawMode = SceneDrawMode::none;
            return;
        }

        Artist::drawGhostConnection(connStartEntity, getNVPMousePos(m_mousePos));
    }

    UUID Scene::createSimEntity(const UUID &simEngineEntt, const SimEngine::ComponentDefinition &comp, const glm::vec2 &pos) {
        auto entity = m_registry.create();
        auto &idComp = m_registry.emplace<Components::IdComponent>(entity);
        auto &transformComp = m_registry.emplace<Components::TransformComponent>(entity);
        auto &sprite = m_registry.emplace<Components::SpriteComponent>(entity);
        auto &tag = m_registry.emplace<Components::TagComponent>(entity);
        auto &simComp = m_registry.emplace<Components::SimulationComponent>(entity);

        if (comp.type == SimEngine::ComponentType::INPUT) {
            m_registry.emplace<Components::SimulationInputComponent>(entity);
        } else if (comp.type == SimEngine::ComponentType::OUTPUT) {
            m_registry.emplace<Components::SimulationOutputComponent>(entity);
        }

        tag.name = comp.name;
        tag.type.simCompType = comp.type;
        tag.isSimComponent = true;

        transformComp.position = glm::vec3(pos, getNextZCoord());
        transformComp.scale = glm::vec2(100.f, 100.f);

        if (comp.type == SimEngine::ComponentType::INPUT || comp.type == SimEngine::ComponentType::OUTPUT) {
            glm::vec4 ioCompColor = glm::vec4(0.2f, 0.2f, 0.4f, 0.6f);
            sprite.color = ioCompColor;
            sprite.borderRadius = glm::vec4(8.f);
        } else {
            sprite.color = ViewportTheme::componentBGColor;
            sprite.borderRadius = glm::vec4(6.f);
        }

        sprite.borderSize = glm::vec4(1.f);
        sprite.borderColor = ViewportTheme::componentBorderColor;

        simComp.simEngineEntity = simEngineEntt;

        auto state = SimEngine::SimulationEngine::instance().getComponentState(simEngineEntt);

        for (int i = 0; i < state.inputStates.size(); i++) {
            simComp.inputSlots.emplace_back(createSlotEntity(Components::SlotType::digitalInput, idComp.uuid, i));
        }

        for (int i = 0; i < state.outputStates.size(); i++) {
            simComp.outputSlots.emplace_back(createSlotEntity(Components::SlotType::digitalOutput, idComp.uuid, i));
        }
        BESS_INFO("[Scene] Created entity {}", (uint64_t)entity);
        return idComp.uuid;
    }

    UUID Scene::createNonSimEntity(const Canvas::Components::NSComponent &comp, const glm::vec2 &pos) {
        using namespace Canvas::Components;
        auto entity = m_registry.create();
        auto &idComp = m_registry.emplace<Components::IdComponent>(entity);
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
                textComp.color = ViewportTheme::textColor;
            } break;
            default:
                break;
            }
        }
        BESS_INFO("[Scene] Created Non simulation entity {}", (uint64_t)entity);
        return idComp.uuid;
    }

    void Scene::deleteEntity(const UUID &entUuid) {
        auto ent = getEntityWithUuid(entUuid);
        auto hoveredEntity = getEntityWithUuid(m_hoveredEntity);
        if (ent == hoveredEntity)
            hoveredEntity = entt::null;

        if (m_registry.all_of<Components::SimulationComponent>(ent)) {
            auto &simComp = m_registry.get<Components::SimulationComponent>(ent);

            // take care of connections
            auto view = m_registry.view<Components::ConnectionComponent>();
            for (auto connEntt : view) {
                auto &connComp = view.get<Components::ConnectionComponent>(connEntt);
                auto parentA = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComp.inputSlot)).parentId;
                auto parentB = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComp.outputSlot)).parentId;
                if (parentA != entUuid && parentB != entUuid)
                    continue;
                BESS_INFO("[Scene] Deleted connection {}", (uint64_t)connEntt);
                m_registry.destroy(connEntt);
            }

            // take care of slots
            for (auto slot : simComp.inputSlots)
                m_registry.destroy(getEntityWithUuid(slot));

            for (auto slot : simComp.outputSlots)
                m_registry.destroy(getEntityWithUuid(slot));

            // remove from simlation engine
            SimEngine::SimulationEngine::instance().deleteComponent(simComp.simEngineEntity);
        }

        // remove from registry
        m_registry.destroy(ent);
        BESS_INFO("[Scene] Deleted entity {}", (uint64_t)ent);
    }

    UUID Scene::createSlotEntity(Components::SlotType type, const UUID &parent, unsigned int idx) {
        auto entity = m_registry.create();
        auto &idComp = m_registry.emplace<Components::IdComponent>(entity);
        auto &transform = m_registry.emplace<Components::TransformComponent>(entity);
        auto &sprite = m_registry.emplace<Components::SpriteComponent>(entity);
        auto &slot = m_registry.emplace<Components::SlotComponent>(entity);

        transform.scale = glm::vec2(20.f);
        sprite.color = ViewportTheme::stateLowColor;
        sprite.borderColor = ViewportTheme::componentBorderColor;
        sprite.borderSize = glm::vec4(10.f);

        slot.parentId = parent;
        slot.slotType = type;
        slot.idx = idx;

        return idComp.uuid;
    }

    const glm::vec2 &Scene::getSize() {
        return m_size;
    }

    entt::entity Scene::getEntityWithUuid(const UUID &uuid) {
        auto view = m_registry.view<Components::IdComponent>();
        for (const auto &ent : view) {
            auto &idComp = view.get<Components::IdComponent>(ent);
            if (idComp.uuid == uuid)
                return ent;
        }
        return entt::null;
    }

    const UUID &Scene::getUuidOfEntity(entt::entity ent) {
        auto &idComp = m_registry.get<Components::IdComponent>(ent);
        return idComp.uuid;
    }

    entt::entity Scene::getSceneEntityFromSimUuid(const UUID &uuid) const {
        for (auto ent : m_registry.view<Components::SimulationComponent>()) {
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

    glm::vec2 Scene::getNVPMousePos(const glm::vec2 &mousePos) {
        glm::vec2 pos = mousePos;

        const auto cameraPos = m_camera->getPos();
        glm::mat4 tansform = glm::translate(glm::mat4(1.f), glm::vec3(cameraPos.x, cameraPos.y, 0.f));
        auto zoom = m_camera->getZoom();
        tansform = glm::scale(tansform, glm::vec3(1.f / zoom, 1.f / zoom, 1.f));

        pos = glm::vec2(tansform * glm::vec4(pos.x, pos.y, 0.f, 1.f));
        auto span = m_camera->getSpan() / 2.f;
        pos -= glm::vec2({span.x, span.y});
        return pos;
    }

    void Scene::dragConnectionSegment(entt::entity ent, const glm::vec2 &dPos) {
        auto &comp = m_registry.get<Components::ConnectionSegmentComponent>(ent);

        if (comp.isHead() || comp.isTail()) {
            auto newEntt = m_registry.create();
            auto &idComp = m_registry.emplace<Components::IdComponent>(newEntt);
            auto &compNew = m_registry.emplace<Components::ConnectionSegmentComponent>(newEntt);
            compNew.parent = comp.parent;

            glm::vec2 slotPos;

            if (comp.isHead()) {
                comp.prev = idComp.uuid;
                compNew.next = getUuidOfEntity(ent);
                auto &connComponent = m_registry.get<Components::ConnectionComponent>(getEntityWithUuid(comp.parent));
                auto &slotComponent = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComponent.inputSlot));
                connComponent.segmentHead = idComp.uuid;
                slotPos = Artist::getSlotPos(slotComponent);
            } else {
                comp.next = idComp.uuid;
                compNew.prev = getUuidOfEntity(ent);
                auto &connComponent = m_registry.get<Components::ConnectionComponent>(getEntityWithUuid(comp.parent));
                auto &slotComponent = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComponent.outputSlot));
                slotPos = Artist::getSlotPos(slotComponent);
            }

            if (comp.pos.x == 0) // if current is vertical
                compNew.pos.x = slotPos.x;
            else
                compNew.pos.y = slotPos.y;
        }

        // glm::vec2 newPos = getNVPMousePos(m_mousePos);
        // if (!m_isDragging) {
        //     auto offset = newPos - comp.pos;
        //     m_dragOffsets[ent] = offset;
        // }
        // newPos -= m_dragOffsets[ent];
        // newPos = glm::vec2({(int)(std::round(newPos.x) / 4), (int)(std::round(newPos.y) / 4)}) * 4.f;

        if (comp.pos.x == 0) {
            comp.pos.y += dPos.y;
        } else if (comp.pos.y == 0) {
            comp.pos.x += dPos.x;
        }
    }

    void Scene::moveConnection(entt::entity ent, glm::vec2 &dPos) {

        auto &connComp = m_registry.get<Components::ConnectionComponent>(ent);
        auto seg = connComp.segmentHead;

        while (seg != UUID::null) {
            auto &segComp = m_registry.get<Components::ConnectionSegmentComponent>(getEntityWithUuid(seg));

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
        int x = static_cast<int>(mousePos_.x);
        int y = static_cast<int>(mousePos_.y);
        int32_t hoverId = m_normalFramebuffer->readIntFromColAttachment(1, x, y);
        m_registry.clear<Components::HoveredEntityComponent>();
        auto e = (entt::entity)hoverId;
        if (m_registry.valid(e)) {
            m_registry.emplace<Components::HoveredEntityComponent>(e);
            m_hoveredEntity = getUuidOfEntity(e);
        } else {
            m_hoveredEntity = UUID::null;
        }
    }

    void Scene::onMouseMove(const glm::vec2 &pos) {
        m_dMousePos = getNVPMousePos(pos) - getNVPMousePos(m_mousePos);
        m_mousePos = pos;

        // reading the hoverid
        if (!m_isDragging || m_drawMode == SceneDrawMode::connection) {
            updateHoveredId();
        }

        if (m_isLeftMousePressed && m_drawMode == SceneDrawMode::none) {
            auto hoveredEntity = getEntityWithUuid(m_hoveredEntity);
            auto selectedComponentsSize = m_registry.view<Components::SelectedComponent>()->size();
            if (!m_registry.valid(hoveredEntity) && selectedComponentsSize == 0) {
                m_drawMode = SceneDrawMode::selectionBox;
                m_selectionBoxStart = m_mousePos;
            } else if (selectedComponentsSize == 1 && m_registry.valid(hoveredEntity) && m_registry.all_of<Components::ConnectionSegmentComponent>(hoveredEntity)) {
                dragConnectionSegment(hoveredEntity, m_dMousePos);
                m_isDragging = true;
            } else {
                auto view = m_registry.view<Components::SelectedComponent, Components::TransformComponent>();
                for (auto &ent : view) {
                    auto &transformComp = view.get<Components::TransformComponent>(ent);
                    glm::vec2 newPos = getNVPMousePos(m_mousePos);
                    if (!m_isDragging) {
                        auto offset = newPos - glm::vec2(transformComp.position);
                        m_dragOffsets[ent] = offset;
                    }
                    newPos -= m_dragOffsets[ent];
                    newPos = glm::vec2({(int)(std::round(newPos.x) / 4), (int)(std::round(newPos.y) / 4)}) * 4.f;
                    transformComp.position = {newPos, transformComp.position.z};
                }

                auto connectionView = m_registry.view<Components::SelectedComponent, Components::ConnectionComponent>();
                for (auto &ent : connectionView) {
                    moveConnection(ent, m_dMousePos);
                }

                m_isDragging = true;
            }
        } else if (m_isMiddleMousePressed) {
            glm::vec2 dPos = m_dMousePos;
            dPos *= m_camera->getZoom() * -1;
            m_camera->incrementPos(dPos);
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

    void Scene::deleteConnection(const UUID &entityUuid) {
        auto entity = getEntityWithUuid(entityUuid);
        m_hoveredEntity = UUID::null;
        auto &connComp = m_registry.get<Components::ConnectionComponent>(entity);

        auto &slotCompA = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComp.inputSlot));
        auto &slotCompB = m_registry.get<Components::SlotComponent>(getEntityWithUuid(connComp.outputSlot));

        auto &simCompA = m_registry.get<Components::SimulationComponent>(getEntityWithUuid(slotCompA.parentId));
        auto &simCompB = m_registry.get<Components::SimulationComponent>(getEntityWithUuid(slotCompB.parentId));

        auto pinTypeA = slotCompA.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;
        auto pinTypeB = slotCompB.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;

        SimEngine::SimulationEngine::instance().deleteConnection(simCompA.simEngineEntity, pinTypeA, slotCompA.idx, simCompB.simEngineEntity, pinTypeB, slotCompB.idx);

        auto segEntt = connComp.segmentHead;
        while (segEntt != UUID::null) {
            auto connSegComp = m_registry.get<Components::ConnectionSegmentComponent>(getEntityWithUuid(segEntt));
            m_registry.destroy(getEntityWithUuid(segEntt));
            segEntt = connSegComp.next;
        }

        m_registry.destroy(entity);

        BESS_INFO("[Scene] Deleted connection");
    }

    void Scene::generateBasicConnection(entt::entity inputSlot, entt::entity outputSlot) {
        auto connEntt = m_registry.create();
        auto &idComp = m_registry.emplace<Components::IdComponent>(connEntt);
        auto &connComp = m_registry.emplace<Components::ConnectionComponent>(connEntt);
        m_registry.emplace<Components::SpriteComponent>(connEntt);
        connComp.inputSlot = getUuidOfEntity(inputSlot);
        connComp.outputSlot = getUuidOfEntity(outputSlot);

        auto connSeg1 = m_registry.create();
        auto connSeg2 = m_registry.create();
        auto connSeg3 = m_registry.create();

        auto &idComp1 = m_registry.emplace<Components::IdComponent>(connSeg1);
        auto &idComp2 = m_registry.emplace<Components::IdComponent>(connSeg2);
        auto &idComp3 = m_registry.emplace<Components::IdComponent>(connSeg3);

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

        auto inputSlotPos = Artist::getSlotPos(m_registry.get<Components::SlotComponent>(inputSlot));
        auto outputSlotPos = Artist::getSlotPos(m_registry.get<Components::SlotComponent>(outputSlot));

        auto dX = outputSlotPos.x - inputSlotPos.x;

        connSegComp1.pos = glm::vec2(0, inputSlotPos.y);
        connSegComp2.pos = glm::vec2(inputSlotPos.x + (dX * 0.8f), 0);
        connSegComp3.pos = glm::vec2(0, outputSlotPos.y);
    }

    void Scene::connectSlots(entt::entity startSlot, entt::entity endSlot) {
        auto &startSlotComp = m_registry.get<Components::SlotComponent>(startSlot);
        auto &endSlotComp = m_registry.get<Components::SlotComponent>(endSlot);

        auto startSimParent = m_registry.get<Components::SimulationComponent>(getEntityWithUuid(startSlotComp.parentId)).simEngineEntity;
        auto endSimParent = m_registry.get<Components::SimulationComponent>(getEntityWithUuid(endSlotComp.parentId)).simEngineEntity;

        auto startPinType = startSlotComp.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;
        auto dstPinType = endSlotComp.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;

        auto successful = SimEngine::SimulationEngine::instance().connectComponent(startSimParent, startSlotComp.idx, startPinType,
                                                                                   endSimParent, endSlotComp.idx, dstPinType);

        if (!successful)
            return;

        if (startSlotComp.slotType == Components::SlotType::digitalInput) {
            generateBasicConnection(startSlot, endSlot);
        } else {
            generateBasicConnection(endSlot, startSlot);
        }
    }

    bool Scene::isEntityValid(const UUID &uuid) {
        return uuid != UUID::null && m_registry.valid(getEntityWithUuid(uuid));
    }

    void Scene::onRightMouse(bool isPressed) {
        if (!isPressed) {
            return;
        }

        if (!isEntityValid(m_hoveredEntity) && m_lastCreatedComp.componentDefinition != nullptr) {
            auto simEntt = SimEngine::SimulationEngine::instance().addComponent(m_lastCreatedComp.componentDefinition->type,
                                                                                m_lastCreatedComp.inputCount,
                                                                                m_lastCreatedComp.outputCount);
            createSimEntity(simEntt, *m_lastCreatedComp.componentDefinition, getNVPMousePos(m_mousePos));
        }

        if (isEntityValid(m_hoveredEntity)) {
            auto hoveredEntity = getEntityWithUuid(m_hoveredEntity);
            if (m_registry.all_of<Components::SimulationInputComponent>(hoveredEntity)) {
                auto &simComp = m_registry.get<Components::SimulationComponent>(hoveredEntity);
                bool currentState = SimEngine::SimulationEngine::instance().getDigitalPinState(simComp.simEngineEntity, SimEngine::PinType::output, 0);
                SimEngine::SimulationEngine::instance().setDigitalInput(simComp.simEngineEntity, !currentState);
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
            }
            return;
        }
        auto hoveredEntity = getEntityWithUuid(m_hoveredEntity);

        // toggeling selection of hovered entity on click
        if (m_registry.valid(hoveredEntity)) {
            if (m_registry.all_of<Components::SlotComponent>(hoveredEntity)) {
                m_registry.clear<Components::SelectedComponent>();
                if (m_drawMode == SceneDrawMode::none) {
                    m_connectionStartEntity = m_hoveredEntity;
                    m_drawMode = SceneDrawMode::connection;
                } else if (m_drawMode == SceneDrawMode::connection) {
                    m_drawMode = SceneDrawMode::none;
                    connectSlots(getEntityWithUuid(m_connectionStartEntity), hoveredEntity);
                }
            } else {
                if (m_sceneMode != SceneMode::move &&
                    !Pages::MainPageState::getInstance()->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
                    m_registry.clear<Components::SelectedComponent>();
                }

                if (m_registry.all_of<Components::ConnectionSegmentComponent>(hoveredEntity)) {
                    auto &segComp = m_registry.get<Components::ConnectionSegmentComponent>(hoveredEntity);
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
        auto ent = getEntityWithUuid(uuid);
        toggleSelectComponent(ent);
    }

    void Scene::toggleSelectComponent(entt::entity ent) {
        bool isSelected = m_registry.all_of<Canvas::Components::SelectedComponent>(ent);
        if (isSelected)
            m_registry.remove<Canvas::Components::SelectedComponent>(ent);
        else
            m_registry.emplace<Canvas::Components::SelectedComponent>(ent);
    }

    void Scene::copySelectedComponents() {
        m_copiedComponents.clear();

        auto view = m_registry.view<Components::SelectedComponent, Components::SimulationComponent>();

        for (auto entt : view) {
            auto &comp = view.get<Components::SimulationComponent>(entt);
            m_copiedComponents.emplace_back(SimEngine::SimulationEngine::instance().getComponentType(comp.simEngineEntity));
        }
    }

    void Scene::generateCopiedComponents() {
        auto &simEngineInstance = SimEngine::SimulationEngine::instance();
        auto &catalogInstance = SimEngine::ComponentCatalog::instance();
        auto pos = getCameraPos();
        for (auto &compType : m_copiedComponents) {
            auto simEngineEntity = simEngineInstance.addComponent(compType);
            auto def = catalogInstance.getComponentDefinition(compType);
            createSimEntity(simEngineEntity, *def, pos);
            pos += glm::vec2(50.f, 50.f);
        }
    }

    void Scene::selectEntitesInArea(const glm::vec2 &start, const glm::vec2 &end) {
        m_registry.clear<Components::SelectedComponent>();
        auto size = end - start;
        glm::vec2 pos = {std::min(start.x, end.x), std::max(start.y, end.y)};
        size = glm::abs(size);
        int w = (int)size.x;
        int h = (int)size.y;
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
            if (auto *segComp = m_registry.try_get<Components::ConnectionSegmentComponent>(entt)) {
                entt = getEntityWithUuid(segComp->parent);
                isConnection = true;
            }

            if (selected.find(id) != selected.end())
                continue;

            if (isConnection || m_registry.any_of<Components::SimulationComponent, Components::NSComponent>(entt)) {
                m_registry.emplace_or_replace<Components::SelectedComponent>(entt);
            }

            selected[id] = true;
        }
    }

    void Scene::onMouseWheel(double x, double y) {
        if (!isCursorInViewport(m_mousePos))
            return;

        auto mainPageState = Pages::MainPageState::getInstance();
        if (mainPageState->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
            const float delta = static_cast<float>(y) * 0.1f;
            m_camera->incrementZoom(delta);
            /*UI::UIMain::state.cameraZoom += delta;*/
            /*if (UI::UIMain::state.cameraZoom < Camera::zoomMin) {*/
            /*    UI::UIMain::state.cameraZoom = Camera::zoomMin;*/
            /*} else if (UI::UIMain::state.cameraZoom > Camera::zoomMax) {*/
            /*    UI::UIMain::state.cameraZoom = Camera::zoomMax;*/
            /*}*/
        } else {
            glm::vec2 dPos = {x, y};
            dPos *= 10 / m_camera->getZoom() * -1;
            m_camera->incrementPos(dPos);
        }
    }

    void Scene::beginScene() {
        static int value = -1;
        /*static glm::vec4 col = glm::vec4(0.0);*/
        /*m_shadowFramebuffer->clearColorAttachment<GL_FLOAT>(0, glm::value_ptr(col));*/
        /*Gl::FrameBuffer::clearDepthStencilBuf();*/
        /*m_normalFramebuffer->clearColorAttachment<GL_FLOAT>(0, glm::value_ptr(col));*/
        /*m_placeHolderFramebuffer->clearColorAttachment<GL_FLOAT>(0, glm::value_ptr(col));*/
        /*m_placeHolderFramebuffer->clearColorAttachment<GL_FLOAT>(1, glm::value_ptr(col));*/

        m_msaaFramebuffer->bind();
        m_msaaFramebuffer->clearColorAttachment<GL_FLOAT>(0, glm::value_ptr(ViewportTheme::backgroundColor));
        m_msaaFramebuffer->clearColorAttachment<GL_INT>(1, &value);
        m_msaaFramebuffer->clearColorAttachment<GL_FLOAT>(2, glm::value_ptr(ViewportTheme::backgroundColor));

        Gl::FrameBuffer::clearDepthStencilBuf();
    }

    void Scene::endScene() {
        Gl::FrameBuffer::unbindAll();
        /*auto span = m_camera->getSpan();*/
        /**/
        /*// from msaa to normal*/
        /*// -- normal color*/
        /*m_msaaFramebuffer->bindColorAttachmentForRead(0);*/
        /*m_placeHolderFramebuffer->bindColorAttachmentForDraw(0);*/
        /*Gl::FrameBuffer::blitColorBuffer(m_size.x, m_size.y);*/
        /*// -- shadow mask*/
        /*m_msaaFramebuffer->bindColorAttachmentForRead(2);*/
        /*m_placeHolderFramebuffer->bindColorAttachmentForDraw(1);*/
        /*Gl::FrameBuffer::blitColorBuffer(m_size.x, m_size.y);*/
        /**/
        /*// shadow pass*/
        /*m_placeHolderFramebuffer->bindColorAttachmentTexture(1);*/
        /*m_shadowFramebuffer->bind();*/
        /*Renderer2D::Renderer::doShadowRenderPass(span.x, span.y);*/
        /**/
        /*m_shadowFramebuffer->bindColorAttachmentForRead(0);*/
        /*m_placeHolderFramebuffer->bindColorAttachmentForDraw(1);*/
        /*Gl::FrameBuffer::blitColorBuffer(m_size.x, m_size.y);*/
        /**/
        /*// rendering to normal for display*/
        /*m_shadowFramebuffer->clearColorAttachment<GL_FLOAT>(0, glm::value_ptr(ViewportTheme::backgroundColor));*/
        /*Gl::FrameBuffer::clearDepthStencilBuf();*/
        /*m_placeHolderFramebuffer->bindColorAttachmentTexture(0, 0); // color*/
        /*m_placeHolderFramebuffer->bindColorAttachmentTexture(1, 1); // shadow*/
        /*m_shadowFramebuffer->bind();*/
        /*Renderer2D::Renderer::doCompositeRenderPass(span.x, span.y);*/
        /*GL_CHECK(glActiveTexture(GL_TEXTURE0));*/
        /*GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));*/
        /**/
        /*m_shadowFramebuffer->bindColorAttachmentForRead(0);*/
        /*m_normalFramebuffer->bindColorAttachmentForDraw(0);*/
        /*Gl::FrameBuffer::blitColorBuffer(m_size.x, m_size.y);*/

        for (int i = 0; i < 2; i++) {
            m_msaaFramebuffer->bindColorAttachmentForRead(i);
            m_normalFramebuffer->bindColorAttachmentForDraw(i);
            Gl::FrameBuffer::blitColorBuffer(m_size.x, m_size.y);
        }

        Gl::FrameBuffer::unbindAll();
    }

    void Scene::saveScenePNG(const std::string &path) {
        m_normalFramebuffer->saveColorAttachment(0, path);
    }

    glm::vec2 Scene::getCameraPos() {
        return m_camera->getPos();
    }

    float Scene::getCameraZoom() {
        return m_camera->getZoom();
    }

    void Scene::setZoom(float value) {
        m_camera->setZoom(value);
    }

    entt::registry &Scene::getEnttRegistry() {
        return m_registry;
    }

    float Scene::getNextZCoord() {
        float z = m_compZCoord;
        m_compZCoord += m_zIncrement;
        return z;
    }

    void Scene::setZCoord(float value) {
        m_compZCoord = value + m_zIncrement;
    }

    unsigned int Scene::getTextureId() {
        return m_normalFramebuffer->getColorBufferTexId(0);
    }

    std::shared_ptr<Camera> Scene::getCamera() {
        return m_camera;
    }

    void Scene::setLastCreatedComp(const LastCreatedComponent &comp) {
        m_lastCreatedComp = comp;
    }

    void Scene::setSceneMode(SceneMode mode) {
        m_sceneMode = mode;
    }
    SceneMode Scene::getSceneMode() {
        return m_sceneMode;
    }
} // namespace Bess::Canvas
