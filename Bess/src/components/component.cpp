#include "components/component.h"
#include "pages/main_page/main_page_state.h"

namespace Bess::Simulator::Components {

    Component::Component(const uuids::uuid &uid, int renderId, glm::vec3 position,
                         ComponentType type)
        : m_uid(uid), m_renderId(renderId), m_type(type) {

        m_transform.setPosition(position);
    }

    int Component::getRenderId() const { return m_renderId; }

    uuids::uuid Component::getId() const { return m_uid; }

    const glm::vec3 &Component::getPosition() { return m_transform.getPosition(); }

    void Component::setPosition(const glm::vec3 &pos) {
        m_transform.setPosition(pos);
    }

    std::string Component::getIdStr() const {
        return uuids::to_string(m_uid);
    }

    ComponentType Component::getType() const { return m_type; }

    void Component::simulate() {}

    void Component::onEvent(ComponentEventData e) {
        if (m_events.find(e.type) == m_events.end())
            return;

        m_eventsQueue.push(e);
    }

    std::string Component::getName() const {
        return m_name;
    }

    std::string Component::getRenderName() const {
        std::string name = m_name + " " + std::to_string(m_renderId);
        return name;
    }

    void Component::drawProperties() {}

    void Component::update() {
        auto mainPageState = Pages::MainPageState::getInstance();
        m_isSelected = mainPageState->isBulkIdPresent(m_uid);
        m_isHovered = mainPageState->getHoveredId() == m_renderId;

        while (!m_eventsQueue.empty()) {
            auto e = m_eventsQueue.front();
            m_eventsQueue.pop();

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
    }

} // namespace Bess::Simulator::Components
