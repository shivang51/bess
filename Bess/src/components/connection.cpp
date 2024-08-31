#include "components/connection.h"
#include "components/connection_point.h"
#include "components/slot.h"
#include "ui/ui.h"

#include "application_state.h"
#include "common/bind_helpers.h"
#include "common/helpers.h"
#include <imgui.h>

namespace Bess::Simulator::Components {

    Connection::Connection(const uuids::uuid &uid, int renderId,
                           const uuids::uuid &slot1, const uuids::uuid &slot2)
        : Component(uid, renderId, {0.f, 0.f, -1.f}, ComponentType::connection) {
        m_slot1 = slot1;
        m_slot2 = slot2;
        m_events[ComponentEventType::leftClick] =
            (OnLeftClickCB)BIND_FN_1(Connection::onLeftClick);
        m_events[ComponentEventType::focusLost] =
            (VoidCB)BIND_FN(Connection::onFocusLost);
        m_events[ComponentEventType::focus] =
            (VoidCB)BIND_FN(Connection::onFocus);
        m_events[ComponentEventType::mouseHover] = (VoidCB)BIND_FN(Connection::onMouseHover);

        m_name = "Connection";
    }

    Connection::Connection() : Component() {
    }

    void Connection::render() {
        auto slotA = ComponentsManager::components[m_slot1];
        auto slotB = ComponentsManager::components[m_slot2];
        auto m_selected = ApplicationState::getSelectedId() == m_uid;
        auto m_hovered = ApplicationState::hoveredId == m_renderId;

        auto slot = (Components::Slot *)slotA.get();

        auto startPos = slotB->getPosition();

        auto endPos = slotA->getPosition();

        auto posA = startPos;
        for (auto &pointId : m_points) {
            auto point = std::dynamic_pointer_cast<ConnectionPoint>(ComponentsManager::components[pointId]);
            point->render();
            auto posB = point->getPosition();
            Renderer2D::Renderer::curve(
                {posA.x, posA.y, m_position.z},
                {posB.x, posB.y, m_position.z},
                m_hovered ? 2.5f : 2.0f,
                m_selected ? ViewportTheme::selectedWireColor : (slot->getState() == DigitalState::high) ? ViewportTheme::stateHighColor
                                                                                                         : m_color,
                m_renderId);
            posA = posB;
        }
        auto posB = endPos;
        Renderer2D::Renderer::curve(
            {posA.x, posA.y, m_position.z},
            {posB.x, posB.y, m_position.z},
            m_hovered ? 2.5f : 2.0f,
            m_selected ? ViewportTheme::selectedWireColor : (slot->getState() == DigitalState::high) ? ViewportTheme::stateHighColor
                                                                                                     : m_color,
            m_renderId);
    }

    void Connection::deleteComponent() {
        auto slotA = (Slot *)ComponentsManager::components[m_slot1].get();
        slotA->removeConnection(m_slot2);
        auto slotB = (Slot *)ComponentsManager::components[m_slot2].get();
        slotB->removeConnection(m_slot1);

        if (slotA->getType() == ComponentType::inputSlot) {
            ComponentsManager::removeSlotsToConn(m_slot1, m_slot2);
        } else {
            ComponentsManager::removeSlotsToConn(m_slot2, m_slot1);
        }
    }

    void Connection::generate(const glm::vec3 &pos) {}

    void Connection::drawProperties() {
        ImGui::ColorEdit3("Wire Color", &m_color[0]);
    }

    const std::vector<uuids::uuid> &Connection::getPoints() {
        return m_points;
    }

    void Connection::setPoints(const std::vector<uuids::uuid> &points) {
        m_points = points;
    }

    void Connection::addPoint(const uuids::uuid &point) {
        m_points.emplace_back(point);
    }

    void Connection::removePoint(const uuids::uuid &point) {
        m_points.erase(std::remove(m_points.begin(), m_points.end(), point), m_points.end());
    }

    void Connection::generate(const uuids::uuid &slot1, const uuids::uuid &slot2, const glm::vec3 &pos) {
        auto uid = Common::Helpers::uuidGenerator.getUUID();
        auto renderId = ComponentsManager::getNextRenderId();
        ComponentsManager::components[uid] = std::make_shared<Components::Connection>(uid, renderId, slot1, slot2);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        ComponentsManager::renderComponenets.emplace_back(uid);
        ComponentsManager::addSlotsToConn(slot1, slot2, uid);
    }

    void Connection::onLeftClick(const glm::vec2 &pos) {
        ApplicationState::setSelectedId(m_uid);

        if (!ApplicationState::isKeyPressed(GLFW_KEY_LEFT_CONTROL))
            return;

        // add connection point
        auto uid = Common::Helpers::uuidGenerator.getUUID();
        auto renderId = ComponentsManager::getNextRenderId();
        auto parentId = m_uid;
        auto position = glm::vec3(pos.x, pos.y, m_position.z + ComponentsManager::zIncrement);
        ComponentsManager::components[uid] = std::make_shared<ConnectionPoint>(uid, parentId, renderId, position);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        m_points.emplace_back(uid);
    }

    void Connection::onMouseHover() {
        UI::setCursorPointer();
    }

    void Connection::onFocusLost() {
        auto slotA = (Slot *)ComponentsManager::components[m_slot1].get();
        auto slotB = (Slot *)ComponentsManager::components[m_slot2].get();
        slotA->highlightBorder(false);
        slotB->highlightBorder(false);
    }

    void Connection::onFocus() {
        auto slotA = (Slot *)ComponentsManager::components[m_slot1].get();
        auto slotB = (Slot *)ComponentsManager::components[m_slot2].get();
        slotA->highlightBorder(true);
        slotB->highlightBorder(true);
    }
} // namespace Bess::Simulator::Components
