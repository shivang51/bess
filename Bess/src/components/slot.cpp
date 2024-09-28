#include "components/slot.h"
#include "components/connection.h"
#include "components_manager/components_manager.h"
#include "ext/vector_float3.hpp"
#include "pages/main_page/main_page_state.h"
#include "scene/renderer/renderer.h"
#include "settings/viewport_theme.h"

#include "common/helpers.h"
#include "simulator/simulator_engine.h"
#include "ui/ui.h"
#include <common/bind_helpers.h>
#include <memory>

namespace Bess::Simulator::Components {
    float fontSize = 10.f;
    glm::vec4 connectedBg = {0.42f, 0.82f, 0.42f, 1.f};

    Slot::Slot(const uuids::uuid &uid, const uuids::uuid &parentUid, int id, ComponentType type)
        : Component(uid, id, {0.f, 0.f, 0.f}, type), m_parentUid{parentUid} {
        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_FN_1(Slot::onLeftClick);
        m_events[ComponentEventType::mouseHover] = (VoidCB)BIND_FN(Slot::onMouseHover);
        m_state = DigitalState::low;
    }

    void Slot::update(const glm::vec3 &pos, const std::string &label) {
        m_transform.setPosition(pos);
        setLabel(label);
    }

    void Slot::update(const glm::vec3 &pos, const glm::vec2 &labelOffset) {
        m_transform.setPosition(pos);
        m_labelOffset = labelOffset;
    }

    void Slot::update(const glm::vec3 &pos, const glm::vec2 &labelOffset, const std::string &label) {
        m_transform.setPosition(pos);
        m_labelOffset = labelOffset;
        setLabel(label);
    }

    void Slot::update(const glm::vec3 &pos) {

        m_transform.setPosition(pos);
    }

    void Slot::render() {
        if (m_isHovered) {
            UI::setCursorPointer();
        }

        float r = 4.0f;
        auto pos = m_transform.getPosition();
        auto isHigh = m_state == DigitalState::high;

        Renderer2D::Renderer::circle(pos, m_highlightBorder ? r + 2.0f : r + 1.f,
                                     m_highlightBorder
                                         ? ViewportTheme::selectedWireColor
                                         : ViewportTheme::componentBorderColor,
                                     m_renderId);

        auto bgColor = (isHigh) ? ViewportTheme::stateHighColor : ViewportTheme::stateLowColor;
        Renderer2D::Renderer::circle(pos, r, bgColor, m_renderId);

        if (m_label == "")
            return;
        auto charSize = Renderer2D::Renderer::getCharRenderSize('Z', fontSize);
        glm::vec3 offset = glm::vec3(m_labelOffset, ComponentsManager::zIncrement);
        if (offset.x < 0.f) {
            offset.x -= m_labelWidth;
        }
        offset.y -= charSize.y / 2.f;
        Renderer2D::Renderer::text(m_label, pos + offset, fontSize, ViewportTheme::textColor, ComponentsManager::compIdToRid(m_parentUid));
    }

    void Slot::deleteComponent() {
        m_deleting = true;

        uuids::uuid iId, oId;
        if (m_type == ComponentType::inputSlot)
            iId = m_uid;
        else
            oId = m_uid;

        for (const auto &connSlot : m_connections) {
            if (m_type == ComponentType::inputSlot)
                oId = connSlot;
            else
                iId = connSlot;

            auto &connId = ComponentsManager::getConnectionBetween(iId, oId);
            ComponentsManager::deleteComponent(connId);
        }
    }

    void Slot::onLeftClick(const glm::vec2 &pos) {
        if (Pages::MainPageState::getInstance()->getDrawMode() == UI::Types::DrawMode::none) {
            Pages::MainPageState::getInstance()->setConnStartId(m_uid);
            Pages::MainPageState::getInstance()->setDrawMode(UI::Types::DrawMode::connection);
            return;
        }
        auto connStartId = Pages::MainPageState::getInstance()->getConnStartId();
        auto slot = ComponentsManager::components[connStartId];
        auto connEndId = ComponentsManager::emptyId;
        // conditions for invalid selection
        if (slot == nullptr || connStartId == m_uid || slot->getType() == m_type)
            return;

        auto &points = Pages::MainPageState::getInstance()->getPointsRef();
        auto uid = ComponentsManager::addConnection(Pages::MainPageState::getInstance()->getConnStartId(), m_uid);
        std::shared_ptr<Connection> connection;
        if (uid == ComponentsManager::emptyId)
            goto clear;
        connection = ComponentsManager::getComponent<Connection>(uid);
        connection->setPoints(points);
    clear:
        Pages::MainPageState::getInstance()->setDrawMode(UI::Types::DrawMode::none);
        Pages::MainPageState::getInstance()->setConnStartId(ComponentsManager::emptyId);
        points.clear();
    }

    void Slot::onMouseHover() { UI::setCursorPointer(); }

    void Slot::onChange() {
        if (m_type == ComponentType::outputSlot) {
            for (auto &slot : m_connections) {
                Simulator::Engine::addToSimQueue(slot, m_uid, m_state);
            }
        } else {
            Simulator::Engine::addToSimQueue(m_parentUid, m_uid, m_state);
        }
    }

    void Slot::addConnection(const uuids::uuid &uid, bool simulate) {
        if (isConnectedTo(uid))
            return;
        m_connections.emplace_back(uid);
        if (!simulate || m_type == ComponentType::inputSlot)
            return;
        Simulator::Engine::addToSimQueue(uid, m_uid, m_state);
    }

    bool Slot::isConnectedTo(const uuids::uuid &uId) {
        for (auto &conn : m_connections) {
            if (conn == uId)
                return true;
        }
        return false;
    }

    void Slot::highlightBorder(bool highlight) { m_highlightBorder = highlight; }

    Simulator::DigitalState Slot::getState() const {
        return m_state;
    }

    void Slot::setState(const uuids::uuid &uid, Simulator::DigitalState state, bool forceUpdate) {
        if (m_type == ComponentType::inputSlot) {
            m_stateChangeHistory[uid] = state == DigitalState::high;
            if (state == DigitalState::low) {
                for (auto &ent : m_stateChangeHistory) {
                    if (!ent.second)
                        continue;
                    state = DigitalState::high;
                    break;
                }
            }
        }

        if (m_state == state && !forceUpdate)
            return;
        m_state = state;
        onChange();
    }

    DigitalState Slot::flipState() {
        if (DigitalState::high == m_state) {
            m_state = DigitalState::low;
        } else {
            m_state = DigitalState::high;
        }
        onChange();
        return m_state;
    }

    const uuids::uuid &Slot::getParentId() {
        return m_parentUid;
    }

    void Slot::generate(const glm::vec3 &pos) {
    }

    nlohmann::json Slot::toJson() {
        nlohmann::json data;
        data["uid"] = Common::Helpers::uuidToStr(m_uid);
        data["type"] = (int)m_type;
        for (auto &cid : m_connections)
            data["connections"].emplace_back(Common::Helpers::uuidToStr(cid));
        return data;
    }

    uuids::uuid Slot::fromJson(const nlohmann::json &data, const uuids::uuid &parentuid) {
        uuids::uuid slotId;
        slotId = Common::Helpers::strToUUID(static_cast<std::string>(data["uid"]));

        auto type = Common::Helpers::intToCompType(data["type"]);
        int renderId = Simulator::ComponentsManager::getNextRenderId();

        ComponentsManager::components[slotId] = std::make_shared<Components::Slot>(slotId, parentuid, renderId, type);
        ComponentsManager::addRenderIdToCId(renderId, slotId);
        ComponentsManager::addCompIdToRId(renderId, slotId);
        auto slot = (Slot *)ComponentsManager::components[slotId].get();
        if (data.contains("connections")) {
            for (auto &cid : data["connections"]) {
                uuids::uuid connId = Common::Helpers::strToUUID(cid);
                slot->addConnection(connId, false);
                if (type == ComponentType::outputSlot) {
                    Connection::generate(connId, slotId);
                }
            }
        }

        return slotId;
    }

    const std::string &Slot::getLabel() {
        return m_label;
    }
    void Slot::setLabel(const std::string &label) {
        m_label = label;
        m_labelWidth = Common::Helpers::calculateTextWidth(label, fontSize);
    }
    const glm::vec2 &Slot::getLabelOffset() {
        return m_labelOffset;
    }

    void Slot::setLabelOffset(const glm::vec2 &offset) {
        m_labelOffset = offset;
    }

    const std::vector<uuids::uuid> &Slot::getConnections() {
        return m_connections;
    }

    void Slot::simulate(const uuids::uuid &uid, DigitalState state) {
        setState(uid, state);
    }

    void Slot::refresh(const uuids::uuid &uid, DigitalState state) {
        setState(uid, state, true);
    }

    void Slot::removeConnection(const uuids::uuid &uid) {
        if (m_deleting)
            return;
        int i = 0;
        while (i < m_connections.size() && m_connections[i] != uid)
            i++;
        if (i == m_connections.size())
            return;
        m_connections.erase(m_connections.begin() + i);
        if (m_type == ComponentType::inputSlot && m_stateChangeHistory.find(uid) != m_stateChangeHistory.end())
            m_stateChangeHistory.erase(uid);
        setState(uid, DigitalState::low);
    }
} // namespace Bess::Simulator::Components
