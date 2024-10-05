#pragma once

#include "event_type.h"
#include <any>

namespace Bess::Scene::Events {
    class Event {
      public:
        Event() = default;
        Event(EventType type, const std::any &data);
        ~Event();

        Event(const Event &&other);

        template <typename T>
        T getData() const {
            return std::any_cast<T>(m_data);
        }

        EventType getType() const;

      protected:
        EventType m_type;
        std::any m_data;
    };
} // namespace Bess::Scene::Events
