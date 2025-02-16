#include "pages/main_page/main_page.h"
#include "GLFW/glfw3.h"
#include "common/types.h"
#include "events/application_event.h"
#include "ext/matrix_transform.hpp"
#include "ext/vector_float2.hpp"
#include "ext/vector_float3.hpp"
#include "ext/vector_float4.hpp"
#include "pages/page_identifier.h"
#include "scene/renderer/renderer.h"
#include "scene_new/scene.h"
#include "settings/viewport_theme.h"
#include "ui/ui_main/ui_main.h"
#include <memory>
#include <set>

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
        m_parentWindow = parentWindow;

        m_scene = std::make_unique<Bess::Canvas::Scene>();

        UI::UIMain::state.cameraZoom = Camera::defaultZoom;
        UI::UIMain::state.viewportTexture = m_scene->getTextureId();
        m_state = MainPageState::getInstance();
    }

    void MainPage::draw() {
        UI::UIMain::draw();
        m_scene->render();
    }

    void MainPage::update(const std::vector<ApplicationEvent> &events) {
        if (m_scene->getSize() != UI::UIMain::state.viewportSize) {
            m_scene->resize(UI::UIMain::state.viewportSize);
        }

        if (UI::UIMain::state.cameraZoom != m_scene->getCameraZoom()) {
            m_scene->setZoom(UI::UIMain::state.cameraZoom);
        }

        // auto &dragData = m_state->getDragData();
        //
        // if (!dragData.isDragging && isCursorInViewport() && UI::UIMain::state.isViewportFocused) {
        //     auto viewportMousePos = getViewportMousePos();
        //     viewportMousePos.y = UI::UIMain::state.viewportSize.y - viewportMousePos.y;
        //     int x = static_cast<int>(viewportMousePos.x);
        //     int y = static_cast<int>(viewportMousePos.y);
        //     int hoverId = m_normalFramebuffer->readIntFromColAttachment(1, x, y);
        //     // if (hoverId < Simulator::ComponentsManager::emptyRenderId || hoverId > Simulator::ComponentsManager::getCurrentRenderId())
        //     //     hoverId = Simulator::ComponentsManager::emptyRenderId;
        //     m_state->setHoveredId(hoverId);
        // }
        //
        // if (m_state->shouldReadBulkIds()) {
        //     m_state->setReadBulkIds(false);
        //     m_state->clearBulkIds();
        //     auto end = getViewportMousePos();
        //     auto start = dragData.vpMousePos;
        //     auto size = end - start;
        //     glm::vec2 pos = {std::min(start.x, end.x), std::max(start.y, end.y)};
        //     size = glm::abs(size);
        //
        //     int w = (int)size.x;
        //     int h = (int)size.y;
        //     int x = pos.x, y = UI::UIMain::state.viewportSize.y - pos.y;
        //
        //     auto ids = m_normalFramebuffer->readIntsFromColAttachment(1, x, y, w, h);
        //
        //     if (ids.size() == 0)
        //         return;
        //
        //     std::set<int> uniqueIds(ids.begin(), ids.end());
        //
        //     for (auto &id : uniqueIds) {
        //         // if (id <= Simulator::ComponentsManager::emptyRenderId || id > Simulator::ComponentsManager::getCurrentRenderId() || !Simulator::ComponentsManager::isRenderComponent(id))
        //         //     continue;
        //         // m_state->addBulkId(Simulator::ComponentsManager::renderIdToCid(id));
        //     }
        //     m_state->clearDragData();
        // }
        //
        for (auto &event : events) {
            switch (event.getType()) {
                //         case ApplicationEventType::MouseWheel: {
                //             const auto data = event.getData<ApplicationEvent::MouseWheelData>();
                //             onMouseWheel(data.x, data.y);
                //         } break;
                //         case ApplicationEventType::MouseButton: {
                //             const auto data = event.getData<ApplicationEvent::MouseButtonData>();
                //             if (data.button == MouseButton::left) {
                //                 onLeftMouse(data.pressed);
                //             } else if (data.button == MouseButton::right) {
                //                 onRightMouse(data.pressed);
                //             } else if (data.button == MouseButton::middle) {
                //                 onMiddleMouse(data.pressed);
                //             }
                //         } break;
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

        if (UI::UIMain::state.isViewportFocused)
            m_scene->update(events);

        // // key board bindings
        // {
        //     // if (m_state->isKeyPressed(GLFW_KEY_DELETE)) {
        //     //     for (auto &compId : m_state->getBulkIds()) {
        //     //         if (compId == Simulator::ComponentsManager::emptyId)
        //     //             continue;
        //     //         Simulator::ComponentsManager::deleteComponent(compId);
        //     //     }
        //     //     m_state->setHoveredId(Simulator::ComponentsManager::emptyRenderId);
        //     // }
        //     //
        //     // if (m_state->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
        //     //     if (m_state->isKeyPressed(GLFW_KEY_A)) {
        //     //         m_state->clearBulkIds();
        //     //         for (auto &compId : Simulator::ComponentsManager::renderComponents)
        //     //             m_state->addBulkId(compId);
        //     //     }
        //     // }
        // }
        //
        // // if (UI::UIMain::state.isViewportFocused && isCursorInViewport() && m_state->getHoveredId() > Simulator::ComponentsManager::emptyRenderId) {
        // //     auto &cid = Simulator::ComponentsManager::renderIdToCid(m_state->getHoveredId());
        // //     Simulator::Components::ComponentEventData e;
        // //     e.type = Simulator::Components::ComponentEventType::mouseHover;
        // //     Simulator::ComponentsManager::getComponent(cid)->onEvent(e);
        // // }
        // //
        // // for (auto &comp : Simulator::ComponentsManager::components) {
        // //     if (comp.second->getType() == Simulator::ComponentType::clock) {
        // //         const auto clockCmp = Simulator::ComponentsManager::getComponent<Simulator::Components::Clock>(comp.first);
        // //         clockCmp->update();
        // //     } else if (comp.second->getType() == Simulator::ComponentType::connection) {
        // //         const auto connCmp = Simulator::ComponentsManager::getComponent<Simulator::Components::Connection>(comp.first);
        // //         connCmp->update();
        // //     } else if (comp.second->getType() != Simulator::ComponentType::connectionPoint) {
        // //         comp.second->update();
        // //     }
        // // }
        // //
        // // if (!m_state->isSimulationPaused()) {
        // //     Simulator::Engine::Simulate();
        // // }
    }

    std::shared_ptr<Window> MainPage::getParentWindow() {
        return m_parentWindow;
    }

    std::shared_ptr<Canvas::Scene> MainPage::getScene() {
        return m_scene;
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
            // glm::vec2 dPos = {x, y};
            // dPos *= 10 / m_camera->getZoom() * -1;
            // m_camera->incrementPos(dPos);
        }
    }

    void MainPage::finishDragging() {
        auto dragData = m_state->getDragData();
        if (m_state->getDrawMode() == UI::Types::DrawMode::selectionBox) {
            m_state->setReadBulkIds(true);
            m_state->setDrawMode(UI::Types::DrawMode::none);
            dragData.isDragging = false;
            m_state->setDragData(dragData);
        } else if (dragData.isDragging && m_state->getDrawMode() == UI::Types::DrawMode::none) {
            auto cid = m_state->getBulkIdAt(0);
            // auto comp = Simulator::ComponentsManager::getComponent(cid);
            // comp->setPosition({glm::vec2(comp->getPosition()), dragData.orinalEntPos.z});
            // m_state->clearDragData();
        }
    }

    void MainPage::onLeftMouse(bool pressed) {

        if (pressed != m_leftMousePressed) {
            m_lastUpdateTime = std::chrono::steady_clock::now();
        }

        // update only on release when outside viewport
        if (!pressed)
            m_leftMousePressed = pressed;

        if (!isCursorInViewport())
            return;

        m_leftMousePressed = pressed;

        if (!pressed) {
            auto &dragData = m_state->getDragData();
            if (dragData.isDragging) {
                finishDragging();
            }
            return;
        }
        // auto &cid = Simulator::ComponentsManager::renderIdToCid(m_state->getHoveredId());
        //
        // if (Simulator::ComponentsManager::emptyId == cid) {
        //     if (m_state->getDrawMode() == UI::Types::DrawMode::connection) {
        //         if (m_state->isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
        //             m_state->getPointsRef().emplace_back(glm::vec3(getNVPMousePos(), 0.f));
        //         } else {
        //             m_state->setConnStartId(Simulator::ComponentsManager::emptyId);
        //             m_state->getPointsRef().clear();
        //             m_state->setDrawMode(UI::Types::DrawMode::none);
        //         }
        //     } else {
        //         m_state->clearBulkIds();
        //         m_state->setDrawMode(UI::Types::DrawMode::none);
        //     }
        //     return;
        // }
        //
        // Simulator::Components::ComponentEventData e;
        // e.type = Simulator::Components::ComponentEventType::leftClick;
        // e.pos = getNVPMousePos();
        //
        // Simulator::ComponentsManager::components[cid]->onEvent(e);
    }

    void MainPage::onRightMouse(bool pressed) {

        if (!isCursorInViewport())
            return;

        m_rightMousePressed = pressed;

        auto hoveredId = m_state->getHoveredId();

        // if (!pressed && hoveredId == Simulator::ComponentsManager::emptyRenderId) {
        //     auto pos = glm::vec3(getNVPMousePos(), 0.f);
        //     const auto prevGen = m_state->getPrevGenBankElement();
        //     if (prevGen == nullptr)
        //         return;
        //     Simulator::ComponentsManager::generateComponent(*prevGen, glm::vec3({getNVPMousePos(), 0.f}));
        //     return;
        // }
        //
        // auto &cid = Simulator::ComponentsManager::renderIdToCid(hoveredId);
        // if (cid == Simulator::ComponentsManager::emptyId)
        //     return;
        //
        // Simulator::Components::ComponentEventData e;
        // e.type = Simulator::Components::ComponentEventType::rightClick;
        // e.pos = getNVPMousePos();
        //
        // Simulator::ComponentsManager::components[cid]->onEvent(e);
    }

    void MainPage::onMiddleMouse(bool pressed) {
        if (!isCursorInViewport())
            return;

        m_middleMousePressed = pressed;
    }

    void MainPage::onMouseMove(double x, double y) {
        const auto prevMousePos = m_state->getMousePos();
        const float dx = static_cast<float>(x) - prevMousePos.x;
        const float dy = static_cast<float>(y) - prevMousePos.y;
        m_state->setMousePos({x, y});

        auto &dragData = m_state->getDragData();

        if (!isCursorInViewport()) {
            if (dragData.isDragging) {
                finishDragging();
            }
            return;
        }

        if (m_state->isHoveredIdChanged() && !dragData.isDragging) {
            auto prevHoveredId = m_state->getPrevHoveredId();
            const auto hoveredId = m_state->getHoveredId();

            // if (prevHoveredId != Simulator::ComponentsManager::emptyRenderId && Simulator::ComponentsManager::isRenderIdPresent(prevHoveredId)) {
            //     auto &cid = Simulator::ComponentsManager::renderIdToCid(
            //         prevHoveredId);
            //     Simulator::Components::ComponentEventData e;
            //     e.type = Simulator::Components::ComponentEventType::mouseLeave;
            //     Simulator::ComponentsManager::components[cid]->onEvent(e);
            // }

            prevHoveredId = hoveredId;

            // if (hoveredId > Simulator::ComponentsManager::emptyRenderId && Simulator::ComponentsManager::isRenderIdPresent(hoveredId)) {
            //     auto &cid = Simulator::ComponentsManager::renderIdToCid(hoveredId);
            //     Simulator::Components::ComponentEventData e;
            //     e.type = Simulator::Components::ComponentEventType::mouseEnter;
            //     Simulator::ComponentsManager::components[cid]->onEvent(e);
            // }
        }

        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_lastUpdateTime).count();

        if (m_middleMousePressed) {
            glm::vec2 dPos = {dx / UI::UIMain::state.cameraZoom, dy / UI::UIMain::state.cameraZoom};
            dPos *= -1;
            // m_camera->incrementPos(dPos);
        } else if (m_leftMousePressed && (diff > 50 || dragData.isDragging)) {
            // // dragging an entity
            // if (m_state->getHoveredId() > Simulator::ComponentsManager::emptyRenderId || (dragData.isDragging && m_state->getDrawMode() != UI::Types::DrawMode::selectionBox)) {
            //     for (auto &id : m_state->getBulkIds()) {
            //         const auto &entity = Simulator::ComponentsManager::components[id];
            //
            //         // dragable components start from 101
            //         if (static_cast<int>(entity->getType()) <= 100)
            //             return;
            //
            //         auto &pos = entity->getPosition();
            //
            //         if (!dragData.isDragging) {
            //             Types::DragData dragData{};
            //             dragData.isDragging = true;
            //             dragData.orinalEntPos = pos;
            //             dragData.dragOffset = getNVPMousePos() - glm::vec2(pos);
            //             m_state->setDragData(dragData);
            //         }
            //
            //         auto dPos = getNVPMousePos() - dragData.dragOffset;
            //         float snap = 4.f;
            //         dPos = glm::round(dPos / snap) * snap;
            //
            //         entity->setPosition({dPos, 9.f});
            //     }
            // } else if (m_state->getHoveredId() == Simulator::ComponentsManager::emptyRenderId) { // box selection when dragging in empty space
            //     if (!dragData.isDragging) {
            //         Types::DragData dragData{};
            //         dragData.isDragging = true;
            //         dragData.dragOffset = getNVPMousePos();
            //         dragData.vpMousePos = getViewportMousePos();
            //         m_state->setDrawMode(UI::Types::DrawMode::selectionBox);
            //         m_state->setDragData(dragData);
            //         m_state->clearBulkIds();
            //     }
            // }
        }
    }

    bool MainPage::isCursorInViewport() {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        const auto &viewportSize = UI::UIMain::state.viewportSize;
        const auto mousePos = getViewportMousePos();
        return mousePos.x >= 5.f &&
               mousePos.x < viewportSize.x - 5.f &&
               mousePos.y >= 5.f &&
               mousePos.y < viewportSize.y - 5.f;
    }

    glm::vec2 MainPage::getViewportMousePos() {
        const auto &viewportPos = UI::UIMain::state.viewportPos;
        const auto mousePos = m_parentWindow->getMousePos();
        auto x = mousePos.x - viewportPos.x;
        auto y = mousePos.y - viewportPos.y;
        return {x, y};
    }

} // namespace Bess::Pages
