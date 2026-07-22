#pragma once

#include "bess_core/input/input_event.h"
#include "common/bess_api.h"
#include "ui_types.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace Bess::UI {

    // A process-stable name for one representation of drag data. MIME names
    // are recommended for interoperable data; application-specific names
    // should be namespaced (for example, "application/x-bess-asset-ids").
    class BESS_API DragFormatId {
      public:
        DragFormatId() = default;
        explicit DragFormatId(std::string_view value);

        [[nodiscard]] bool isValid() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
        [[nodiscard]] std::string_view value() const noexcept;

        bool operator==(const DragFormatId &) const noexcept = default;

      private:
        std::string m_value;
    };

    // Couples a representation name to its C++ value type. The runtime type
    // check prevents two plugins from accidentally interpreting a shared name
    // as different in-process types.
    template <typename T> class DragFormat {
      public:
        using Value = std::remove_cv_t<T>;

        static_assert(!std::is_reference_v<T>);
        static_assert(!std::is_void_v<T>);

        explicit DragFormat(std::string_view value) : m_id(value) {
        }

        [[nodiscard]] const DragFormatId &id() const noexcept {
            return m_id;
        }

      private:
        DragFormatId m_id;
    };

    namespace Detail {
        struct DragPayloadEntry {
            DragFormatId format;
            const std::type_info *type = nullptr;
            std::shared_ptr<const void> value;
        };
    } // namespace Detail

    class DragPayloadBuilder;

    // Immutable, cheaply copied collection of typed representations. A single
    // payload may expose the same logical object as an internal ID list, text,
    // and a URI list without forcing a target to understand every format.
    class BESS_API DragPayload {
      public:
        DragPayload() = default;

        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] bool has(const DragFormatId &format) const noexcept;
        [[nodiscard]] bool has(std::string_view format) const noexcept;
        [[nodiscard]] const std::type_info *
        type(const DragFormatId &format) const noexcept;

        template <typename T>
        [[nodiscard]] const typename DragFormat<T>::Value *
        get(const DragFormat<T> &format) const noexcept {
            using Value = typename DragFormat<T>::Value;
            const auto *entry = find(format.id());
            if (entry == nullptr || entry->type == nullptr ||
                *entry->type != typeid(Value)) {
                return nullptr;
            }
            return static_cast<const Value *>(entry->value.get());
        }

      private:
        friend class DragPayloadBuilder;
        explicit DragPayload(
            std::shared_ptr<const std::vector<Detail::DragPayloadEntry>>
                entries) noexcept;

        [[nodiscard]] const Detail::DragPayloadEntry *
        find(const DragFormatId &format) const noexcept;
        [[nodiscard]] const Detail::DragPayloadEntry *
        find(std::string_view format) const noexcept;

        std::shared_ptr<const std::vector<Detail::DragPayloadEntry>> m_entries;
    };

    // Mutable construction boundary for DragPayload. Reusing a format replaces
    // its value only when the registered C++ type agrees; a conflicting type
    // is rejected rather than becoming an unsafe cast at the drop target.
    class BESS_API DragPayloadBuilder {
      public:
        DragPayloadBuilder() = default;

        template <typename T, typename U = typename DragFormat<T>::Value>
            requires std::constructible_from<typename DragFormat<T>::Value, U>
        bool set(const DragFormat<T> &format, U &&value) {
            using Value = typename DragFormat<T>::Value;
            return setErased(
                format.id(),
                typeid(Value),
                std::make_shared<const Value>(std::forward<U>(value)));
        }

        template <typename T>
        bool
        setShared(const DragFormat<T> &format,
                  std::shared_ptr<const typename DragFormat<T>::Value> value) {
            using Value = typename DragFormat<T>::Value;
            if (value == nullptr) {
                return false;
            }
            return setErased(format.id(), typeid(Value), std::move(value));
        }

        bool erase(const DragFormatId &format) noexcept;
        void clear() noexcept;
        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] size_t size() const noexcept;

        [[nodiscard]] DragPayload build() const &;
        [[nodiscard]] DragPayload build() &&;

      private:
        bool setErased(const DragFormatId &format,
                       const std::type_info &type,
                       std::shared_ptr<const void> value);

        std::vector<Detail::DragPayloadEntry> m_entries;
    };

    struct DragFileList {
        std::vector<std::string> paths;
    };

    namespace DragFormats {
        inline const DragFormat<std::string> plainText{"text/plain"};
        inline const DragFormat<std::string> uriList{"text/uri-list"};
        inline const DragFormat<DragFileList> files{
            "application/x-bess-file-list"};
    } // namespace DragFormats

    enum class DragOperation : uint8_t {
        none = 0,
        copy = 1U << 0U,
        move = 1U << 1U,
        link = 1U << 2U,
    };

    [[nodiscard]] constexpr DragOperation
    operator|(DragOperation lhs, DragOperation rhs) noexcept {
        return static_cast<DragOperation>(static_cast<uint8_t>(lhs) |
                                          static_cast<uint8_t>(rhs));
    }

    [[nodiscard]] constexpr DragOperation
    operator&(DragOperation lhs, DragOperation rhs) noexcept {
        return static_cast<DragOperation>(static_cast<uint8_t>(lhs) &
                                          static_cast<uint8_t>(rhs));
    }

    constexpr DragOperation &operator|=(DragOperation &lhs,
                                        DragOperation rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    [[nodiscard]] constexpr bool hasDragOperation(DragOperation operations,
                                                  DragOperation operation) {
        return operation != DragOperation::none &&
               (operations & operation) == operation;
    }

    [[nodiscard]] constexpr bool
    isSingleDragOperation(DragOperation operation) noexcept {
        const auto value = static_cast<uint8_t>(operation);
        return value != 0U && (value & static_cast<uint8_t>(value - 1U)) == 0U;
    }

    enum class ExternalDragEventPhase : uint8_t {
        enter,
        move,
        leave,
        drop,
    };

    // Platform adapters translate native drag protocols to this target-local
    // event. Position uses the same top-left surface coordinates as mouse
    // input; WidgetTree performs the centered-space conversion once. Dispatch
    // synchronously when protocol acceptance depends on the returned result.
    struct ExternalDragEvent {
        ExternalDragEventPhase phase = ExternalDragEventPhase::enter;
        DragPayload payload;
        glm::vec2 pos{0.f, 0.f};
        DragOperation allowedOperations = DragOperation::copy;
        DragOperation preferredOperation = DragOperation::copy;
    };

    struct DragSourceIdTag;
    struct DropTargetIdTag;
    struct DragSessionIdTag;

    using DragSourceId = StableId<DragSourceIdTag>;
    using DropTargetId = StableId<DropTargetIdTag>;
    using DragSessionId = StableId<DragSessionIdTag>;

    enum class DragSessionPhase : uint8_t { armed, dragging };

    enum class DragLeaveReason : uint8_t {
        targetChanged,
        canceled,
        dropped,
        rejected,
        targetRemoved,
    };

    enum class DragCompletionReason : uint8_t {
        dropped,
        rejected,
        canceled,
        escape,
        pointerCaptureLost,
        sourceUnavailable,
        payloadUnavailable,
        callbackFailed,
    };

    struct DragProposal {
        // Exactly one operation accepts the offer. `none` rejects it.
        DragOperation operation = DragOperation::none;

        [[nodiscard]] bool accepted() const noexcept {
            return isSingleDragOperation(operation);
        }
    };

    struct DragTargetEvent {
        DragSessionId session;
        DragSourceId source;
        DropTargetId target;
        // DragPayload is an immutable shared snapshot, so copying an event is
        // cheap and remains safe after the source session has completed.
        DragPayload payload;
        glm::vec2 position{0.f, 0.f};
        glm::vec2 localPosition{0.f, 0.f};
        Input::Modifiers modifiers;
        DragOperation allowedOperations = DragOperation::none;
        DragOperation requestedOperation = DragOperation::none;
        DragOperation operation = DragOperation::none;
        bool external = false;
    };

    struct DragLeaveEvent : DragTargetEvent {
        DragLeaveReason reason = DragLeaveReason::targetChanged;
    };

    struct DropEvent : DragTargetEvent {};

    struct DropTargetCallbacks {
        // Called on every pointer update for candidates, deepest first. It
        // should be inexpensive and free of visible side effects; lifecycle
        // visuals belong in onEnter/onOver/onLeave.
        std::function<DragProposal(const DragTargetEvent &)> propose;
        std::function<void(const DragTargetEvent &)> onEnter;
        std::function<void(const DragTargetEvent &)> onOver;
        std::function<void(const DragLeaveEvent &)> onLeave;
        // Return true only after the target committed the model mutation.
        std::function<bool(const DropEvent &)> onDrop;
    };

    struct DragStartedEvent {
        DragSessionId session;
        DragSourceId source;
        DragPayload payload;
        DragOperation allowedOperations = DragOperation::none;
        bool external = false;
    };

    struct DragCompletedEvent {
        DragSessionId session;
        DragSourceId source;
        DropTargetId target;
        DragOperation operation = DragOperation::none;
        DragCompletionReason reason = DragCompletionReason::canceled;
        bool started = false;
        bool accepted = false;
        bool external = false;
    };

    struct DragSourceCallbacks {
        std::function<void(const DragStartedEvent &)> onStarted;
        std::function<void(const DragCompletedEvent &)> onCompleted;
    };

    struct DragDropOptions {
        float dragThreshold = 5.f;
    };

    struct DragStartRequest {
        DragSourceId source;
        glm::vec2 pressPosition{0.f, 0.f};
        DragPayload payload;
        // Evaluated only after the threshold is crossed. It is used when
        // building or serializing the payload is non-trivial.
        std::function<DragPayload()> createPayload;
        DragOperation allowedOperations = DragOperation::move;
        DragOperation preferredOperation = DragOperation::move;
        std::optional<float> threshold;
        CursorIcon cursor = CursorIcon::move;
        DragSourceCallbacks callbacks;
    };

    struct ExternalDragStartRequest {
        glm::vec2 position{0.f, 0.f};
        DragPayload payload;
        DragOperation allowedOperations = DragOperation::copy;
        DragOperation preferredOperation = DragOperation::copy;
        DragSourceCallbacks callbacks;
    };

    struct DropTargetCandidate {
        DropTargetId target;
        glm::vec2 localPosition{0.f, 0.f};
    };

    struct DragPointerUpdate {
        glm::vec2 position{0.f, 0.f};
        Input::Modifiers modifiers;
        // `none` lets the service use the source's preferred operation.
        DragOperation requestedOperation = DragOperation::none;
        // Deepest/frontmost target first, followed by accepting ancestors.
        std::span<const DropTargetCandidate> candidates;
    };

    struct DragUpdateResult {
        DragSessionId session;
        DropTargetId target;
        DragOperation operation = DragOperation::none;
        bool started = false;
        // True once this pointer gesture crosses the threshold, even if
        // payload creation or a callback synchronously cancels the session.
        bool thresholdCrossed = false;
        bool dragging = false;
        bool accepted = false;
    };

    struct DragSessionSnapshot {
        DragSessionId id;
        DragSourceId source;
        DropTargetId target;
        DragSessionPhase phase = DragSessionPhase::armed;
        glm::vec2 pressPosition{0.f, 0.f};
        glm::vec2 position{0.f, 0.f};
        DragOperation allowedOperations = DragOperation::none;
        DragOperation operation = DragOperation::none;
        CursorIcon cursor = CursorIcon::move;
        bool external = false;
    };

    class DragDropService;

    namespace Detail {
        struct DragDropServiceControl {
            DragDropService *service = nullptr;
        };
    } // namespace Detail

    // Non-owning, lifetime-aware access for retained widgets. It becomes
    // empty before DragDropService storage is released, so a service may be
    // application-owned without imposing shared ownership on every control.
    class BESS_API DragDropServiceHandle {
      public:
        DragDropServiceHandle() = default;

        [[nodiscard]] DragDropService *get() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;

      private:
        friend class DragDropService;
        explicit DragDropServiceHandle(
            std::weak_ptr<Detail::DragDropServiceControl> control) noexcept;

        std::weak_ptr<Detail::DragDropServiceControl> m_control;
    };

    // Move-only RAII registration. Destroying a widget can therefore never
    // leave a callable target behind in an application-owned drag service.
    class BESS_API DropTargetRegistration {
      public:
        DropTargetRegistration() = default;
        DropTargetRegistration(const DropTargetRegistration &) = delete;
        DropTargetRegistration &
        operator=(const DropTargetRegistration &) = delete;
        DropTargetRegistration(DropTargetRegistration &&other) noexcept;
        DropTargetRegistration &
        operator=(DropTargetRegistration &&other) noexcept;
        ~DropTargetRegistration();

        [[nodiscard]] DropTargetId id() const noexcept;
        [[nodiscard]] bool isRegistered() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
        bool reset() noexcept;

      private:
        friend class DragDropService;
        DropTargetRegistration(
            std::weak_ptr<Detail::DragDropServiceControl> control,
            DropTargetId id) noexcept;

        std::weak_ptr<Detail::DragDropServiceControl> m_control;
        DropTargetId m_id;
    };

    // Renderer- and widget-independent drag session authority. The embedding
    // UI supplies hit-tested candidates on pointer updates. One service may be
    // shared by several WidgetTrees, but it does not select a host window,
    // forward events, or transform coordinates; the host must route each
    // update once and provide candidates in the receiving tree's coordinate
    // space. All methods are UI-thread-only.
    class BESS_API DragDropService {
      public:
        explicit DragDropService(DragDropOptions options = {});
        DragDropService(const DragDropService &) = delete;
        DragDropService(DragDropService &&) = delete;
        ~DragDropService();
        DragDropService &operator=(const DragDropService &) = delete;
        DragDropService &operator=(DragDropService &&) = delete;

        [[nodiscard]] DropTargetRegistration
        registerTarget(DropTargetCallbacks callbacks);
        bool unregisterTarget(DropTargetId target) noexcept;
        [[nodiscard]] bool containsTarget(DropTargetId target) const noexcept;
        [[nodiscard]] DragDropServiceHandle handle() const noexcept;

        [[nodiscard]] DragSessionId arm(DragStartRequest request);
        [[nodiscard]] DragSessionId
        beginExternal(ExternalDragStartRequest request);
        [[nodiscard]] DragUpdateResult
        updatePointer(const DragPointerUpdate &update);
        [[nodiscard]] DragCompletedEvent drop(const DragPointerUpdate &update);
        bool
        cancel(DragCompletionReason reason = DragCompletionReason::canceled);
        bool sourceUnavailable(DragSourceId source) noexcept;

        [[nodiscard]] bool hasSession() const noexcept;
        [[nodiscard]] bool isDragging() const noexcept;
        [[nodiscard]] bool ownsSession(DragSourceId source) const noexcept;
        [[nodiscard]] std::optional<DragSessionSnapshot>
        session() const noexcept;
        [[nodiscard]] const DragPayload *payload() const noexcept;
        [[nodiscard]] DropTargetId activeTarget() const noexcept;
        [[nodiscard]] DragOperation activeOperation() const noexcept;

        [[nodiscard]] const DragDropOptions &options() const noexcept;
        void setOptions(DragDropOptions options) noexcept;

      private:
        friend class DropTargetRegistration;
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

    // Capability queried while walking the hit widget's ancestry. It avoids a
    // dependency from DragDropService to WidgetTree or concrete controls.
    class BESS_API DropTargetProvider {
      public:
        virtual ~DropTargetProvider() = default;
        [[nodiscard]] virtual DropTargetId dropTargetId() const noexcept = 0;
    };

    // Capability used to identify the retained wrapper that owns an armed
    // drag. WidgetTree may prioritize it for pointer capture once the service
    // crosses the drag threshold without coupling to a concrete Draggable.
    class BESS_API DragSourceProvider {
      public:
        virtual ~DragSourceProvider() = default;
        [[nodiscard]] virtual DragSourceId dragSourceId() const noexcept = 0;
        [[nodiscard]] virtual bool canStartDrag() const noexcept {
            return true;
        }
    };

} // namespace Bess::UI
