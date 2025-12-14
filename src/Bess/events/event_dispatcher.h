#pragma once

#include <any>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Bess::Events {

    template <typename Event>
    using EventHandler = std::function<void(const Event &)>;

    class EventDispatcher {
      public:
        template <typename Event>
        class Sink {
            EventDispatcher &m_dispatcher;

          public:
            Sink(EventDispatcher &dispatcher) : m_dispatcher(dispatcher) {}

            void connect(EventHandler<Event> handler) {
                m_dispatcher.addListener<Event>(handler);
            }

            // Helper for member functions
            template <auto Candidate, typename Type>
            void connect(Type *instance) {
                m_dispatcher.addListener<Event>([instance](const Event &e) {
                    (instance->*Candidate)(e);
                });
            }

            // Helper for static member functions
            template <auto Candidate>
            void connect() {
                m_dispatcher.addListener<Event>([](const Event &e) {
                    Candidate(e);
                });
            }
        };

        template <typename Event>
        Sink<Event> sink() {
            return Sink<Event>(*this);
        }

        template <typename Event>
        void trigger(const Event &event) {
            auto type = std::type_index(typeid(Event));
            if (m_handlers.contains(type)) {
                for (auto &handlerAny : m_handlers[type]) {
                    const auto &handler = std::any_cast<EventHandler<Event>>(handlerAny);
                    handler(event);
                }
            }
        }

        void clear() {
            m_handlers.clear();
        }

      private:
        template <typename Event>
        void addListener(EventHandler<Event> handler) {
            auto type = std::type_index(typeid(Event));
            m_handlers[type].push_back(handler);
        }

        std::unordered_map<std::type_index, std::vector<std::any>> m_handlers;
    };
} // namespace Bess::Events

