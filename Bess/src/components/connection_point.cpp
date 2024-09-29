#include "components/connection_point.h"
#include "common/bind_helpers.h"
#include "common/helpers.h"
#include "components/component.h"
#include "components/connection.h"
#include "components_manager/components_manager.h"
#include "pages/main_page/main_page_state.h"
#include "scene/renderer/renderer.h"
#include "settings/viewport_theme.h"

namespace Bess::Simulator::Components {
    ConnectionPoint::ConnectionPoint(const uuids::uuid &uid,
                                     const uuids::uuid &parentId, int renderId,
                                     const glm::vec3 &pos)
        : Component(uid, renderId, pos, ComponentType::connectionPoint) {
        m_parentId = parentId;
        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_FN_1(ConnectionPoint::onLeftClick);
        m_name = "Connection Point";
    }

    void ConnectionPoint::render() {
        float r = 3.0f;
        if (m_isHovered || m_isSelected) {
            r = 4.5f;
            Renderer2D::Renderer::circle(m_transform.getPosition(), r + 1.f, ViewportTheme::selectedWireColor, m_renderId);
        }
        Renderer2D::Renderer::circle(m_transform.getPosition(), r, ViewportTheme::wireColor, m_renderId);
    }

    void ConnectionPoint::deleteComponent() {
        auto connection = std::dynamic_pointer_cast<Connection>(
            ComponentsManager::components[m_parentId]);
        connection->removePoint(m_uid);
    }

    void ConnectionPoint::generate(const glm::vec3 &pos) {}

    void ConnectionPoint::drawProperties() {}

    void ConnectionPoint::setSelected(bool selected) {
        m_isSelected = selected;
    }

    void ConnectionPoint::fromJson(const nlohmann::json &j) {
        auto uid = Common::Helpers::strToUUID(j["uid"].get<std::string>());
        auto parentSlotsId = j["parentSlots"].get<std::string>();
        auto position = Common::Helpers::DecodeVec3(j["position"]);
        auto renderId = ComponentsManager::getNextRenderId();

        auto parentId = ComponentsManager::getConnectionBetween(parentSlotsId);
        auto connComp = std::dynamic_pointer_cast<Connection>(ComponentsManager::components[parentId]);
        connComp->addPoint(uid);

        ComponentsManager::components[uid] = std::make_shared<ConnectionPoint>(uid, parentId, renderId, position);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
    }

    nlohmann::json ConnectionPoint::toJson() {
        nlohmann::json j;
        j["type"] = (int)ComponentType::connectionPoint;
        j["uid"] = getIdStr();
        j["parentSlots"] = ComponentsManager::getSlotsForConnection(m_parentId);
        j["position"] = Common::Helpers::EncodeVec3(m_transform.getPosition());
        return j;
    }

    void ConnectionPoint::onLeftClick(const glm::vec2 &pos) {
        Pages::MainPageState::getInstance()->setBulkId(m_uid);
    }
} // namespace Bess::Simulator::Components
