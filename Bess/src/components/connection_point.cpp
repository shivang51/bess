#include "components/connection_point.h"
#include "renderer/renderer.h"
#include "common/theme.h"
#include "components_manager/components_manager.h"
#include "components/connection.h"
#include "common/helpers.h"
#include "common/bind_helpers.h"
#include "application_state.h"

namespace Bess::Simulator::Components {
    ConnectionPoint::ConnectionPoint(const uuids::uuid& uid, const uuids::uuid& parentId, int renderId, const glm::vec3& pos): Component(uid, renderId, pos, ComponentType::connectionPoint)
    {
        m_parentId = parentId;
        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_FN_1(ConnectionPoint::onLeftClick);
        m_name = "Connection Point";
    }

    void ConnectionPoint::render()
    {
        Renderer2D::Renderer::circle(m_position, 4.0f, Theme::wireColor, m_renderId);
    }

    void ConnectionPoint::deleteComponent()
    {
        auto connection = std::dynamic_pointer_cast<Connection>(ComponentsManager::components[m_parentId]);
        connection->removePoint(m_uid);
    }

    void ConnectionPoint::generate(const glm::vec3& pos)
    {
    }

    void ConnectionPoint::drawProperties()
    {
    }

    void ConnectionPoint::fromJson(const nlohmann::json& j)
    {
        auto uid = Common::Helpers::strToUUID(j["uid"].get<std::string>());
        auto parentId = Common::Helpers::strToUUID(j["parentId"].get<std::string>());
        auto position = Common::Helpers::DecodeVec3(j["position"]);
        auto renderId = ComponentsManager::getNextRenderId();

        ComponentsManager::components[uid] = std::make_shared<ConnectionPoint>(uid, parentId, renderId, position);
        ComponentsManager::addRenderIdToCId(renderId, uid);
        ComponentsManager::addCompIdToRId(renderId, uid);
    }

    nlohmann::json ConnectionPoint::toJson()
    {
        nlohmann::json j;
        j["type"] = (int)ComponentType::connectionPoint;
        j["uid"] = getIdStr();
        j["parentId"] = Common::Helpers::uuidToStr(m_parentId);
        j["position"] = Common::Helpers::EncodeVec3(m_position);
        return j;
    }

    void ConnectionPoint::onLeftClick(const glm::vec2& pos)
    {
        ApplicationState::setSelectedId(m_uid);
    }
}