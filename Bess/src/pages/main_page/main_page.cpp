#include "pages/main_page/main_page.h"
#include "application_state.h"
#include "components/clock.h"
#include "events/application_event.h"
#include "pages/page_identifier.h"
#include "renderer/renderer.h"
#include "settings/viewport_theme.h"
#include "simulator/simulator_engine.h"
#include "ui/ui_main/ui_main.h"

using namespace Bess::Renderer2D;

namespace Bess::Pages {
    std::shared_ptr<Page> MainPage::getInstance() {
        static std::shared_ptr<MainPage> instance = std::make_shared<MainPage>();
        return instance;
    }

    MainPage::MainPage() : Page(PageIdentifier::MainPage) {
        m_camera = std::make_shared<Camera>(800, 600);
        m_framebuffer = std::make_unique<Gl::FrameBuffer>(800, 600);
        UI::UIMain::state.cameraZoom = Camera::defaultZoom;
        UI::UIMain::state.viewportTexture = m_framebuffer->getTexture();
    }

    void MainPage::draw() {
        UI::UIMain::draw();
        drawScene();
    }

    void MainPage::drawScene() {
        m_framebuffer->bind();

        Renderer::begin(m_camera);

        auto pos = m_camera->getSpan() / 2.f;
        Renderer::grid({pos.x, -pos.y, -2.f}, m_camera->getSpan(), -1);

        switch (ApplicationState::drawMode) {
        case DrawMode::connection: {
            auto sPos = ApplicationState::points[0];
            sPos.z = -1;
            auto mPos = glm::vec3(getNVPMousePos(m_camera->getPos()), -1);
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
            viewportMousePos.y = UI::UIMain::state.viewportSize.y - viewportMousePos.y;
            ApplicationState::hoveredId = m_framebuffer->readId((int)viewportMousePos.x, (int)viewportMousePos.y);
        }

        m_framebuffer->unbind();
    }

    void MainPage::update(const std::vector<ApplicationEvent> &events) {
        static bool firstTime = true;
        if (firstTime) {
            if (UI::UIMain::state.viewportSize.x != 800.f)
                firstTime = false;
            auto pos = UI::UIMain::state.viewportSize / 2.f;
            // m_camera->setPos({pos.x, pos.y});
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

        for (auto &event : events) {
            switch (event.getType()) {
            case ApplicationEventType::MouseWheel: {
                auto data = event.getData<ApplicationEvent::MouseWheelData>();
                onMouseWheel(data.x, data.y);
            } break;
            case ApplicationEventType::MouseButton: {
                auto data = event.getData<ApplicationEvent::MouseButtonData>();
                if (data.button == MouseButton::left) {
                    onLeftMouse(data.pressed);
                } else if (data.button == MouseButton::right) {
                    onRightMouse(data.pressed);
                } else if (data.button == MouseButton::middle) {
                    onMiddleMouse(data.pressed);
                }
            } break;
            case ApplicationEventType::MouseMove: {
                auto data = event.getData<ApplicationEvent::MouseMoveData>();
                onMouseMove(data.x, data.y);
            } break;
            default:
                break;
            }
        }

        if (ApplicationState::hoveredId != -1) {
            auto &cid = Simulator::ComponentsManager::renderIdToCid(ApplicationState::hoveredId);
            Simulator::Components::ComponentEventData e;
            e.type = Simulator::Components::ComponentEventType::mouseHover;
            Simulator::ComponentsManager::components[cid]->onEvent(e);
        }

        if (ApplicationState::isKeyPressed(GLFW_KEY_DELETE)) {
            auto &compId = ApplicationState::getSelectedId();
            if (compId == Simulator::ComponentsManager::emptyId)
                return;
            Simulator::ComponentsManager::deleteComponent(compId);
        }

        for (auto &comp : Simulator::ComponentsManager::components) {
            if (comp.second->getType() == Simulator::ComponentType::clock) {
                auto clockCmp = std::dynamic_pointer_cast<Simulator::Components::Clock>(comp.second);
                clockCmp->update();
            }
        }

        if (!ApplicationState::simulationPaused)
            Simulator::Engine::Simulate();
    }

    void MainPage::onMouseWheel(double x, double y) {
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

    void MainPage::onLeftMouse(bool pressed) {
        m_leftMousePressed = pressed;
        if (!pressed || !isCursorInViewport()) {
            if (ApplicationState::dragData.isDragging) {
                ApplicationState::dragData.isDragging = false;
                ApplicationState::dragData.dragOffset = {0.f, 0.f};
            }
            return;
        }

        if (pressed) {
            if (isCursorInViewport()) {
                auto pos = glm::vec3(getNVPMousePos(m_camera->getPos()), 0.f);
            }
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
        e.pos = getNVPMousePos(m_camera->getPos());

        Simulator::ComponentsManager::components[cid]->onEvent(e);
    }

    void MainPage::onRightMouse(bool pressed) {
        m_rightMousePressed = pressed;

        if (!pressed && isCursorInViewport()) {
            auto pos = glm::vec3(getNVPMousePos(m_camera->getPos()), 0.f);
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
        e.pos = getNVPMousePos(m_camera->getPos());

        Simulator::ComponentsManager::components[cid]->onEvent(e);
    }

    void MainPage::onMiddleMouse(bool pressed) {
        m_middleMousePressed = pressed;
    }

    void MainPage::onMouseMove(double x, double y) {
        auto prevMousePos = ApplicationState::getMousePos();
        float dx = (float)x - prevMousePos.x;
        float dy = (float)y - prevMousePos.y;
        ApplicationState::setMousePos({x, y});

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
                    getNVPMousePos(m_camera->getPos()) - glm::vec2(pos);
            }

            auto dPos = getNVPMousePos(m_camera->getPos()) - ApplicationState::dragData.dragOffset;

            pos = {dPos, pos.z};
        }
    }

    bool MainPage::isCursorInViewport() {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        const auto &viewportSize = UI::UIMain::state.viewportSize;
        auto mousePos = ApplicationState::getMousePos();
        return mousePos.x > viewportPos.x &&
               mousePos.x < viewportPos.x + viewportSize.x &&
               mousePos.y > viewportPos.y &&
               mousePos.y < viewportPos.y + viewportSize.y;
    }

    glm::vec2 MainPage::getViewportMousePos() {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        auto mousePos = ApplicationState::getMousePos();
        auto x = mousePos.x - viewportPos.x;
        auto y = mousePos.y - viewportPos.y;
        return {x, y};
    }

    glm::vec2 MainPage::getNVPMousePos(const glm::vec2 &cameraPos) {
        const auto &viewportPos = getViewportMousePos();
        const auto &viewportSize = UI::UIMain::state.viewportSize;

        float x = viewportPos.x;
        float y = viewportPos.y;

        glm::vec2 pos = {x, y};
        pos.y *= -1;
        pos /= UI::UIMain::state.cameraZoom;
        pos -= cameraPos;
        return pos;
    }

} // namespace Bess::Pages
