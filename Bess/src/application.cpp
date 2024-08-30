#include "application.h"
#include "application_state.h"
#include "fwd.hpp"
#include "renderer/renderer.h"
#include "ui/ui.h"
#include <GLFW/glfw3.h>

#include "components_manager/component_bank.h"
#include "components_manager/components_manager.h"

#include "settings/settings.h"
#include "settings/viewport_theme.h"
#include "simulator/simulator_engine.h"
#include "components/clock.h"

#include "common/bind_helpers.h"

using Bess::Renderer2D::Renderer;

namespace Bess {
    Application::Application() : m_mainWindow(800, 600, "Bess") { init(); }

    Application::Application(const std::string &path)
        : m_mainWindow(800, 600, "Bess") {
        init();
        ApplicationState::loadProject(path);
    }

    Application::~Application() {
        UI::UIMain::shutdown();
        shutdown();
    }

    void Application::drawUI() {}

    void Application::drawScene() {
        m_framebuffer->bind();

        Renderer::begin(m_camera);

        auto pos = m_camera->getSpan() / 2.f;
        Renderer::grid({pos.x, -pos.y, -2.f}, m_camera->getSpan(), -1);

        switch (ApplicationState::drawMode) {
        case DrawMode::connection: {
            auto sPos = ApplicationState::points[0];
            sPos.z = -1;
            auto mPos = glm::vec3(getNVPMousePos(), -1);
            Renderer::curve(sPos, mPos, 2.5, ViewportTheme::wireColor, -1);
        } break;
        default:
            break;
        }

        for (auto &id : Simulator::ComponentsManager::renderComponenets) {
            auto &entity = Simulator::ComponentsManager::components[id];
            entity->render();
        }

        Renderer::end();

        if (isCursorInViewport()) {
            auto viewportMousePos = getViewportMousePos();
            viewportMousePos.y =
                UI::UIMain::state.viewportSize.y - viewportMousePos.y;
            ApplicationState::hoveredId = m_framebuffer->readId(
                (int)viewportMousePos.x, (int)viewportMousePos.y);
        }

        m_framebuffer->unbind();
    }

    void Application::run() {
        double prevTime = glfwGetTime();
        int fps = 0;
        double frameTime = 1.0 / 60.0;
        double accumulatedTime = 0.0;

        while (!m_mainWindow.isClosed()) {
            double currTime = glfwGetTime();
            double deltaTime = currTime - prevTime;
            prevTime = currTime;

            accumulatedTime += deltaTime;

            if (accumulatedTime >= frameTime) {
                update();
                UI::UIMain::begin();
                UI::UIMain::draw();
                UI::UIMain::drawStats(fps);
                drawScene();
                UI::UIMain::end();

                fps = static_cast<int>(std::round(1.0 / accumulatedTime));
                accumulatedTime = 0.0;
            }

            // Poll events
            Window::pollEvents();
        }
    }

    void Application::update() {
        m_mainWindow.update();

        for (auto& comp : Simulator::ComponentsManager::components) {
            if (comp.second->getType() == Simulator::ComponentType::clock) {
                auto clockCmp = std::dynamic_pointer_cast<Simulator::Components::Clock>(comp.second);
                clockCmp->update();
            }
        }

        if (ApplicationState::hoveredId != -1) {
            auto &cid = Simulator::ComponentsManager::renderIdToCid(ApplicationState::hoveredId);
            Simulator::Components::ComponentEventData e;
            e.type = Simulator::Components::ComponentEventType::mouseHover;
            Simulator::ComponentsManager::components[cid]->onEvent(e);
        }

        if (m_framebuffer->getSize() != UI::UIMain::state.viewportSize) {
            m_framebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
            m_camera->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
            ApplicationState::normalizingFactor = glm::min(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        }

        if (UI::UIMain::state.cameraZoom != m_camera->getZoom()) {
            // auto mp = getViewportMousePos();
            // mp = m_mousePos;
            // m_camera->zoomToPoint({ mp.x, mp.y},
            // UI::UIMain::state.cameraZoom);
            m_camera->setZoom(UI::UIMain::state.cameraZoom);
        }

        if (ApplicationState::isKeyPressed(GLFW_KEY_DELETE)) {
            auto &compId = ApplicationState::getSelectedId();
            if (compId == Simulator::ComponentsManager::emptyId)
                return;
            Simulator::ComponentsManager::deleteComponent(compId);
        }

        if (!ApplicationState::simulationPaused)
            Simulator::Engine::Simulate();

        static bool firstTime = true;
        if (firstTime) {
            if (UI::UIMain::state.viewportSize.x != 800.f)
                firstTime = false;
            auto pos = UI::UIMain::state.viewportSize / 2.f;
            // m_camera->setPos({pos.x, pos.y});
        }
    }

    void Application::quit() { m_mainWindow.close(); }

    // callbacks

    void Application::onWindowResize(int w, int h) {
        // glViewport(0, 0, w, h);
    }

    void Application::onMouseWheel(double x, double y) {
        if (!isCursorInViewport())
            return;

        if (ApplicationState::isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
            float delta = (float)y * 0.1f;
            UI::UIMain::state.cameraZoom += delta;
            if (UI::UIMain::state.cameraZoom < Camera::zoomMin) {
                UI::UIMain::state.cameraZoom = Camera::zoomMin;
            } else if (UI::UIMain::state.cameraZoom > Camera::zoomMax) {
                UI::UIMain::state.cameraZoom = Camera::zoomMax;
            }
        } else {
            m_camera->incrementPos(
                {x * 10 / m_camera->getZoom(), -y * 10 / m_camera->getZoom()});
        }
    }

    void Application::onKeyPress(int key) {
        ApplicationState::setKeyPressed(key, true);
    }

    void Application::onKeyRelease(int key) {
        ApplicationState::setKeyPressed(key, false);
    }

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
            ApplicationState::setSelectedId(
                Simulator::ComponentsManager::emptyId);
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
            // Simulator::ComponentsManager::generateNandGate(pos);
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
            if (ApplicationState::prevHoveredId != -1 &&
                Simulator::ComponentsManager::isRenderIdPresent(
                    ApplicationState::prevHoveredId)) {
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
            m_camera->incrementPos({dx / UI::UIMain::state.cameraZoom,
                                    -dy / UI::UIMain::state.cameraZoom});
        } else if (m_leftMousePressed &&
                   ApplicationState::getSelectedId() !=
                       Simulator::ComponentsManager::emptyId) {
            auto &entity = Simulator::ComponentsManager::components
                [ApplicationState::getSelectedId()];

            // dragable components start from 101
            if ((int)entity->getType() <= 100)
                return;

            auto &pos = entity->getPosition();

            if (!ApplicationState::dragData.isDragging) {
                ApplicationState::dragData.isDragging = true;
                ApplicationState::dragData.dragOffset =
                    getNVPMousePos() - glm::vec2(pos);
            }

            auto dPos =
                getNVPMousePos() - ApplicationState::dragData.dragOffset;

            pos = {dPos, pos.z};
        }
    }

    bool Application::isCursorInViewport() const {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        const auto &viewportSize = UI::UIMain::state.viewportSize;
        return m_mousePos.x > viewportPos.x &&
               m_mousePos.x < viewportPos.x + viewportSize.x &&
               m_mousePos.y > viewportPos.y &&
               m_mousePos.y < viewportPos.y + viewportSize.y;
    }

    glm::vec2 Application::getViewportMousePos() const {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        auto x = m_mousePos.x - viewportPos.x;
        auto y = m_mousePos.y - viewportPos.y;
        return {x, y};
    }

    glm::vec2 Application::getNVPMousePos() {
        const auto &viewportPos = getViewportMousePos();
        const auto &viewportSize = UI::UIMain::state.viewportSize;

        float x = viewportPos.x;
        float y = viewportPos.y;

        glm::vec2 pos = {x, y};
        pos.y *= -1;
        pos /= UI::UIMain::state.cameraZoom;
        pos -= m_camera->getPos();
        return pos;
    }

    void Application::init() {
        Config::Settings::init();

        Simulator::ComponentsManager::init();

        Simulator::ComponentBankElement el(Simulator::ComponentType::inputProbe, "Input Probe");
        Simulator::ComponentBank::addToCollection("I/O", el);
        Simulator::ComponentBank::addToCollection("I/O", {Simulator::ComponentType::outputProbe, "Ouput Probe"});
        Simulator::ComponentBank::addToCollection("I/O", {Simulator::ComponentType::clock, "Clock"});
        Simulator::ComponentBank::addToCollection("Misc", {Simulator::ComponentType::text, "Text"});
        Simulator::ComponentBank::loadMultiFromJson("assets/comp_collections.json");

        ApplicationState::init(&m_mainWindow);

        m_framebuffer = std::make_unique<Gl::FrameBuffer>(800.f, 600.f);
        m_camera = std::make_shared<Camera>(800.f, 600.f);

        UI::UIMain::init(m_mainWindow.getGLFWHandle());
        UI::UIMain::state.viewportTexture = m_framebuffer->getTexture();
        UI::UIMain::state.cameraZoom = Camera::defaultZoom;

        Renderer::init();

        m_mainWindow.onWindowResize(BIND_FN_2(Application::onWindowResize));
        m_mainWindow.onMouseWheel(BIND_FN_2(Application::onMouseWheel));
        m_mainWindow.onKeyPress(BIND_FN_1(Application::onKeyPress));
        m_mainWindow.onKeyRelease(BIND_FN_1(Application::onKeyRelease));
        m_mainWindow.onLeftMouse(BIND_FN_1(Application::onLeftMouse));
        m_mainWindow.onRightMouse(BIND_FN_1(Application::onRightMouse));
        m_mainWindow.onMiddleMouse(BIND_FN_1(Application::onMiddleMouse));
        m_mainWindow.onMouseMove(BIND_FN_2(Application::onMouseMove));
    }

    void Application::shutdown() { m_mainWindow.close(); }

    void Application::loadProject(const std::string &path) {
        ApplicationState::loadProject(path);
    }

    void Application::saveProject() { ApplicationState::saveCurrentProject(); }

} // namespace Bess
