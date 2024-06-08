#include "application.h"
#include "application_state.h"
#include "components/connection.h"
#include "components/nand_gate.h"
#include "fwd.hpp"
#include "renderer/renderer.h"
#include "ui.h"
#include <GLFW/glfw3.h>
#include <unordered_map>

#include "components_manager/components_manager.h"

using Bess::Renderer2D::Renderer;

namespace Bess {

#define BIND_EVENT_FN(fn) std::bind(&Application::fn, this)

#define BIND_EVENT_FN_1(fn)                                                    \
    std::bind(&Application::fn, this, std::placeholders::_1)

#define BIND_EVENT_FN_2(fn)                                                    \
    std::bind(&Application::fn, this, std::placeholders::_1,                   \
              std::placeholders::_2)

Application::Application() : m_window(800, 600, "Bess") {
    Simulator::ComponentsManager::init();
    ApplicationState::init();

    m_framebuffer = std::make_unique<Gl::FrameBuffer>(800.f, 600.f);
    m_camera = std::make_shared<Camera>(800.f, 600.f);

    UI::init(m_window.getGLFWHandle());
    UI::state.viewportTexture = m_framebuffer->getTexture();

    Renderer::init();

    m_window.onWindowResize(BIND_EVENT_FN_2(onWindowResize));
    m_window.onMouseWheel(BIND_EVENT_FN_2(onMouseWheel));
    m_window.onKeyPress(BIND_EVENT_FN_1(onKeyPress));
    m_window.onKeyRelease(BIND_EVENT_FN_1(onKeyRelease));
    m_window.onLeftMouse(BIND_EVENT_FN_1(onLeftMouse));
    m_window.onRightMouse(BIND_EVENT_FN_1(onRightMouse));
    m_window.onMiddleMouse(BIND_EVENT_FN_1(onMiddleMouse));
    m_window.onMouseMove(BIND_EVENT_FN_2(onMouseMove));

    Simulator::ComponentsManager::generateNandGate();
    Simulator::ComponentsManager::generateInputProbe();
}

Application::~Application() {
    UI::shutdown();
    m_window.close();
}

void Application::drawUI() { UI::draw(); }

void Application::drawScene() {
    m_framebuffer->bind();

    Renderer::begin(m_camera);

    switch (ApplicationState::drawMode) {
    case DrawMode::connection: {
        auto mPos = glm::vec3(getNVPMousePos(), -1);
        Renderer::curve(ApplicationState::points[0], mPos, {0.5, 0.8, 0.5}, -1);
    } break;
    default:
        break;
    }

    for (auto &[id, entity] : Simulator::ComponentsManager::renderComponenets) {
        entity->render();
    }

    Renderer::end();

    if (isCursorInViewport()) {
        auto viewportMousePos = getViewportMousePos();
        viewportMousePos.y = UI::state.viewportSize.y - viewportMousePos.y;
        ApplicationState::hoveredId = m_framebuffer->readId(
            (int)viewportMousePos.x, (int)viewportMousePos.y);
    }

    m_framebuffer->unbind();
}

void Application::run() {
    while (!m_window.isClosed()) {
        m_window.waitEventsTimeout(0.0167);
        update();
        drawScene();
        drawUI();

        m_window.update();
    }
}

void Application::update() {
    if (ApplicationState::hoveredId != -1) {
        auto &cid = Simulator::ComponentsManager::renderIdToCid(
            ApplicationState::hoveredId);
        Simulator::Components::ComponentEventData e;
        e.type = Simulator::Components::ComponentEventType::mouseHover;
        Simulator::ComponentsManager::components[cid]->onEvent(e);
    }

    if (m_framebuffer->getSize() != UI::state.viewportSize) {
        m_framebuffer->resize(UI::state.viewportSize.x,
                              UI::state.viewportSize.y);
        m_camera->resize(UI::state.viewportSize.x, UI::state.viewportSize.y);
    }

    if (UI::state.cameraZoom != m_camera->getZoom()) {
        m_camera->setZoom(UI::state.cameraZoom);
    }
}

void Application::quit() { m_window.close(); }

bool Application::isKeyPressed(int key) { return m_pressedKeys[key]; }

// callbacks

void Application::onWindowResize(int w, int h) {
    // glViewport(0, 0, w, h);
}

void Application::onMouseWheel(double x, double y) {
    if (!isCursorInViewport())
        return;

    if (isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
        float delta = (float)y * 0.1f;
        UI::state.cameraZoom += delta;
        if (UI::state.cameraZoom < 0.6f) {
            UI::state.cameraZoom = 0.6f;
        } else if (UI::state.cameraZoom > 1.6f) {
            UI::state.cameraZoom = 1.6f;
        }
    } else {
        m_camera->incrementPos(
            {x * 10 / m_camera->getZoom(), -y * 10 / m_camera->getZoom()});
    }
}

void Application::onKeyPress(int key) { m_pressedKeys[key] = true; }

void Application::onKeyRelease(int key) { m_pressedKeys[key] = false; }

void Application::onLeftMouse(bool pressed) {
    m_leftMousePressed = pressed;

    if (!pressed || !isCursorInViewport()) {
        if (ApplicationState::dragData.isDragging) {
            ApplicationState::dragData.isDragging = false;
            ApplicationState::dragData.dragOffset = {0.f, 0.f};
        }
        return;
    }

    auto &cid = Simulator::ComponentsManager::renderIdToCid(
        ApplicationState::hoveredId);

    if (Simulator::ComponentsManager::emptyId == cid) {
        if (ApplicationState::drawMode == DrawMode::connection) {
            ApplicationState::connStartId =
                Simulator::ComponentsManager::emptyId;
            ApplicationState::points.pop_back();
        }
        ApplicationState::setSelectedId(Simulator::ComponentsManager::emptyId);
        ApplicationState::drawMode = DrawMode::none;

        return;
    }

    Simulator::Components::ComponentEventData e;
    e.type = Simulator::Components::ComponentEventType::leftClick;
    e.pos = getNVPMousePos();

    Simulator::ComponentsManager::components[cid]->onEvent(e);
}

void Application::onRightMouse(bool pressed) {
    m_rightMousePressed = pressed;
    if (!pressed && isCursorInViewport()) {
        auto pos = glm::vec3(getNVPMousePos(), 0.f);

        Simulator::ComponentsManager::generateNandGate(pos);
        return;
    }

    auto &cid = Simulator::ComponentsManager::renderIdToCid(
        ApplicationState::hoveredId);

    if (Simulator::ComponentsManager::emptyId == cid) {
        return;
    }

    Simulator::Components::ComponentEventData e;
    e.type = Simulator::Components::ComponentEventType::rightClick;
    e.pos = getNVPMousePos();

    Simulator::ComponentsManager::components[cid]->onEvent(e);
}

void Application::onMiddleMouse(bool pressed) {
    m_middleMousePressed = pressed;
}

void Application::onMouseMove(double x, double y) {
    float dx = (float)x - m_mousePos.x;
    float dy = (float)y - m_mousePos.y;
    m_mousePos = {x, y};

    if (!isCursorInViewport())
        return;

    if (ApplicationState::prevHoveredId != ApplicationState::hoveredId) {
        if (ApplicationState::prevHoveredId != -1) {
            auto &cid = Simulator::ComponentsManager::renderIdToCid(
                ApplicationState::prevHoveredId);
            Simulator::Components::ComponentEventData e;
            e.type = Simulator::Components::ComponentEventType::mouseLeave;
            Simulator::ComponentsManager::components[cid]->onEvent(e);
        }

        ApplicationState::prevHoveredId = ApplicationState::hoveredId;
        if (ApplicationState::hoveredId < 0)
            return;

        auto &cid = Simulator::ComponentsManager::renderIdToCid(
            ApplicationState::hoveredId);
        Simulator::Components::ComponentEventData e;
        e.type = Simulator::Components::ComponentEventType::mouseEnter;
        Simulator::ComponentsManager::components[cid]->onEvent(e);
    }

    if (m_middleMousePressed) {
        m_camera->incrementPos(
            {dx / UI::state.cameraZoom, -dy / UI::state.cameraZoom});
    }

    else if (m_leftMousePressed && ApplicationState::getSelectedId() !=
                                       Simulator::ComponentsManager::emptyId) {
        auto &entity = Simulator::ComponentsManager::components
            [ApplicationState::getSelectedId()];

        if (entity->getType() != Simulator::ComponentType::gate &&
            entity->getType() != Simulator::ComponentType::inputProbe &&
            entity->getType() != Simulator::ComponentType::outputProbe)
            return;

        auto &pos = entity->getPosition();

        if (!ApplicationState::dragData.isDragging) {
            ApplicationState::dragData.isDragging = true;
            ApplicationState::dragData.dragOffset =
                getNVPMousePos() - glm::vec2(pos);
        }

        auto dPos = getNVPMousePos() - ApplicationState::dragData.dragOffset;

        pos = {dPos, pos.z};
    }
}

bool Application::isCursorInViewport() const {
    const auto &viewportPos = UI::state.viewportPos;
    const auto &viewportSize = UI::state.viewportSize;
    return m_mousePos.x > viewportPos.x &&
           m_mousePos.x < viewportPos.x + viewportSize.x &&
           m_mousePos.y > viewportPos.y &&
           m_mousePos.y < viewportPos.y + viewportSize.y;
}

glm::vec2 Application::getViewportMousePos() const {
    const auto &viewportPos = UI::state.viewportPos;
    const auto &viewportSize = UI::state.viewportSize;
    auto x = m_mousePos.x - viewportPos.x;
    auto y = m_mousePos.y - viewportPos.y;
    return {x, y};
}

glm::vec2 Application::getNVPMousePos() {
    const auto &viewportPos = getViewportMousePos();
    const auto &viewportSize = UI::state.viewportSize;

    float x = viewportPos.x;
    float y = viewportPos.y;

    glm::vec2 pos = {x, y};
    pos.y *= -1;
    pos /= UI::state.cameraZoom;
    pos -= m_camera->getPos();
    return pos;
}

} // namespace Bess
