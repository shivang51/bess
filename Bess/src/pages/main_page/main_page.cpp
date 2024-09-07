#include "pages/main_page/main_page.h"
#include "common/types.h"
#include "components/clock.h"
#include "components_manager/components_manager.h"
#include "events/application_event.h"
#include "ext/matrix_transform.hpp"
#include "pages/page_identifier.h"
#include "renderer/renderer.h"
#include "settings/viewport_theme.h"
#include "simulator/simulator_engine.h"
#include "ui/ui_main/ui_main.h"

using namespace Bess::Renderer2D;

namespace Bess::Pages {
    std::shared_ptr<Page> MainPage::getInstance(std::shared_ptr<Window> parentWindow) {
        static std::shared_ptr<MainPage> instance = std::make_shared<MainPage>(parentWindow);
        return instance;
    }

    std::shared_ptr<MainPage> MainPage::getTypedInstance(std::shared_ptr<Window> parentWindow) {
        return std::dynamic_pointer_cast<MainPage>(getInstance(parentWindow));
    }

    MainPage::MainPage(std::shared_ptr<Window> parentWindow) : Page(PageIdentifier::MainPage) {
        if (m_parentWindow == nullptr && parentWindow == nullptr) {
            throw std::runtime_error("MainPage: parentWindow is nullptr. Need to pass a parent window.");
        }

        m_camera = std::make_shared<Camera>(800, 600);
        m_framebuffer = std::make_unique<Gl::FrameBuffer>(800, 600);
        m_parentWindow = parentWindow;
        UI::UIMain::state.cameraZoom = Camera::defaultZoom;
        UI::UIMain::state.viewportTexture = m_framebuffer->getTexture();
        m_state = m_state->getInstance();
    }

    void MainPage::draw() {
        UI::UIMain::draw();
        drawScene();
    }

    void MainPage::drawScene() {
        m_framebuffer->bind();

        Renderer::begin(m_camera);

        Renderer::grid({0.f, 0.f, -2.f}, m_camera->getSpan(), -1);

        switch (m_state->getDrawMode()) {
        case UI::Types::DrawMode::connection: {
            auto sPos = m_state->getPoints()[0];
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
            viewportMousePos.y = UI::UIMain::state.viewportSize.y - viewportMousePos.y;
            m_state->setHoveredId(m_framebuffer->readId((int)viewportMousePos.x, (int)viewportMousePos.y));
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
            case ApplicationEventType::KeyPress: {
                auto data = event.getData<ApplicationEvent::KeyPressData>();
                m_state->setKeyPressed(data.key, true);
            } break;
            case ApplicationEventType::KeyRelease: {
                auto data = event.getData<ApplicationEvent::KeyReleaseData>();
                m_state->setKeyPressed(data.key, false);
            } break;
            default:
                break;
            }
        }

        if (m_state->getHoveredId() != -1) {
            auto &cid = Simulator::ComponentsManager::renderIdToCid(m_state->getHoveredId());
            Simulator::Components::ComponentEventData e;
            e.type = Simulator::Components::ComponentEventType::mouseHover;
            Simulator::ComponentsManager::components[cid]->onEvent(e);
        }

        if (m_state->isKeyPressed(GLFW_KEY_DELETE)) {
            auto &compId = m_state->getSelectedId();
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

        if (!m_state->isSimulationPaused())
            Simulator::Engine::Simulate();
    }

    glm::vec2 MainPage::getCameraPos() {
        return m_camera->getPos();
    }

    std::shared_ptr<Window> MainPage::getParentWindow() {
        return m_parentWindow;
    }

    void MainPage::onMouseWheel(double x, double y) {
        if (!isCursorInViewport())
            return;

        if (m_state->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
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
            auto &dragData = m_state->getDragDataRef();
            if (dragData.isDragging) {
                dragData.isDragging = false;
                dragData.dragOffset = {0.f, 0.f};
            }
            return;
        }

        if (pressed) {
            if (isCursorInViewport()) {
                auto pos = glm::vec3(getNVPMousePos(), 0.f);
            }
        }

        auto &cid = Simulator::ComponentsManager::renderIdToCid(m_state->getHoveredId());

        if (Simulator::ComponentsManager::emptyId == cid) {
            if (m_state->getDrawMode() == UI::Types::DrawMode::connection) {
                m_state->setConnStartId(Simulator::ComponentsManager::emptyId);
                m_state->getPointsRef().pop_back();
            }
            m_state->setSelectedId(Simulator::ComponentsManager::emptyId);
            m_state->setDrawMode(UI::Types::DrawMode::none);

            return;
        }

        Simulator::Components::ComponentEventData e;
        e.type = Simulator::Components::ComponentEventType::leftClick;
        e.pos = getNVPMousePos();

        Simulator::ComponentsManager::components[cid]->onEvent(e);
    }

    void MainPage::onRightMouse(bool pressed) {
        m_rightMousePressed = pressed;

        if (!pressed && isCursorInViewport()) {
            auto pos = glm::vec3(getNVPMousePos(), 0.f);
            // Simulator::ComponentsManager::generateNandGate(pos);
            return;
        }

        auto &cid = Simulator::ComponentsManager::renderIdToCid(m_state->getHoveredId());

        if (Simulator::ComponentsManager::emptyId == cid) {
            if (m_state->getPrevGenBankElement() == nullptr)
                return;
            auto prevGen = m_state->getPrevGenBankElement();
            Simulator::ComponentsManager::generateComponent(*prevGen, glm::vec3({getNVPMousePos(), 0.f}));
            return;
        }

        Simulator::Components::ComponentEventData e;
        e.type = Simulator::Components::ComponentEventType::rightClick;
        e.pos = getNVPMousePos();

        Simulator::ComponentsManager::components[cid]->onEvent(e);
    }

    void MainPage::onMiddleMouse(bool pressed) {
        m_middleMousePressed = pressed;
    }

    void MainPage::onMouseMove(double x, double y) {
        auto prevMousePos = m_state->getMousePos();
        float dx = (float)x - prevMousePos.x;
        float dy = (float)y - prevMousePos.y;
        m_state->setMousePos({x, y});

        if (!isCursorInViewport())
            return;

        if (m_state->isHoveredIdChanged()) {
            auto prevHoveredId = m_state->getPrevHoveredId();
            auto hoveredId = m_state->getHoveredId();

            if (prevHoveredId != -1 && Simulator::ComponentsManager::isRenderIdPresent(prevHoveredId)) {
                auto &cid = Simulator::ComponentsManager::renderIdToCid(
                    prevHoveredId);
                Simulator::Components::ComponentEventData e;
                e.type = Simulator::Components::ComponentEventType::mouseLeave;
                Simulator::ComponentsManager::components[cid]->onEvent(e);
            }

            prevHoveredId = hoveredId;

            if (hoveredId < 0)
                return;

            auto &cid = Simulator::ComponentsManager::renderIdToCid(
                hoveredId);
            Simulator::Components::ComponentEventData e;
            e.type = Simulator::Components::ComponentEventType::mouseEnter;
            Simulator::ComponentsManager::components[cid]->onEvent(e);
        }

        if (m_middleMousePressed) {
            m_camera->incrementPos({dx / UI::UIMain::state.cameraZoom,
                                    -dy / UI::UIMain::state.cameraZoom});
        } else if (m_leftMousePressed &&
                   m_state->getSelectedId() !=
                       Simulator::ComponentsManager::emptyId) {
            auto &entity = Simulator::ComponentsManager::components
                [m_state->getSelectedId()];

            // dragable components start from 101
            if ((int)entity->getType() <= 100)
                return;

            auto &pos = entity->getPosition();

            auto &dragData = m_state->getDragDataRef();
            if (!dragData.isDragging) {
                dragData.isDragging = true;
                dragData.dragOffset =
                    getNVPMousePos() - glm::vec2(pos);
            }

            auto dPos = getNVPMousePos() - dragData.dragOffset;

            pos = {dPos, pos.z};
        }
    }

    bool MainPage::isCursorInViewport() {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        const auto &viewportSize = UI::UIMain::state.viewportSize;
        auto mousePos = m_state->getMousePos();
        return mousePos.x > viewportPos.x &&
               mousePos.x < viewportPos.x + viewportSize.x &&
               mousePos.y > viewportPos.y &&
               mousePos.y < viewportPos.y + viewportSize.y;
    }

    glm::vec2 MainPage::getViewportMousePos() {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        auto mousePos = m_state->getMousePos();
        auto x = mousePos.x - viewportPos.x;
        auto y = mousePos.y - viewportPos.y;
        return {x, y};
    }

    glm::vec2 MainPage::getNVPMousePos() {
        const auto &viewportPos = getViewportMousePos();

        glm::vec2 pos = viewportPos;

        auto cameraPos = m_camera->getPos();
        glm::mat4 tansform = glm::translate(glm::mat4(1.f), glm::vec3(cameraPos.x, cameraPos.y, 0.f));
        tansform = glm::scale(tansform, glm::vec3(1.f / UI::UIMain::state.cameraZoom, 1.f / UI::UIMain::state.cameraZoom, 1.f));

        pos = glm::vec2(tansform * glm::vec4(pos.x, -pos.y, 0.f, 1.f));
        auto span = m_camera->getSpan() / 2.f;
        pos += glm::vec2({-span.x, span.y});
        return pos;
    }

} // namespace Bess::Pages
