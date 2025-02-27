#include "scene/scene.h"
#include "GLFW/glfw3.h"
#include "application_state.h"
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
#include "scene/components/components.h"
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
        m_size = glm::vec2(800.f / 600.f, 1.f);
        m_camera = std::make_shared<Camera>(m_size.x, m_size.y);
        std::vector<Gl::FBAttachmentType> attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::R32I_REDI, Gl::DEPTH32F_STENCIL8};
        m_msaaFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments, true);

        attachments = {Gl::FBAttachmentType::RGB_RGB, Gl::FBAttachmentType::R32I_REDI};
        m_normalFramebuffer = std::make_unique<Gl::FrameBuffer>(m_size.x, m_size.y, attachments);
        m_registry.clear();

        Artist::sceneRef = this;
    }

    unsigned int Scene::getTextureId() {
        return m_normalFramebuffer->getColorBufferTexId(0);
    }

    void Scene::drawConnection() {
        if (!m_registry.valid(m_connectionStartEntity)) {
            m_drawMode = ScenDrawMode::none;
            return;
        }

        Artist::drawGhostConnection(m_connectionStartEntity, getNVPMousePos(m_mousePos));
    }

    void Scene::render() {
        if (m_registry.valid(m_hoveredEntiy) && m_registry.all_of<Components::SlotComponent>(m_hoveredEntiy)) {
            UI::setCursorPointer();
        }

        beginScene();
        Renderer::grid({0.f, 0.f, -2.f}, m_camera->getSpan(), -1, ViewportTheme::gridColor);

        auto connectionsView = m_registry.view<Components::ConnectionComponent>();
        for (auto entity : connectionsView) {
            Artist::drawConnectionEntity(entity);
        }

        switch (m_drawMode) {
        case ScenDrawMode::connection:
            drawConnection();
            break;
        default:
            break;
        }

        auto view = m_registry.view<Components::SimulationComponent>();
        for (auto entity : view) {
            Artist::drawSimEntity(entity);
        }

        endScene();
    }

    entt::entity Scene::createSimEntity(entt::entity simEngineEntt, std::string name, int inputs, int outputs) {
        auto entity = m_registry.create();
        auto &transformComp = m_registry.emplace<Components::TransformComponent>(entity);
        auto &sprite = m_registry.emplace<Components::SpriteComponent>(entity);
        auto &tag = m_registry.emplace<Components::TagComponent>(entity);
        auto &simComp = m_registry.emplace<Components::SimulationComponent>(entity);

        tag.name = name;
        tag.id = (uint64_t)entity;

        glm::mat4 transform = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, getNextZCoord()));
        transform = glm::scale(transform, glm::vec3(150.f, 150.f, 1.f));

        transformComp = transform;

        sprite.color = ViewportTheme::componentBGColor;
        sprite.borderRadius = glm::vec4(16.f);
        sprite.borderSize = glm::vec4(1.f);
        sprite.borderColor = ViewportTheme::componentBorderColor;

        simComp.simEngineEntity = simEngineEntt;

        for (int i = 0; i < inputs; i++) {
            simComp.inputSlots.emplace_back(createSlotEntity(Components::SlotType::digitalInput, entity, i));
        }

        for (int i = 0; i < outputs; i++) {
            simComp.outputSlots.emplace_back(createSlotEntity(Components::SlotType::digitalOutput, entity, i));
        }
        std::cout << "[Scene] Created entity " << (uint64_t)entity << std::endl;
        return entity;
    }

    void Scene::deleteEntity(entt::entity ent) {
        if (ent == m_hoveredEntiy)
            m_hoveredEntiy = entt::null;

        auto &simComp = m_registry.get<Components::SimulationComponent>(ent);

        // take care of connections
        auto view = m_registry.view<Components::ConnectionComponent>();
        for (auto connEntt : view) {
            auto &connComp = view.get<Components::ConnectionComponent>(connEntt);
            auto parentA = (entt::entity)m_registry.get<Components::SlotComponent>(connComp.slotAEntity).parentId;
            auto parentB = (entt::entity)m_registry.get<Components::SlotComponent>(connComp.slotBEntity).parentId;
            if (parentA != ent && parentB != ent)
                continue;
            std::cout << "[Scene] Deleted connection " << (uint64_t)connEntt << std::endl;
            m_registry.destroy(connEntt);
        }

        // take care of slots
        for (auto slot : simComp.inputSlots)
            m_registry.destroy(slot);

        for (auto slot : simComp.outputSlots)
            m_registry.destroy(slot);

        // remove from simlation engine
        SimEngine::SimulationEngine::instance().deleteComponent(simComp.simEngineEntity);

        // remove from registry
        m_registry.destroy(ent);
        std::cout << "[Scene] Deleted entity " << (uint64_t)ent << std::endl;
    }

    entt::entity Scene::createSlotEntity(Components::SlotType type, entt::entity parent, uint idx) {
        auto entity = m_registry.create();
        auto &transform = m_registry.emplace<Components::TransformComponent>(entity);
        auto &sprite = m_registry.emplace<Components::SpriteComponent>(entity);
        auto &slot = m_registry.emplace<Components::SlotComponent>(entity);

        transform.scale(glm::vec2(20.f));
        sprite.color = ViewportTheme::stateLowColor;
        sprite.borderColor = ViewportTheme::componentBorderColor;
        sprite.borderSize = glm::vec4(10.f);

        slot.parentId = (uint64_t)parent;
        slot.slotType = type;
        slot.idx = idx;

        return entity;
    }

    const glm::vec2 &Scene::getSize() {
        return m_size;
    }

    void Scene::resize(const glm::vec2 &size) {
        m_size = UI::UIMain::state.viewportSize;
        m_msaaFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        m_normalFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
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
        tansform = glm::scale(tansform, glm::vec3(1.f / UI::UIMain::state.cameraZoom, 1.f / UI::UIMain::state.cameraZoom, 1.f));

        pos = glm::vec2(tansform * glm::vec4(pos.x, pos.y, 0.f, 1.f));
        auto span = m_camera->getSpan() / 2.f;
        pos -= glm::vec2({span.x, span.y});
        return pos;
    }

    void Scene::onMouseMove(const glm::vec2 &pos) {
        auto dPos = getNVPMousePos(pos) - getNVPMousePos(m_mousePos);
        m_mousePos = pos;

        // reading the hoverid
        {
            auto m_mousePos_ = pos;
            m_mousePos_.y = UI::UIMain::state.viewportSize.y - m_mousePos_.y;
            int x = static_cast<int>(m_mousePos_.x);
            int y = static_cast<int>(m_mousePos_.y);
            int32_t hoverId = m_normalFramebuffer->readIntFromColAttachment(1, x, y);
            m_hoveredEntiy = (entt::entity)hoverId;
        }

        if (m_isLeftMousePressed) {
            auto view = m_registry.view<Components::SelectedComponent, Components::TransformComponent>();

            for (auto &ent : view) {
                auto &transformComp = view.get<Components::TransformComponent>(ent);
                auto dPos_ = glm::vec3(dPos, 0.f);
                transformComp.translate(transformComp.getPosition() + dPos_);
            }
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

    void Scene::connectSlots(entt::entity startSlot, entt::entity endSlot) {
        auto &startSlotComp = m_registry.get<Components::SlotComponent>(startSlot);
        auto &endSlotComp = m_registry.get<Components::SlotComponent>(endSlot);

        auto startSimParent = m_registry.get<Components::SimulationComponent>((entt::entity)startSlotComp.parentId).simEngineEntity;
        auto endSimParent = m_registry.get<Components::SimulationComponent>((entt::entity)endSlotComp.parentId).simEngineEntity;

        auto startPinType = startSlotComp.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;
        auto dstPinType = endSlotComp.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;

        auto successful = SimEngine::SimulationEngine::instance().connectComponent(startSimParent, startSlotComp.idx, startPinType,
                                                                                   endSimParent, endSlotComp.idx, dstPinType);

        if (!successful)
            return;

        auto connEntt = m_registry.create();
        auto &connComp = m_registry.emplace<Components::ConnectionComponent>(connEntt);

        connComp.slotAEntity = startSlot;
        connComp.slotBEntity = endSlot;
    }

    void Scene::onLeftMouse(bool isPressed) {
        m_isLeftMousePressed = isPressed;
        if (!isPressed)
            return;

        // toggeling selection of hovered entity on click
        if (m_registry.valid(m_hoveredEntiy)) {
            if (m_registry.all_of<Components::SlotComponent>(m_hoveredEntiy)) {
                m_registry.clear<Components::SelectedComponent>();
                if (m_drawMode == ScenDrawMode::none) {
                    m_connectionStartEntity = m_hoveredEntiy;
                    m_drawMode = ScenDrawMode::connection;
                } else if (m_drawMode == ScenDrawMode::connection) {
                    m_drawMode = ScenDrawMode::none;
                    connectSlots(m_connectionStartEntity, m_hoveredEntiy);
                }
            } else {
                bool isSelected = m_registry.all_of<Canvas::Components::SelectedComponent>(m_hoveredEntiy);
                if (!Pages::MainPageState::getInstance()->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
                    m_registry.clear<Components::SelectedComponent>();
                    isSelected = false;
                }

                if (isSelected)
                    m_registry.erase<Canvas::Components::SelectedComponent>(m_hoveredEntiy);
                else
                    m_registry.emplace<Canvas::Components::SelectedComponent>(m_hoveredEntiy);
            }

        } else { // deselecting all when clicking outside
            m_registry.clear<Components::SelectedComponent>();
        }
    }

    void Scene::update(const std::vector<ApplicationEvent> &events) {
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
                    m_isLeftMousePressed = false;
                    break;
                }
                const auto data = event.getData<ApplicationEvent::MouseButtonData>();
                if (data.button == MouseButton::left) {
                    onLeftMouse(data.pressed);
                } else if (data.button == MouseButton::right) {
                    // onRightMouse(data.pressed);
                } else if (data.button == MouseButton::middle) {
                    // onMiddleMouse(data.pressed);
                }
            } break;
            default:
                break;
            }
        }
        auto mainPageState = Pages::MainPageState::getInstance();
        if (mainPageState->isKeyPressed(GLFW_KEY_DELETE)) {
            auto view = m_registry.view<Canvas::Components::SelectedComponent>();
            for (auto &entt : view) {
                deleteEntity(entt);
            }
        }
    }

    void Scene::beginScene() {
        static int value = -1;
        m_msaaFramebuffer->bind();
        m_msaaFramebuffer->clearColorAttachment<GL_FLOAT>(0, glm::value_ptr(ViewportTheme::backgroundColor));
        m_msaaFramebuffer->clearColorAttachment<GL_INT>(1, &value);
        Gl::FrameBuffer::clearDepthStencilBuf();
        Renderer::begin(m_camera);
    }

    void Scene::endScene() {
        Renderer2D::Renderer::end();
        Gl::FrameBuffer::unbindAll();
        for (int i = 0; i < 2; i++) {
            m_msaaFramebuffer->bindColorAttachmentForRead(i);
            m_normalFramebuffer->bindColorAttachmentForDraw(i);
            Gl::FrameBuffer::blitColorBuffer(m_size.x, m_size.y);
        }
        Gl::FrameBuffer::unbindAll();
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

} // namespace Bess::Canvas
