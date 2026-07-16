#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct _XDisplay;

namespace Bess::Platform::X11 {

    using X11WindowHandle = unsigned long;

    enum class FileDropEventType {
        enter,
        position,
        leave,
        drop,
        selectionData,
        finished,
    };

    struct FileDropEvent {
        FileDropEventType type = FileDropEventType::enter;
        std::vector<std::string> files;
        std::string rawData;
        std::string requestedType;
        std::string actualType;
        int x = 0;
        int y = 0;
        int actualFormat = 0;
        bool accepted = false;
    };

    const char *toString(FileDropEventType type);

    class FileDropHandler {
      public:
        using SubscriptionId = std::size_t;
        using Callback = std::function<void(const FileDropEvent &)>;

        FileDropHandler(_XDisplay *display, X11WindowHandle targetWindow);
        ~FileDropHandler();

        FileDropHandler(const FileDropHandler &) = delete;
        FileDropHandler &operator=(const FileDropHandler &) = delete;
        FileDropHandler(FileDropHandler &&) noexcept;
        FileDropHandler &operator=(FileDropHandler &&) noexcept;

        bool isInitialized() const;
        bool poll();

        SubscriptionId subscribe(Callback callback);
        void unsubscribe(SubscriptionId subscriptionId);

      private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace Bess::Platform::X11
