#include "services/window_drop_service/window_drop_service.h"

#include "bess_core/g_app_context.h"
#include "common/logger.h"
#include "event_dispatcher.h"
#include "window.h"

#include <algorithm>
#include <exception>
#include <vector>

namespace Bess::Svc {
    namespace {
        template <typename Callbacks, typename Event>
        void notifySubscribers(const Callbacks &callbacks, const Event &event) {
            std::vector<typename Callbacks::key_type> snapshot;
            snapshot.reserve(callbacks.size());
            for (const auto &[subscription, callback] : callbacks) {
                static_cast<void>(callback);
                snapshot.push_back(subscription);
            }
            for (const auto subscription : snapshot) {
                const auto found = callbacks.find(subscription);
                if (found == callbacks.end()) {
                    continue;
                }
                // Copy before invocation: the callback may unsubscribe and
                // destroy the callable that is stored in the map.
                auto callback = found->second;
                try {
                    callback(event);
                } catch (const std::exception &exception) {
                    BESS_ERROR("[WindowDrop] Observer callback failed: {}",
                               exception.what());
                } catch (...) {
                    BESS_ERROR("[WindowDrop] Observer callback failed");
                }
            }
        }
    } // namespace

    void WindowDropService::onInit() {
    }

    void WindowDropService::onPostInit() {
        auto &appCtx = GAppContext::getInstance();
        const auto window = appCtx.getSubSystem<Window>();
        if (!window) {
            BESS_WARN("[WindowDrop] Window is unavailable; drops are disabled");
            return;
        }

        m_window = window;
        m_eventDispatcher = appCtx.getSubSystem<EventSystem::EventDispatcher>();

#if defined(__linux__)
        if (!window->isNativeX11()) {
            return;
        }

        auto *display = static_cast<_XDisplay *>(window->getNativeX11Display());
        const auto targetWindow = static_cast<Platform::X11::X11WindowHandle>(
            window->getNativeX11Window());
        if (!display || targetWindow == 0) {
            BESS_WARN("[WindowDrop] X11 window handles are unavailable; drops "
                      "are disabled");
            return;
        }

        m_x11WindowDropHandler =
            std::make_unique<Platform::X11::WindowDropHandler>(display,
                                                               targetWindow);
        if (!m_x11WindowDropHandler->isInitialized()) {
            BESS_WARN("[WindowDrop] Native X11 handler initialization failed");
            m_x11WindowDropHandler.reset();
            return;
        }

        m_x11WindowDropHandler->setAcceptanceCallback(
            [this](const Platform::X11::WindowDropEvent &event) {
                return handleNativeEvent(event);
            });

        BESS_INFO("[WindowDrop] Native X11 handler enabled");
#endif
    }

    void WindowDropService::onBeginFrame() {
        pollNativeHandlers();
    }

    void WindowDropService::onPreUpdate() {
        pollNativeHandlers();
    }

    void WindowDropService::onShutdown() {
#if defined(__linux__)
        if (m_x11WindowDropHandler) {
            m_x11WindowDropHandler->setAcceptanceCallback({});
        }
        m_x11WindowDropHandler.reset();
#endif

        m_window.reset();
        m_eventDispatcher.reset();
    }

    void WindowDropService::onDestroy() {
        m_callbacks.clear();
        m_dragCallbacks.clear();
        m_dragDecisionCallbacks.clear();
        m_nextSubscriptionId = 1;
    }

    WindowDropService::SubscriptionId
    WindowDropService::allocateSubscriptionId() noexcept {
        // Zero remains the invalid token. IDs are shared by both subscriber
        // sets so callers cannot accidentally pass a live token to the wrong
        // unsubscribe overload after wraparound.
        SubscriptionId candidate = 0;
        do {
            candidate = m_nextSubscriptionId++;
            if (m_nextSubscriptionId == 0) {
                m_nextSubscriptionId = 1;
            }
        } while (candidate == 0 || m_callbacks.contains(candidate) ||
                 m_dragCallbacks.contains(candidate) ||
                 m_dragDecisionCallbacks.contains(candidate));
        return candidate;
    }

    WindowDropService::SubscriptionId
    WindowDropService::subscribe(Callback callback) {
        if (!callback) {
            return 0;
        }

        const auto subscriptionId = allocateSubscriptionId();
        m_callbacks.emplace(subscriptionId, std::move(callback));
        return subscriptionId;
    }

    void WindowDropService::unsubscribe(SubscriptionId subscriptionId) {
        m_callbacks.erase(subscriptionId);
    }

    WindowDropService::SubscriptionId
    WindowDropService::subscribeDrag(DragCallback callback) {
        if (!callback) {
            return 0;
        }

        const auto subscriptionId = allocateSubscriptionId();
        m_dragCallbacks.emplace(subscriptionId, std::move(callback));
        return subscriptionId;
    }

    void WindowDropService::unsubscribeDrag(SubscriptionId subscriptionId) {
        m_dragCallbacks.erase(subscriptionId);
    }

    WindowDropService::SubscriptionId
    WindowDropService::subscribeDragDecision(DragDecisionCallback callback) {
        if (!callback) {
            return 0;
        }
        const auto subscriptionId = allocateSubscriptionId();
        m_dragDecisionCallbacks.emplace(subscriptionId, std::move(callback));
        return subscriptionId;
    }

    void
    WindowDropService::unsubscribeDragDecision(SubscriptionId subscriptionId) {
        m_dragDecisionCallbacks.erase(subscriptionId);
    }

    void WindowDropService::pollNativeHandlers() {
#if defined(__linux__)
        if (m_x11WindowDropHandler) {
            m_x11WindowDropHandler->poll();
        }
#endif
    }

    void WindowDropService::emit(const Events::WindowDropEvent &event) const {
        notifySubscribers(m_callbacks, event);
    }

    void WindowDropService::emitDrag(
        const Events::WindowDragDropEvent &event) const {
        notifySubscribers(m_dragCallbacks, event);
    }

    bool WindowDropService::emitDragDecision(
        const Events::WindowDragDropEvent &event) const {
        std::vector<SubscriptionId> snapshot;
        snapshot.reserve(m_dragDecisionCallbacks.size());
        for (const auto &[subscription, callback] : m_dragDecisionCallbacks) {
            static_cast<void>(callback);
            snapshot.push_back(subscription);
        }
        std::ranges::sort(snapshot);

        for (const auto subscription : snapshot) {
            const auto found = m_dragDecisionCallbacks.find(subscription);
            if (found == m_dragDecisionCallbacks.end()) {
                continue;
            }
            auto callback = found->second;
            try {
                if (callback(event)) {
                    // Decision callbacks form an ordered chain of
                    // responsibility. A successful handler may already have
                    // committed a model mutation, so invoking later handlers
                    // could process the same native drop more than once.
                    return true;
                }
            } catch (const std::exception &exception) {
                BESS_ERROR("[WindowDrop] Decision callback failed: {}",
                           exception.what());
            } catch (...) {
                BESS_ERROR("[WindowDrop] Decision callback failed");
            }
        }
        return false;
    }

#if defined(__linux__)
    bool WindowDropService::handleNativeEvent(
        const Platform::X11::WindowDropEvent &nativeEvent) {
        const auto window = m_window.lock();
        if (!window) {
            return false;
        }

        const auto eventType = [&] {
            switch (nativeEvent.type) {
            case Platform::X11::WindowDropEventType::enter:
                return Events::WindowDragDropEventType::enter;
            case Platform::X11::WindowDropEventType::position:
                return Events::WindowDragDropEventType::move;
            case Platform::X11::WindowDropEventType::leave:
                return Events::WindowDragDropEventType::leave;
            case Platform::X11::WindowDropEventType::drop:
                return Events::WindowDragDropEventType::drop;
            }
            return Events::WindowDragDropEventType::leave;
        }();

        const Events::WindowDragDropEvent dragEvent{
            .window = window.get(),
            .type = eventType,
            .payload = nativeEvent.payload,
            .x = nativeEvent.x,
            .y = nativeEvent.y,
            .acceptedByPlatform = nativeEvent.accepted,
        };
        const bool acceptedByInteractiveTarget = emitDragDecision(dragEvent);
        emitDrag(dragEvent);

        if (nativeEvent.type != Platform::X11::WindowDropEventType::drop ||
            !nativeEvent.accepted || !nativeEvent.payload) {
            const bool hasCompatibilityTarget =
                !m_callbacks.empty() || !m_eventDispatcher.expired();
            return nativeEvent.accepted &&
                   (acceptedByInteractiveTarget || hasCompatibilityTarget);
        }

        // A retained DropZone already committed the drop synchronously. Do
        // not also publish the compatibility WindowDropEvent, which would
        // import the same payload a second time through legacy consumers.
        if (acceptedByInteractiveTarget) {
            return true;
        }

        Events::WindowDropEvent event{
            .window = window.get(),
            .payload = nativeEvent.payload,
            .x = nativeEvent.x,
            .y = nativeEvent.y,
        };

        BESS_INFO("[WindowDrop] mime={} bytes={} paths={} pos=({}, {})",
                  event.payload->mimeType,
                  event.payload->data.size(),
                  event.payload->paths.size(),
                  event.x,
                  event.y);

        bool delivered = false;
        if (!m_callbacks.empty()) {
            emit(event);
            delivered = true;
        }
        if (const auto dispatcher = m_eventDispatcher.lock()) {
            dispatcher->queue(event);
            delivered = true;
        }
        if (!delivered) {
            BESS_WARN("[WindowDrop] Dropped payload had no consumer");
        }
        return delivered;
    }
#endif

} // namespace Bess::Svc
