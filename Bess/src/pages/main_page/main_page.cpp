#include "pages/main_page/main_page.h"
#include "common/types.h"
#include "components/clock.h"
#include "components_manager/components_manager.h"
#include "events/application_event.h"
#include "ext/matrix_transform.hpp"
#include "ext/vector_float3.hpp"
#include "pages/page_identifier.h"
#include "renderer/renderer.h"
#include "settings/viewport_theme.h"
#include "simulator/simulator_engine.h"
#include "ui/ui_main/ui_main.h"

using namespace Bess::Renderer2D;

namespace Bess::Pages {
    std::shared_ptr<Page> MainPage::getInstance(const std::shared_ptr<Window> &parentWindow) {
        static auto instance = std::make_shared<MainPage>(parentWindow);
        return instance;
    }

    std::shared_ptr<MainPage> MainPage::getTypedInstance(std::shared_ptr<Window> parentWindow) {
        const auto instance = getInstance(parentWindow);
        return std::dynamic_pointer_cast<MainPage>(instance);
    }

    MainPage::MainPage(std::shared_ptr<Window> parentWindow) : Page(PageIdentifier::MainPage) {
        if (m_parentWindow == nullptr && parentWindow == nullptr) {
            throw std::runtime_error("MainPage: parentWindow is nullptr. Need to pass a parent window.");
        }
        m_camera = std::make_shared<Camera>(800, 600);
        m_parentWindow = parentWindow;

        std::vector<Gl::FBAttachmentType> attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::R32I_REDI, Gl::DEPTH32F_STENCIL8};
        m_multiSampledFramebuffer = std::make_unique<Gl::FrameBuffer>(800, 600, attachments, true);

        attachments = {Gl::FBAttachmentType::RGB_RGB, Gl::FBAttachmentType::R32I_REDI};
        m_normalFramebuffer = std::make_unique<Gl::FrameBuffer>(800, 600, attachments);

        UI::UIMain::state.cameraZoom = Camera::defaultZoom;
        UI::UIMain::state.viewportTexture = m_normalFramebuffer->getColorBufferTexId(0);
        m_state = MainPageState::getInstance();
    }

    void MainPage::draw() {
        drawScene();

        for (int i = 0; i < 2; i++) {
            m_multiSampledFramebuffer->bindColorAttachmentForRead(i);
            m_normalFramebuffer->bindColorAttachmentForDraw(i);
            Gl::FrameBuffer::blitColorBuffer(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        }
        Gl::FrameBuffer::unbindAll();
        UI::UIMain::draw();
    }

    void MainPage::drawScene() {
        static int value = -1;
        m_normalFramebuffer->clearColorAttachment<GL_INT>(1, &value);

        m_multiSampledFramebuffer->bind();

        const auto bgColor = ViewportTheme::backgroundColor;
        const float clearColor[] = {bgColor.x, bgColor.y, bgColor.z, bgColor.a};
        m_multiSampledFramebuffer->clearColorAttachment<GL_FLOAT>(0, clearColor);
        m_multiSampledFramebuffer->clearColorAttachment<GL_INT>(1, &value);

        Gl::FrameBuffer::clearDepthStencilBuf();

        Renderer::begin(m_camera);

        Renderer::grid({0.f, 0.f, -2.f}, m_camera->getSpan(), -1);

        switch (m_state->getDrawMode()) {
        case UI::Types::DrawMode::connection: {
            std::vector<glm::vec3> points = m_state->getPoints();
            auto startPos = Simulator::ComponentsManager::components[m_state->getConnStartId()]->getPosition();
            points.insert(points.begin(), startPos);
            const auto mPos = glm::vec3(getNVPMousePos(), -1);
            points.emplace_back(mPos);
            float weight = 2.f;

            for (int i = 0; i < points.size() - 1; i++) {
                auto sPos = points[i];
                auto ePos = points[i + 1];
                float offset = weight / 2.f;
                if (sPos.y > ePos.y)
                    offset = -offset;
                float midX = (sPos.x + ePos.x) / 2.f;
                Renderer::line(sPos, {midX, sPos.y, -1}, weight, ViewportTheme::wireColor, -1);
                Renderer::line({midX, sPos.y - offset, -1}, {midX, ePos.y + offset, -1}, weight, ViewportTheme::wireColor, -1);
                Renderer::line({midX, ePos.y, -1}, ePos, weight, ViewportTheme::wireColor, -1);
            }
        } break;
        default:
            break;
        }

        for (auto &id : Simulator::ComponentsManager::renderComponenets) {
            const auto &entity = Simulator::ComponentsManager::components[id];
            entity->render();
        }

        Renderer::end();

        Gl::FrameBuffer::unbindAll();
    }

    void MainPage::update(const std::vector<ApplicationEvent> &events) {
        static bool firstTime = true;
        if (firstTime) {
            if (UI::UIMain::state.viewportSize.x != 800.f)
                firstTime = false;
            auto pos = UI::UIMain::state.viewportSize / 2.f;
            // m_camera->setPos({pos.x, pos.y});
        }

        if (m_multiSampledFramebuffer->getSize() != UI::UIMain::state.viewportSize) {
            m_multiSampledFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
            m_normalFramebuffer->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
            m_camera->resize(UI::UIMain::state.viewportSize.x, UI::UIMain::state.viewportSize.y);
        }

        if (UI::UIMain::state.cameraZoom != m_camera->getZoom()) {
            // auto mp = getViewportMousePos();
            // mp = m_mousePos;
            // m_camera->zoomToPoint({ mp.x, mp.y},
            // UI::UIMain::state.cameraZoom);
            m_camera->setZoom(UI::UIMain::state.cameraZoom);
        }

        if (isCursorInViewport()) {
            auto viewportMousePos = getViewportMousePos();
            viewportMousePos.y = UI::UIMain::state.viewportSize.y - viewportMousePos.y;
            m_state->setHoveredId(m_normalFramebuffer->readIntFromColAttachment(1, static_cast<int>(viewportMousePos.x), static_cast<int>(viewportMousePos.y)));
        }

        for (auto &event : events) {
            switch (event.getType()) {
            case ApplicationEventType::MouseWheel: {
                const auto data = event.getData<ApplicationEvent::MouseWheelData>();
                onMouseWheel(data.x, data.y);
            } break;
            case ApplicationEventType::MouseButton: {
                const auto data = event.getData<ApplicationEvent::MouseButtonData>();
                if (data.button == MouseButton::left) {
                    onLeftMouse(data.pressed);
                } else if (data.button == MouseButton::right) {
                    onRightMouse(data.pressed);
                } else if (data.button == MouseButton::middle) {
                    onMiddleMouse(data.pressed);
                }
            } break;
            case ApplicationEventType::MouseMove: {
                const auto data = event.getData<ApplicationEvent::MouseMoveData>();
                onMouseMove(data.x, data.y);
            } break;
            case ApplicationEventType::KeyPress: {
                const auto data = event.getData<ApplicationEvent::KeyPressData>();
                m_state->setKeyPressed(data.key, true);
            } break;
            case ApplicationEventType::KeyRelease: {
                const auto data = event.getData<ApplicationEvent::KeyReleaseData>();
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
                const auto clockCmp = std::dynamic_pointer_cast<Simulator::Components::Clock>(comp.second);
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
            const float delta = static_cast<float>(y) * 0.1f;
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
                if (m_state->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
                    m_state->getPointsRef().emplace_back(glm::vec3(getNVPMousePos(), 0.f));
                } else {
                    m_state->setConnStartId(Simulator::ComponentsManager::emptyId);
                    m_state->getPointsRef().clear();
                    m_state->setDrawMode(UI::Types::DrawMode::none);
                }
            } else {
                m_state->setSelectedId(Simulator::ComponentsManager::emptyId);
                m_state->setDrawMode(UI::Types::DrawMode::none);
            }
            return;
        }

        Simulator::Components::ComponentEventData e;
        e.type = Simulator::Components::ComponentEventType::leftClick;
        e.pos = getNVPMousePos();

        Simulator::ComponentsManager::components[cid]->onEvent(e);
    }

    void MainPage::onRightMouse(bool pressed) {
        m_rightMousePressed = pressed;

        auto hoveredId = m_state->getHoveredId();
        auto &cid = Simulator::ComponentsManager::renderIdToCid(hoveredId);

        if (!pressed && isCursorInViewport() && cid == Simulator::ComponentsManager::emptyId) {
            auto pos = glm::vec3(getNVPMousePos(), 0.f);
            // Simulator::ComponentsManager::generateNandGate(pos);
            const auto prevGen = m_state->getPrevGenBankElement();
            if (prevGen == nullptr)
                return;
            Simulator::ComponentsManager::generateComponent(*prevGen, glm::vec3({getNVPMousePos(), 0.f}));
            return;
        }

        if (cid == Simulator::ComponentsManager::emptyId)
            return;

        Simulator::Components::ComponentEventData e;
        e.type = Simulator::Components::ComponentEventType::rightClick;
        e.pos = getNVPMousePos();

        Simulator::ComponentsManager::components[cid]->onEvent(e);
    }

    void MainPage::onMiddleMouse(bool pressed) {
        m_middleMousePressed = pressed;
    }

    void MainPage::onMouseMove(double x, double y) {
        const auto prevMousePos = m_state->getMousePos();
        const float dx = static_cast<float>(x) - prevMousePos.x;
        const float dy = static_cast<float>(y) - prevMousePos.y;
        m_state->setMousePos({x, y});

        if (!isCursorInViewport())
            return;

        if (m_state->isHoveredIdChanged()) {
            auto prevHoveredId = m_state->getPrevHoveredId();
            const auto hoveredId = m_state->getHoveredId();

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
            const auto &entity = Simulator::ComponentsManager::components
                [m_state->getSelectedId()];

            // dragable components start from 101
            if (static_cast<int>(entity->getType()) <= 100)
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
        const auto mousePos = m_state->getMousePos();
        return mousePos.x > viewportPos.x &&
               mousePos.x < viewportPos.x + viewportSize.x &&
               mousePos.y > viewportPos.y &&
               mousePos.y < viewportPos.y + viewportSize.y;
    }

    glm::vec2 MainPage::getViewportMousePos() {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        const auto mousePos = m_state->getMousePos();
        auto x = mousePos.x - viewportPos.x;
        auto y = mousePos.y - viewportPos.y;
        return {x, y};
    }

    glm::vec2 MainPage::getNVPMousePos() {
        const auto &viewportPos = getViewportMousePos();

        glm::vec2 pos = viewportPos;

        const auto cameraPos = m_camera->getPos();
        glm::mat4 tansform = glm::translate(glm::mat4(1.f), glm::vec3(cameraPos.x, cameraPos.y, 0.f));
        tansform = glm::scale(tansform, glm::vec3(1.f / UI::UIMain::state.cameraZoom, 1.f / UI::UIMain::state.cameraZoom, 1.f));

        pos = glm::vec2(tansform * glm::vec4(pos.x, -pos.y, 0.f, 1.f));
        auto span = m_camera->getSpan() / 2.f;
        pos += glm::vec2({-span.x, span.y});
        return pos;
    }

} // namespace Bess::Pages
