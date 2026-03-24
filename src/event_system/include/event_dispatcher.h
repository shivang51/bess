#pragma once

#include "common/logger.h"
#include <any>
#include <functional>
#include <queue>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Bess::EventSystem {

    template <typename Event>
    using EventHandler = std::function<void(const Event &)>;

    class EventDispatcher {
      public:
        static EventDispatcher &instance() {
            static EventDispatcher dispatcher;
            return dispatcher;
        }

        void dispatchAll() {
            // BESS_TRACE("[EventDispatcher] Dispatching All");
            while (!m_eventQueue.empty()) {
                auto queue = std::queue<std::function<void()>>{};
                queue.swap(m_eventQueue);
                // BESS_TRACE("Dispatching {} events", queue.size());
                while (!queue.empty()) {
                    auto &eventFn = queue.front();
                    eventFn();
                    queue.pop();
                }
            }
            // BESS_TRACE("[EventDispatcher] Finished Dispatching All");
        }

        template <typename Event>
        class Sink {
          public:
            Sink() = default;

            void connect(EventHandler<Event> handler) {
                EventDispatcher::instance().addListener<Event>(handler);
            }

            // Helper for member functions
            template <auto Candidate, typename Type>
            void connect(Type *instance) {
                EventDispatcher::instance().addListener<Event>([instance](const Event &e) {
                    (instance->*Candidate)(e);
                });
            }

            // Helper for static member functions
            template <auto Candidate>
            void connect() {
                EventDispatcher::instance().addListener<Event>([](const Event &e) {
                    Candidate(e);
                });
            }
        };

        template <typename Event>
        Sink<Event> sink() {
            return Sink<Event>();
        }

        template <typename Event>
        void queue(const Event &event) {
            // BESS_DEBUG("[EventSystem] Queueing event of type {}",
            //            std::type_index(typeid(Event)).name());
            m_eventQueue.push([this, event]() {
                dispatch(event);
            });
        }

        template <typename Event>
        void dispatch(const Event &event) {
            auto type = std::type_index(typeid(Event));
            BESS_DEBUG("[EventSystem] Dispatching event of type {}", type.name());
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
        std::queue<std::function<void()>> m_eventQueue;
    };
} // namespace Bess::EventSystem
