#pragma once

#include "common/sub_system.h"

#if defined(__linux__)
    #include "platform/x11/x11_file_drop_handler.h"
#endif

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::Svc {

    enum class FileDragDropEventType {
        enter,
        position,
        leave,
        drop,
        selectionData,
        finished,
    };

    struct FileDragDropEvent {
        FileDragDropEventType type = FileDragDropEventType::enter;
        std::vector<std::string> files;
        std::string rawData;
        std::string requestedType;
        std::string actualType;
        int x = 0;
        int y = 0;
        int actualFormat = 0;
        bool accepted = false;
    };

    const char *toString(FileDragDropEventType type);

    class FileDragDropService : public ISubSystem {
      public:
        using SubscriptionId = std::size_t;
        using Callback = std::function<void(const FileDragDropEvent &)>;

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
        void emit(const FileDragDropEvent &event) const;

#if defined(__linux__)
        static FileDragDropEvent
        fromNativeX11Event(const Platform::X11::FileDropEvent &event);
#endif

      private:
        SubscriptionId m_nextSubscriptionId = 1;
        SubscriptionId m_logSubscriptionId = 0;
        std::unordered_map<SubscriptionId, Callback> m_callbacks;

#if defined(__linux__)
        std::unique_ptr<Platform::X11::FileDropHandler> m_x11FileDropHandler;
        Platform::X11::FileDropHandler::SubscriptionId m_x11SubscriptionId = 0;
#endif
    };

} // namespace Bess::Svc
