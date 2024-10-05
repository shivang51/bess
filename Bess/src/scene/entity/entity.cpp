
#include "scene/entity/entity.h"
#include "scene/events/event_type.h"

namespace Bess::Scene {
    void Entity::onEvent(const Events::EntityEvent &evt) {
        m_events.push(evt);
    }

    void Entity::update() {
        while (!m_events.empty()) {
            const auto &evt = m_events.front();

            switch (evt.getType()) {
            case Events::EventType::mouseButton: {
            } break;
            case Events::EventType::mouseMove: {
            } break;
            case Events::EventType::mouseWheel: {
            } break;
            case Events::EventType::keyPress: {
            } break;
            case Events::EventType::keyRelease: {
            } break;
            case Events::EventType::mouseEnter: {
            } break;
            case Events::EventType::mouseLeave: {
            } break;
            case Events::EventType::mouse:
            }

            m_events.pop();
        }
    }
} // namespace Bess::Scene
