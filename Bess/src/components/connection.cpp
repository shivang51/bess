#include "components/connection.h"
#include "common/bind_helpers.h"
#include "common/helpers.h"
#include "components/connection_point.h"
#include "components/slot.h"
#include "components_manager/components_manager.h"
#include "ext/vector_float3.hpp"
#include "ext/vector_float4.hpp"
#include "pages/main_page/main_page_state.h"
#include "scene/renderer/renderer.h"
#include "ui/m_widgets.h"
#include "ui/ui.h"
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

    void Connection::renderCurveConnection(glm::vec3 startPos, glm::vec3 endPos, float weight, glm::vec4 color) {
        auto slot = ComponentsManager::getComponent<Slot>(m_slot1);
        auto posA = startPos;
        auto pos = m_transform.getPosition();
        for (auto &pointId : m_points) {
            auto point = std::dynamic_pointer_cast<ConnectionPoint>(ComponentsManager::components[pointId]);
            point->render();
            auto posB = point->getPosition();
            Renderer2D::Renderer::curve(
                {posA.x, posA.y, pos.z},
                {posB.x, posB.y, pos.z},
                m_isHovered ? 2.5f : 2.0f,
                m_isSelected ? ViewportTheme::selectedWireColor : (slot->getState() == DigitalState::high) ? ViewportTheme::stateHighColor
                                                                                                           : m_color,
                m_renderId);
            posA = posB;
        }
        auto posB = endPos;
        Renderer2D::Renderer::curve(
            {posA.x, posA.y, pos.z},
            {posB.x, posB.y, pos.z},
            m_isHovered ? 2.5f : 2.0f,
            m_isSelected ? ViewportTheme::selectedWireColor : (slot->getState() == DigitalState::high) ? ViewportTheme::stateHighColor
                                                                                                       : m_color,
            m_renderId);
    }

    void Connection::renderStraightConnection(glm::vec3 startPos, glm::vec3 endPos, float weight, glm::vec4 color) {
        auto z = -ComponentsManager::zIncrement;
        std::vector<glm::vec3> points = {{startPos.x, startPos.y, z}};
        points.emplace_back(startPos + glm::vec3({30.f, 0.f, 0.f}));
        for (auto &pointId : m_points) {
            auto point = std::dynamic_pointer_cast<ConnectionPoint>(ComponentsManager::components[pointId]);
            points.emplace_back(point->getPosition());
            point->render();
        }
        points.emplace_back(endPos - glm::vec3({30.f, 0.f, 0.f}));
        points.emplace_back(glm::vec3({endPos.x, endPos.y, z}));

        for (int i = 0; i < points.size() - 1; i++) {
            auto sPos = points[i];
            auto ePos = points[i + 1];
            float midX = ePos.x;
            float offset = weight / 2.f;

            if (sPos.y > ePos.y)
                offset = -offset;

            sPos.z = z;
            ePos.z = z;

            Renderer2D::Renderer::line(sPos, {midX, sPos.y, z}, weight, color, m_renderId);
            Renderer2D::Renderer::line({midX, sPos.y - offset, z}, {midX, ePos.y + offset, z}, weight, color, m_renderId);
            Renderer2D::Renderer::line({midX, ePos.y, z}, ePos, weight, color, m_renderId);
        }
    }

    void Connection::render() {
        auto slotA = ComponentsManager::components[m_slot1];
        auto slotB = ComponentsManager::components[m_slot2];

        auto slot = ComponentsManager::getComponent<Slot>(m_slot1);

        auto startPos = slotB->getPosition();
        auto endPos = slotA->getPosition();

        float weight = m_isHovered ? 2.5f : 2.0f;
        glm::vec4 color = m_isSelected ? ViewportTheme::selectedWireColor : (slot->getState() == DigitalState::high) ? ViewportTheme::stateHighColor
                                                                                                                     : m_color;

        if (m_type == ConnectionType::curve) {
            renderCurveConnection(startPos, endPos, weight, color);
        } else {
            renderStraightConnection(startPos, endPos, weight, color);
        }
    }

    void Connection::update() {
        Component::update();
        if (m_isSelected) {
            for (auto &cpId : m_points) {
                auto cp = std::dynamic_pointer_cast<ConnectionPoint>(ComponentsManager::components[cpId]);
                cp->update();
                cp->setSelected(true);
            }
        }
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
        static std::string currentValue = (m_type == ConnectionType::straight) ? "Straight" : "Curve";
        if (UI::MWidgets::ComboBox("Connection Type", currentValue, std::vector<std::string>{"Straight", "Curve"})) {
            if (currentValue == "Straight") {
                m_type = ConnectionType::straight;
            } else {
                m_type = ConnectionType::curve;
            }
        }
    }

    const std::vector<uuids::uuid> &Connection::getPoints() {
        return m_points;
    }

    void Connection::setPoints(const std::vector<glm::vec3> &points) {
        m_points.clear();
        for (const auto &point : points) {
            auto uid = Common::Helpers::uuidGenerator.getUUID();
            auto renderId = ComponentsManager::getNextRenderId();
            ComponentsManager::components[uid] = std::make_shared<ConnectionPoint>(uid, m_uid, renderId, point);
            ComponentsManager::addRenderIdToCId(renderId, uid);
            ComponentsManager::addCompIdToRId(renderId, uid);
            m_points.emplace_back(uid);
        }
    }

    void Connection::addPoint(const uuids::uuid &point) {
        m_points.emplace_back(point);
    }

    void Connection::removePoint(const uuids::uuid &point) {
        m_points.erase(std::remove(m_points.begin(), m_points.end(), point), m_points.end());
    }

    uuids::uuid Connection::generate(const uuids::uuid &slot1, const uuids::uuid &slot2, const glm::vec3 &pos) {
        auto uid = Common::Helpers::uuidGenerator.getUUID();
        auto renderId = ComponentsManager::getNextRenderId();
        ComponentsManager::components[uid] = std::make_shared<Components::Connection>(uid, renderId, slot1, slot2);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        ComponentsManager::renderComponents.emplace_back(uid);
        ComponentsManager::addSlotsToConn(slot1, slot2, uid);
        return uid;
    }

    void Connection::onLeftClick(const glm::vec2 &pos) {
        Pages::MainPageState::getInstance()->setBulkId(m_uid);

        if (!Pages::MainPageState::getInstance()->isKeyPressed(GLFW_KEY_LEFT_CONTROL))
            return;

        createConnectionPoint(pos);
    }

    void Connection::onMouseHover() {
        UI::setCursorPointer();
    }

    void Connection::onFocusLost() {
        auto slotA = (Slot *)ComponentsManager::components[m_slot1].get();
        auto slotB = (Slot *)ComponentsManager::components[m_slot2].get();
        slotA->highlightBorder(false);
        slotB->highlightBorder(false);
        for (auto &cpId : m_points) {
            auto cp = std::dynamic_pointer_cast<ConnectionPoint>(ComponentsManager::components[cpId]);
            cp->update();
            cp->setSelected(false);
        }
    }

    void Connection::onFocus() {
        auto slotA = (Slot *)ComponentsManager::components[m_slot1].get();
        auto slotB = (Slot *)ComponentsManager::components[m_slot2].get();
        slotA->highlightBorder(true);
        slotB->highlightBorder(true);
    }

    void Connection::createConnectionPoint(const glm::vec2 &pos) {
        auto uid = Common::Helpers::uuidGenerator.getUUID();
        auto renderId = ComponentsManager::getNextRenderId();
        auto parentId = m_uid;
        auto position = glm::vec3(pos.x, pos.y, -ComponentsManager::zIncrement);
        ComponentsManager::components[uid] = std::make_shared<ConnectionPoint>(uid, parentId, renderId, position);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
        m_points.emplace_back(uid);
    }
} // namespace Bess::Simulator::Components
