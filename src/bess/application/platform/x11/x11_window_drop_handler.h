#pragma once

#include "common/events.h"

#include <cstddef>
#include <functional>
#include <memory>

struct _XDisplay;

namespace Bess::Platform::X11 {

    using X11WindowHandle = unsigned long;

    enum class WindowDropEventType {
        enter,
        position,
        leave,
        drop,
    };

    struct WindowDropEvent {
        WindowDropEventType type = WindowDropEventType::enter;
        std::shared_ptr<const Events::WindowDropPayload> payload;
        int x = 0;
        int y = 0;
        bool accepted = false;
    };

    class WindowDropHandler {
      public:
        using SubscriptionId = std::size_t;
        using Callback = std::function<void(const WindowDropEvent &)>;

        WindowDropHandler(_XDisplay *display, X11WindowHandle targetWindow);
        ~WindowDropHandler();

        WindowDropHandler(const WindowDropHandler &) = delete;
        WindowDropHandler &operator=(const WindowDropHandler &) = delete;
        WindowDropHandler(WindowDropHandler &&) noexcept;
        WindowDropHandler &operator=(WindowDropHandler &&) noexcept;

        bool isInitialized() const;
        bool poll();

        SubscriptionId subscribe(Callback callback);
        void unsubscribe(SubscriptionId subscriptionId);

      private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace Bess::Platform::X11
