#include "services/file_drag_drop_service/file_drag_drop_service.h"

#include "bess_core/g_app_context.h"
#include "common/logger.h"
#include "window.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace Bess::Svc {
    namespace {
        constexpr std::size_t kMaxRawDataLogBytes = 2048;
        constexpr std::size_t kRawDataPreviewBytes = 64;

        constexpr char kHexDigits[] = "0123456789abcdef";

        std::string formatRawDataHexPreview(std::string_view rawData) {
            const auto byteCount = rawData.size() > kRawDataPreviewBytes
                                       ? kRawDataPreviewBytes
                                       : rawData.size();

            std::string preview;
            preview.reserve(byteCount * 3);

            for (std::size_t i = 0; i < byteCount; ++i) {
                if (i != 0) {
                    preview.push_back(' ');
                }

                const auto value = static_cast<unsigned char>(rawData[i]);
                preview.push_back(kHexDigits[(value >> 4) & 0xf]);
                preview.push_back(kHexDigits[value & 0xf]);
            }

            if (rawData.size() > kRawDataPreviewBytes) {
                preview += " ...";
            }

            return preview;
        }

        std::string formatRawDataAsciiPreview(std::string_view rawData) {
            const auto byteCount = rawData.size() > kRawDataPreviewBytes
                                       ? kRawDataPreviewBytes
                                       : rawData.size();

            std::string preview;
            preview.reserve(byteCount);

            for (std::size_t i = 0; i < byteCount; ++i) {
                const auto value = static_cast<unsigned char>(rawData[i]);
                preview.push_back(std::isprint(value) ? static_cast<char>(value)
                                                      : '.');
            }

            if (rawData.size() > kRawDataPreviewBytes) {
                preview += "...";
            }

            return preview;
        }

        std::string escapeRawDataForLog(std::string_view rawData) {
            static constexpr char kHexDigits[] = "0123456789abcdef";

            const auto byteCount = rawData.size() > kMaxRawDataLogBytes
                                       ? kMaxRawDataLogBytes
                                       : rawData.size();

            std::string escaped;
            escaped.reserve(byteCount);

            for (std::size_t i = 0; i < byteCount; ++i) {
                const auto value = static_cast<unsigned char>(rawData[i]);
                if (value == '\n' || value == '\r' || value == '\t' ||
                    std::isprint(value)) {
                    escaped.push_back(static_cast<char>(value));
                    continue;
                }

                escaped.push_back('\\');
                escaped.push_back('x');
                escaped.push_back(kHexDigits[(value >> 4) & 0xf]);
                escaped.push_back(kHexDigits[value & 0xf]);
            }

            if (rawData.size() > kMaxRawDataLogBytes) {
                escaped += "\n... truncated ...";
            }

            return escaped;
        }

        void logFileDragDropEvent(const FileDragDropEvent &event) {
            if (event.type == FileDragDropEventType::position) {
                BESS_DEBUG("[FileDragDrop] event={} accepted={} pos=({}, {})",
                           toString(event.type),
                           event.accepted,
                           event.x,
                           event.y);
            } else {
                BESS_INFO("[FileDragDrop] event={} accepted={} pos=({}, {})",
                          toString(event.type),
                          event.accepted,
                          event.x,
                          event.y);
            }

            if (!event.requestedType.empty() || !event.actualType.empty()) {
                BESS_INFO("[FileDragDrop] selection target={} actual-type={} "
                          "format={} raw-bytes={}",
                          event.requestedType.empty() ? "unknown"
                                                      : event.requestedType,
                          event.actualType.empty() ? "unknown"
                                                   : event.actualType,
                          event.actualFormat,
                          event.rawData.size());
            }

            if (!event.rawData.empty()) {
                BESS_INFO("[FileDragDrop] raw first {} bytes hex: {}",
                          std::min(event.rawData.size(), kRawDataPreviewBytes),
                          formatRawDataHexPreview(event.rawData));
                BESS_INFO("[FileDragDrop] raw first {} bytes ascii: {}",
                          std::min(event.rawData.size(), kRawDataPreviewBytes),
                          formatRawDataAsciiPreview(event.rawData));

                BESS_DEBUG("[FileDragDrop] raw data ({} bytes, target={}, "
                           "actual-type={}, format={}):\n{}",
                           event.rawData.size(),
                           event.requestedType.empty() ? "unknown"
                                                       : event.requestedType,
                           event.actualType.empty() ? "unknown"
                                                    : event.actualType,
                           event.actualFormat,
                           escapeRawDataForLog(event.rawData));
            }

            for (const auto &file : event.files) {
                BESS_INFO("[FileDragDrop] dropped file: {}", file);
            }
        }
    } // namespace

    const char *toString(FileDragDropEventType type) {
        switch (type) {
        case FileDragDropEventType::enter:
            return "enter";
        case FileDragDropEventType::position:
            return "position";
        case FileDragDropEventType::leave:
            return "leave";
        case FileDragDropEventType::drop:
            return "drop";
        case FileDragDropEventType::selectionData:
            return "selection-data";
        case FileDragDropEventType::finished:
            return "finished";
        }

        return "unknown";
    }

    void FileDragDropService::onInit() {
    }

    void FileDragDropService::onPostInit() {
        m_logSubscriptionId = subscribe(logFileDragDropEvent);

        auto &appCtx = GAppContext::getInstance();
        const auto window = appCtx.getSubSystem<Window>();
        if (!window) {
            BESS_WARN("[FileDragDrop] Window is unavailable; file drops will "
                      "not be monitored");
            return;
        }

#if defined(__linux__)
        if (!window->isNativeX11()) {
            return;
        }

        auto *display = static_cast<_XDisplay *>(window->getNativeX11Display());
        const auto targetWindow = static_cast<Platform::X11::X11WindowHandle>(
            window->getNativeX11Window());
        if (!display || targetWindow == 0) {
            BESS_WARN("[FileDragDrop] X11 window handles are unavailable; file "
                      "drops will not be monitored");
            return;
        }

        m_x11FileDropHandler = std::make_unique<Platform::X11::FileDropHandler>(
            display, targetWindow);
        if (!m_x11FileDropHandler->isInitialized()) {
            BESS_WARN("[FileDragDrop] Native X11 file drag/drop handler could "
                      "not be initialized");
            m_x11FileDropHandler.reset();
            return;
        }

        m_x11SubscriptionId = m_x11FileDropHandler->subscribe(
            [this](const Platform::X11::FileDropEvent &event) {
                emit(fromNativeX11Event(event));
            });

        BESS_INFO("[FileDragDrop] Native X11 file drag/drop handler enabled");
#endif
    }

    void FileDragDropService::onBeginFrame() {
        pollNativeHandlers();
    }

    void FileDragDropService::onPreUpdate() {
        pollNativeHandlers();
    }

    void FileDragDropService::onShutdown() {
#if defined(__linux__)
        if (m_x11FileDropHandler && m_x11SubscriptionId != 0) {
            m_x11FileDropHandler->unsubscribe(m_x11SubscriptionId);
            m_x11SubscriptionId = 0;
        }
        m_x11FileDropHandler.reset();
#endif

        if (m_logSubscriptionId != 0) {
            unsubscribe(m_logSubscriptionId);
            m_logSubscriptionId = 0;
        }
    }

    void FileDragDropService::onDestroy() {
        m_callbacks.clear();
    }

    FileDragDropService::SubscriptionId
    FileDragDropService::subscribe(Callback callback) {
        if (!callback) {
            return 0;
        }

        const auto subscriptionId = m_nextSubscriptionId++;
        m_callbacks.emplace(subscriptionId, std::move(callback));
        return subscriptionId;
    }

    void FileDragDropService::unsubscribe(SubscriptionId subscriptionId) {
        m_callbacks.erase(subscriptionId);
    }

    void FileDragDropService::pollNativeHandlers() {
#if defined(__linux__)
        if (m_x11FileDropHandler) {
            m_x11FileDropHandler->poll();
        }
#endif
    }

    void FileDragDropService::emit(const FileDragDropEvent &event) const {
        for (const auto &[_, callback] : m_callbacks) {
            callback(event);
        }
    }

#if defined(__linux__)
    FileDragDropEvent FileDragDropService::fromNativeX11Event(
        const Platform::X11::FileDropEvent &event) {
        FileDragDropEventType type = FileDragDropEventType::enter;
        switch (event.type) {
        case Platform::X11::FileDropEventType::enter:
            type = FileDragDropEventType::enter;
            break;
        case Platform::X11::FileDropEventType::position:
            type = FileDragDropEventType::position;
            break;
        case Platform::X11::FileDropEventType::leave:
            type = FileDragDropEventType::leave;
            break;
        case Platform::X11::FileDropEventType::drop:
            type = FileDragDropEventType::drop;
            break;
        case Platform::X11::FileDropEventType::selectionData:
            type = FileDragDropEventType::selectionData;
            break;
        case Platform::X11::FileDropEventType::finished:
            type = FileDragDropEventType::finished;
            break;
        }

        return {
            .type = type,
            .files = event.files,
            .rawData = event.rawData,
            .requestedType = event.requestedType,
            .actualType = event.actualType,
            .x = event.x,
            .y = event.y,
            .actualFormat = event.actualFormat,
            .accepted = event.accepted,
        };
    }
#endif

} // namespace Bess::Svc
