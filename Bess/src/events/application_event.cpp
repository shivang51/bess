#include "events/application_event.h"

namespace Bess {
    ApplicationEvent::ApplicationEvent(ApplicationEventType type, std::any data) : m_type(type), m_data(data) {
    }

    ApplicationEventType ApplicationEvent::getType() const {
        return m_type;
    }
} // namespace Bess
