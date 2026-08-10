#pragma once

#include "common/bess_api.h"

#include "common/events.h"
#include "common/sub_system.h"

#if defined(__linux__)
    #include "platform/x11/x11_window_drop_handler.h"
#endif

#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>

namespace Bess {
    class Window;
    namespace EventSystem {
        class EventDispatcher;
    }
} // namespace Bess

namespace Bess::Svc {
    class BESS_API WindowDropService : public ISubSystem {
      public:
        WindowDropService() = default;
        WindowDropService(const WindowDropService &) = delete;
        WindowDropService &operator=(const WindowDropService &) = delete;

        using SubscriptionId = std::size_t;
        using Callback = std::function<void(const Events::WindowDropEvent &)>;

        void onInit() override;
        void onPostInit() override;
        void onBeginFrame() override;
        void onPreUpdate() override;
        void onShutdown() override;
        void onDestroy() override;

        SubscriptionId subscribe(Callback callback);
        void unsubscribe(SubscriptionId subscriptionId);

      private:
        void pollNativeHandlers();
        void emit(const Events::WindowDropEvent &event) const;

#if defined(__linux__)
        void handleNativeEvent(const Platform::X11::WindowDropEvent &event);
#endif

      private:
        SubscriptionId m_nextSubscriptionId = 1;
        std::unordered_map<SubscriptionId, Callback> m_callbacks;
        std::weak_ptr<Window> m_window;
        std::weak_ptr<EventSystem::EventDispatcher> m_eventDispatcher;

#if defined(__linux__)
        std::unique_ptr<Platform::X11::WindowDropHandler>
            m_x11WindowDropHandler;
        Platform::X11::WindowDropHandler::SubscriptionId m_x11SubscriptionId =
            0;
#endif
    };

} // namespace Bess::Svc
