#include "events/application_event.h"

namespace Bess {
    ApplicationEvent::ApplicationEvent(const ApplicationEventType type, const std::any &data) : m_type(type), m_data(data) {
    }

    ApplicationEventType ApplicationEvent::getType() const {
        return m_type;
    }
} // namespace Bess
