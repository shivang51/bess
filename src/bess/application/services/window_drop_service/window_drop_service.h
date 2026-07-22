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
        using DragCallback =
            std::function<void(const Events::WindowDragDropEvent &)>;
        using DragDecisionCallback =
            std::function<bool(const Events::WindowDragDropEvent &)>;

        void onInit() override;
        void onPostInit() override;
        void onBeginFrame() override;
        void onPreUpdate() override;
        void onShutdown() override;
        void onDestroy() override;

        // Subscription mutation and delivery occur on the application thread.
        // Callbacks may safely unsubscribe themselves or another callback;
        // additions made during delivery begin with the next native event.
        SubscriptionId subscribe(Callback callback);
        void unsubscribe(SubscriptionId subscriptionId);
        SubscriptionId subscribeDrag(DragCallback callback);
        void unsubscribeDrag(SubscriptionId subscriptionId);
        // Interactive targets answer synchronously so native protocols can
        // advertise and finish only drops the UI actually accepts. They run in
        // subscription order and stop after the first acceptance, preventing
        // more than one target from committing the same native drop.
        SubscriptionId subscribeDragDecision(DragDecisionCallback callback);
        void unsubscribeDragDecision(SubscriptionId subscriptionId);

      private:
        [[nodiscard]] SubscriptionId allocateSubscriptionId() noexcept;
        void pollNativeHandlers();
        void emit(const Events::WindowDropEvent &event) const;
        void emitDrag(const Events::WindowDragDropEvent &event) const;
        [[nodiscard]] bool
        emitDragDecision(const Events::WindowDragDropEvent &event) const;

#if defined(__linux__)
        [[nodiscard]] bool
        handleNativeEvent(const Platform::X11::WindowDropEvent &event);
#endif

      private:
        SubscriptionId m_nextSubscriptionId = 1;
        std::unordered_map<SubscriptionId, Callback> m_callbacks;
        std::unordered_map<SubscriptionId, DragCallback> m_dragCallbacks;
        std::unordered_map<SubscriptionId, DragDecisionCallback>
            m_dragDecisionCallbacks;
        std::weak_ptr<Window> m_window;
        std::weak_ptr<EventSystem::EventDispatcher> m_eventDispatcher;

#if defined(__linux__)
        std::unique_ptr<Platform::X11::WindowDropHandler>
            m_x11WindowDropHandler;
#endif
    };

} // namespace Bess::Svc
