#include "components/component.h"

namespace Bess::Simulator::Components {
Component::Component(const UUIDv4::UUID &uid, int renderId, glm::vec3 position,
                     ComponentType type)
    : m_uid(uid), m_renderId(renderId), m_position(position), m_type(type) {}

int Component::getRenderId() const { return m_renderId; }

UUIDv4::UUID Component::getId() const { return m_uid; }

glm::vec3 &Component::getPosition() { return m_position; }

std::string Component::getIdStr() const {
    std::string idStr;
    m_uid.str(idStr);
    return idStr;
}

ComponentType Component::getType() const { return m_type; }

void Component::simulate(){}

void Component::onEvent(ComponentEventData e) {
    if (m_events.find(e.type) == m_events.end())
        return;

    switch (e.type) {
    case ComponentEventType::leftClick: {
        auto cb = std::any_cast<OnLeftClickCB>(m_events[e.type]);
        cb(e.pos);
    } break;
    case ComponentEventType::rightClick: {
        auto cb = std::any_cast<OnRightClickCB>(m_events[e.type]);
        cb(e.pos);
    } break;
    case ComponentEventType::mouseEnter:
    case ComponentEventType::mouseLeave:
    case ComponentEventType::mouseHover:
    case ComponentEventType::focus:
    case ComponentEventType::focusLost: {
        auto cb = std::any_cast<VoidCB>(m_events[e.type]);
        cb();
    } break;
    default:
        break;
    }
}

std::string Component::getName() const
{
    return m_name;
}

std::string Component::getRenderName() const {
    std::string name = m_name + " " + std::to_string(m_renderId);
    return name;
}

} // namespace Bess::Simulator::Components
