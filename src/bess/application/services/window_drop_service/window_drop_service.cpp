#include "services/window_drop_service/window_drop_service.h"

#include "bess_core/g_app_context.h"
#include "common/logger.h"
#include "event_dispatcher.h"
#include "window.h"

#include <vector>

namespace Bess::Svc {
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

        m_x11SubscriptionId = m_x11WindowDropHandler->subscribe(
            [this](const Platform::X11::WindowDropEvent &event) {
                handleNativeEvent(event);
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
        if (m_x11WindowDropHandler && m_x11SubscriptionId != 0) {
            m_x11WindowDropHandler->unsubscribe(m_x11SubscriptionId);
            m_x11SubscriptionId = 0;
        }
        m_x11WindowDropHandler.reset();
#endif

        m_window.reset();
        m_eventDispatcher.reset();
    }

    void WindowDropService::onDestroy() {
        m_callbacks.clear();
    }

    WindowDropService::SubscriptionId
    WindowDropService::subscribe(Callback callback) {
        if (!callback) {
            return 0;
        }

        const auto subscriptionId = m_nextSubscriptionId++;
        m_callbacks.emplace(subscriptionId, std::move(callback));
        return subscriptionId;
    }

    void WindowDropService::unsubscribe(SubscriptionId subscriptionId) {
        m_callbacks.erase(subscriptionId);
    }

    void WindowDropService::pollNativeHandlers() {
#if defined(__linux__)
        if (m_x11WindowDropHandler) {
            m_x11WindowDropHandler->poll();
        }
#endif
    }

    void WindowDropService::emit(const Events::WindowDropEvent &event) const {
        std::vector<Callback> snapshot;
        snapshot.reserve(m_callbacks.size());
        for (const auto &[_, callback] : m_callbacks) {
            snapshot.push_back(callback);
        }
        for (const auto &callback : snapshot) {
            callback(event);
        }
    }

#if defined(__linux__)
    void WindowDropService::handleNativeEvent(
        const Platform::X11::WindowDropEvent &nativeEvent) {
        if (nativeEvent.type != Platform::X11::WindowDropEventType::drop ||
            !nativeEvent.accepted || !nativeEvent.payload) {
            return;
        }

        const auto window = m_window.lock();
        const auto dispatcher = m_eventDispatcher.lock();
        if (!window || !dispatcher) {
            BESS_WARN("[WindowDrop] Dropped payload could not be dispatched");
            return;
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

        emit(event);
        dispatcher->queue(event);
    }
#endif

} // namespace Bess::Svc
