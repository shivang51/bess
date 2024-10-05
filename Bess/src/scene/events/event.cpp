#include "scene/events/event.h"

namespace Bess::Scene::Events {

    Event::Event(EventType type, const std::any &data) : m_type(type), m_data(data) {}

    Event::~Event() {}

    Event::Event(const Event &&other) : m_type(other.m_type), m_data(other.m_data) {}

    EventType Event::getType() const { return m_type; }
} // namespace Bess::Scene::Events
