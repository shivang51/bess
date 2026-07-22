#include "drag_drop.h"

#include "common/types.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <optional>
#include <utility>

namespace Bess::UI {
    namespace {
        constexpr DragOperation kAllOperations =
            DragOperation::copy | DragOperation::move | DragOperation::link;

        [[nodiscard]] bool finite(glm::vec2 value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        [[nodiscard]] DragOperation
        validOperations(DragOperation operations) noexcept {
            return operations & kAllOperations;
        }

        [[nodiscard]] DragOperation
        selectOperation(DragOperation requested,
                        DragOperation preferred,
                        DragOperation allowed) noexcept {
            allowed = validOperations(allowed);
            if (isSingleDragOperation(requested) &&
                hasDragOperation(allowed, requested)) {
                return requested;
            }
            if (isSingleDragOperation(preferred) &&
                hasDragOperation(allowed, preferred)) {
                return preferred;
            }
            for (const auto candidate : {DragOperation::copy,
                                         DragOperation::move,
                                         DragOperation::link}) {
                if (hasDragOperation(allowed, candidate)) {
                    return candidate;
                }
            }
            return DragOperation::none;
        }

        [[nodiscard]] float normalizedThreshold(float value,
                                                float fallback) noexcept {
            value = std::isfinite(value) ? value : fallback;
            return std::max(0.f, value);
        }
    } // namespace

    DragFormatId::DragFormatId(std::string_view value) : m_value(value) {
    }

    bool DragFormatId::isValid() const noexcept {
        return !m_value.empty();
    }

    DragFormatId::operator bool() const noexcept {
        return isValid();
    }

    std::string_view DragFormatId::value() const noexcept {
        return m_value;
    }

    DragPayload::DragPayload(
        std::shared_ptr<const std::vector<Detail::DragPayloadEntry>>
            entries) noexcept
        : m_entries(std::move(entries)) {
    }

    bool DragPayload::empty() const noexcept {
        return m_entries == nullptr || m_entries->empty();
    }

    size_t DragPayload::size() const noexcept {
        return m_entries != nullptr ? m_entries->size() : 0U;
    }

    bool DragPayload::has(const DragFormatId &format) const noexcept {
        return find(format) != nullptr;
    }

    bool DragPayload::has(std::string_view format) const noexcept {
        return find(format) != nullptr;
    }

    const std::type_info *
    DragPayload::type(const DragFormatId &format) const noexcept {
        const auto *entry = find(format);
        return entry != nullptr ? entry->type : nullptr;
    }

    const Detail::DragPayloadEntry *
    DragPayload::find(const DragFormatId &format) const noexcept {
        return find(format.value());
    }

    const Detail::DragPayloadEntry *
    DragPayload::find(std::string_view format) const noexcept {
        if (m_entries == nullptr || format.empty()) {
            return nullptr;
        }
        const auto it = std::find_if(
            m_entries->begin(), m_entries->end(), [format](const auto &entry) {
                return entry.format.value() == format;
            });
        return it != m_entries->end() ? &*it : nullptr;
    }

    bool DragPayloadBuilder::erase(const DragFormatId &format) noexcept {
        const auto it = std::find_if(
            m_entries.begin(), m_entries.end(), [&format](const auto &entry) {
                return entry.format == format;
            });
        if (it == m_entries.end()) {
            return false;
        }
        m_entries.erase(it);
        return true;
    }

    void DragPayloadBuilder::clear() noexcept {
        m_entries.clear();
    }

    bool DragPayloadBuilder::empty() const noexcept {
        return m_entries.empty();
    }

    size_t DragPayloadBuilder::size() const noexcept {
        return m_entries.size();
    }

    DragPayload DragPayloadBuilder::build() const & {
        if (m_entries.empty()) {
            return {};
        }
        return DragPayload{
            std::make_shared<const std::vector<Detail::DragPayloadEntry>>(
                m_entries)};
    }

    DragPayload DragPayloadBuilder::build() && {
        if (m_entries.empty()) {
            return {};
        }
        return DragPayload{
            std::make_shared<const std::vector<Detail::DragPayloadEntry>>(
                std::move(m_entries))};
    }

    bool DragPayloadBuilder::setErased(const DragFormatId &format,
                                       const std::type_info &type,
                                       std::shared_ptr<const void> value) {
        if (!format || value == nullptr) {
            return false;
        }
        const auto it = std::find_if(
            m_entries.begin(), m_entries.end(), [&format](const auto &entry) {
                return entry.format == format;
            });
        if (it == m_entries.end()) {
            m_entries.push_back({
                .format = format,
                .type = &type,
                .value = std::move(value),
            });
            return true;
        }
        if (it->type == nullptr || *it->type != type) {
            return false;
        }
        it->value = std::move(value);
        return true;
    }

    struct DragDropService::Impl {
        struct TargetEntry {
            DropTargetCallbacks callbacks;
            bool registered = true;
        };

        struct Session {
            DragSessionId id;
            DragSourceId source;
            DragSessionPhase phase = DragSessionPhase::armed;
            glm::vec2 pressPosition{0.f, 0.f};
            glm::vec2 position{0.f, 0.f};
            DragPayload payload;
            std::function<DragPayload()> createPayload;
            DragOperation allowedOperations = DragOperation::none;
            DragOperation preferredOperation = DragOperation::none;
            DragOperation requestedOperation = DragOperation::none;
            DragOperation operation = DragOperation::none;
            float threshold = 0.f;
            CursorIcon cursor = CursorIcon::move;
            DropTargetId target;
            glm::vec2 targetLocalPosition{0.f, 0.f};
            Input::Modifiers modifiers;
            DragSourceCallbacks callbacks;
            bool external = false;
            bool started = false;
        };

        struct CallbackScope {
            explicit CallbackScope(Impl &owner) noexcept : owner(owner) {
                ++owner.callbackDepth;
            }

            ~CallbackScope() {
                if (--owner.callbackDepth == 0U) {
                    owner.flushTargetRemovals();
                }
            }

            Impl &owner;
        };

        explicit Impl(DragDropOptions value) : options(value) {
            options.dragThreshold =
                normalizedThreshold(options.dragThreshold, 5.f);
        }

        [[nodiscard]] bool targetAvailable(DropTargetId id) const noexcept {
            const auto it = targets.find(id);
            return it != targets.end() && it->second.registered;
        }

        void flushTargetRemovals() noexcept {
            for (const auto id : pendingTargetRemovals) {
                targets.erase(id);
                if (session && session->target == id) {
                    session->target = {};
                    session->operation = DragOperation::none;
                }
            }
            pendingTargetRemovals.clear();
        }

        [[nodiscard]] DragTargetEvent
        targetEvent(DropTargetId target,
                    glm::vec2 localPosition,
                    DragOperation operation) const {
            const auto &active = *session;
            return {
                .session = active.id,
                .source = active.source,
                .target = target,
                .payload = active.payload,
                .position = active.position,
                .localPosition = localPosition,
                .modifiers = active.modifiers,
                .allowedOperations = active.allowedOperations,
                .requestedOperation = active.requestedOperation,
                .operation = operation,
                .external = active.external,
            };
        }

        [[nodiscard]] DragProposal propose(DropTargetId target,
                                           const DragTargetEvent &event) {
            const auto it = targets.find(target);
            if (it == targets.end() || !it->second.registered ||
                !it->second.callbacks.propose) {
                return {};
            }
            CallbackScope callback{*this};
            return it->second.callbacks.propose(event);
        }

        void enter(DropTargetId target, const DragTargetEvent &event) {
            const auto it = targets.find(target);
            if (it == targets.end() || !it->second.registered ||
                !it->second.callbacks.onEnter) {
                return;
            }
            CallbackScope callback{*this};
            it->second.callbacks.onEnter(event);
        }

        void over(DropTargetId target, const DragTargetEvent &event) {
            const auto it = targets.find(target);
            if (it == targets.end() || !it->second.registered ||
                !it->second.callbacks.onOver) {
                return;
            }
            CallbackScope callback{*this};
            it->second.callbacks.onOver(event);
        }

        void leave(DragLeaveReason reason) {
            if (!session || !session->target) {
                return;
            }
            const DropTargetId target = session->target;
            const auto localPosition = session->targetLocalPosition;
            const auto operation = session->operation;
            session->target = {};
            session->operation = DragOperation::none;

            const auto it = targets.find(target);
            if (it == targets.end() || !it->second.registered ||
                !it->second.callbacks.onLeave) {
                return;
            }
            const auto base = targetEvent(target, localPosition, operation);
            const DragLeaveEvent event{base, reason};
            CallbackScope callback{*this};
            it->second.callbacks.onLeave(event);
        }

        [[nodiscard]] bool performDrop(DropTargetId target,
                                       const DropEvent &event) {
            const auto it = targets.find(target);
            if (it == targets.end() || !it->second.registered ||
                !it->second.callbacks.onDrop) {
                return false;
            }
            CallbackScope callback{*this};
            return it->second.callbacks.onDrop(event);
        }

        void sourceStarted(const DragStartedEvent &event) {
            if (!session || !session->callbacks.onStarted) {
                return;
            }
            CallbackScope callback{*this};
            session->callbacks.onStarted(event);
        }

        void sourceCompleted(const DragSourceCallbacks &callbacks,
                             const DragCompletedEvent &event) {
            if (!callbacks.onCompleted) {
                return;
            }
            CallbackScope callback{*this};
            callbacks.onCompleted(event);
        }

        [[nodiscard]] DragCompletedEvent
        finish(DragCompletionReason reason,
               bool accepted,
               DropTargetId targetOverride = {},
               DragOperation operationOverride = DragOperation::none) {
            if (!session) {
                return {.reason = reason};
            }

            const DropTargetId target =
                targetOverride ? targetOverride : session->target;
            const DragOperation selected =
                operationOverride != DragOperation::none ? operationOverride
                                                         : session->operation;
            const auto leaveReason = accepted ? DragLeaveReason::dropped
                                     : reason == DragCompletionReason::rejected
                                         ? DragLeaveReason::rejected
                                         : DragLeaveReason::canceled;

            std::exception_ptr failure;
            try {
                leave(leaveReason);
            } catch (...) {
                failure = std::current_exception();
            }

            auto finished = std::move(*session);
            session.reset();
            pendingCancel.reset();

            DragCompletedEvent event{
                .session = finished.id,
                .source = finished.source,
                .target = target,
                .operation = accepted ? selected : DragOperation::none,
                .reason = reason,
                .started = finished.started,
                .accepted = accepted,
                .external = finished.external,
            };
            try {
                sourceCompleted(finished.callbacks, event);
            } catch (...) {
                if (failure == nullptr) {
                    failure = std::current_exception();
                }
            }
            if (failure != nullptr) {
                std::rethrow_exception(failure);
            }
            return event;
        }

        [[nodiscard]] std::optional<DragCompletedEvent> applyDeferredCancel() {
            if (callbackDepth != 0U || !pendingCancel.has_value() || !session) {
                return std::nullopt;
            }
            const auto reason = *pendingCancel;
            pendingCancel.reset();
            return finish(reason, false);
        }

        [[nodiscard]] bool startArmed() {
            if (!session || session->phase != DragSessionPhase::armed) {
                return false;
            }
            if (session->payload.empty() && session->createPayload) {
                // Payload construction is application code. Treat it like all
                // other drag callbacks so cancellation/unregistration remains
                // deferred until we no longer hold references into Session.
                auto createPayload = session->createPayload;
                DragPayload payload;
                {
                    CallbackScope callback{*this};
                    payload = createPayload();
                }
                static_cast<void>(applyDeferredCancel());
                if (!session) {
                    return false;
                }
                session->payload = std::move(payload);
            }
            session->createPayload = {};
            if (session->payload.empty()) {
                static_cast<void>(
                    finish(DragCompletionReason::payloadUnavailable, false));
                return false;
            }
            session->phase = DragSessionPhase::dragging;
            session->started = true;
            const DragStartedEvent event{
                .session = session->id,
                .source = session->source,
                .payload = session->payload,
                .allowedOperations = session->allowedOperations,
                .external = false,
            };
            sourceStarted(event);
            static_cast<void>(applyDeferredCancel());
            return session.has_value();
        }

        void resolveTarget(std::span<const DropTargetCandidate> candidates) {
            if (!session || session->phase != DragSessionPhase::dragging) {
                return;
            }

            DropTargetId selected;
            glm::vec2 selectedLocal{0.f, 0.f};
            DragOperation selectedOperation = DragOperation::none;
            std::optional<glm::vec2> oldTargetLocal;

            for (size_t index = 0; index < candidates.size(); ++index) {
                const auto &candidate = candidates[index];
                if (!candidate.target || !targetAvailable(candidate.target)) {
                    continue;
                }
                bool duplicate = false;
                for (size_t prior = 0; prior < index; ++prior) {
                    if (candidates[prior].target == candidate.target) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    continue;
                }
                if (candidate.target == session->target) {
                    oldTargetLocal = candidate.localPosition;
                }

                const auto event = targetEvent(candidate.target,
                                               candidate.localPosition,
                                               DragOperation::none);
                const auto proposal = propose(candidate.target, event);
                static_cast<void>(applyDeferredCancel());
                if (!session) {
                    return;
                }
                if (!targetAvailable(candidate.target) ||
                    !isSingleDragOperation(proposal.operation) ||
                    !hasDragOperation(session->allowedOperations,
                                      proposal.operation)) {
                    continue;
                }
                selected = candidate.target;
                selectedLocal = candidate.localPosition;
                selectedOperation = proposal.operation;
                break;
            }

            if (oldTargetLocal.has_value()) {
                session->targetLocalPosition = *oldTargetLocal;
            }
            const bool targetChanged = session->target != selected;
            if (targetChanged && session->target) {
                leave(DragLeaveReason::targetChanged);
                static_cast<void>(applyDeferredCancel());
                if (!session) {
                    return;
                }
            }
            if (!selected) {
                return;
            }

            session->target = selected;
            session->targetLocalPosition = selectedLocal;
            session->operation = selectedOperation;
            const auto event =
                targetEvent(selected, selectedLocal, selectedOperation);
            if (targetChanged) {
                enter(selected, event);
                static_cast<void>(applyDeferredCancel());
                if (!session || session->target != selected ||
                    !targetAvailable(selected)) {
                    return;
                }
            }
            over(selected, event);
            static_cast<void>(applyDeferredCancel());
        }

        DragDropOptions options;
        std::shared_ptr<Detail::DragDropServiceControl> control;
        NodeHashMap<DropTargetId, TargetEntry> targets;
        std::vector<DropTargetId> pendingTargetRemovals;
        std::optional<Session> session;
        std::optional<DragCompletionReason> pendingCancel;
        uint32_t callbackDepth = 0;
    };

    DropTargetRegistration::DropTargetRegistration(
        std::weak_ptr<Detail::DragDropServiceControl> control,
        DropTargetId id) noexcept
        : m_control(std::move(control)),
          m_id(id) {
    }

    DragDropServiceHandle::DragDropServiceHandle(
        std::weak_ptr<Detail::DragDropServiceControl> control) noexcept
        : m_control(std::move(control)) {
    }

    DragDropService *DragDropServiceHandle::get() const noexcept {
        const auto control = m_control.lock();
        return control != nullptr ? control->service : nullptr;
    }

    DragDropServiceHandle::operator bool() const noexcept {
        return get() != nullptr;
    }

    DropTargetRegistration::DropTargetRegistration(
        DropTargetRegistration &&other) noexcept
        : m_control(std::move(other.m_control)),
          m_id(other.m_id) {
        other.m_id = {};
    }

    DropTargetRegistration &
    DropTargetRegistration::operator=(DropTargetRegistration &&other) noexcept {
        if (this == &other) {
            return *this;
        }
        static_cast<void>(reset());
        m_control = std::move(other.m_control);
        m_id = other.m_id;
        other.m_id = {};
        return *this;
    }

    DropTargetRegistration::~DropTargetRegistration() {
        static_cast<void>(reset());
    }

    DropTargetId DropTargetRegistration::id() const noexcept {
        return m_id;
    }

    bool DropTargetRegistration::isRegistered() const noexcept {
        const auto control = m_control.lock();
        return m_id && control != nullptr && control->service != nullptr &&
               control->service->containsTarget(m_id);
    }

    DropTargetRegistration::operator bool() const noexcept {
        return isRegistered();
    }

    bool DropTargetRegistration::reset() noexcept {
        if (!m_id) {
            return false;
        }
        const auto id = std::exchange(m_id, {});
        const auto control = m_control.lock();
        m_control.reset();
        return control != nullptr && control->service != nullptr &&
               control->service->unregisterTarget(id);
    }

    DragDropService::DragDropService(DragDropOptions options)
        : m_impl(std::make_unique<Impl>(options)) {
        m_impl->control = std::make_shared<Detail::DragDropServiceControl>();
        m_impl->control->service = this;
    }

    DragDropService::~DragDropService() {
        if (m_impl == nullptr) {
            return;
        }
        if (m_impl->session.has_value()) {
            try {
                static_cast<void>(m_impl->finish(
                    DragCompletionReason::sourceUnavailable, false));
            } catch (...) {
                // Destruction is nonthrowing. finish() clears the session
                // before completion callbacks, so lifecycle state is already
                // terminal even when application cleanup reports a failure.
            }
        }
        if (m_impl->control != nullptr) {
            m_impl->control->service = nullptr;
        }
        m_impl->targets.clear();
    }

    DropTargetRegistration
    DragDropService::registerTarget(DropTargetCallbacks callbacks) {
        if (!callbacks.propose || !callbacks.onDrop) {
            return {};
        }
        DropTargetId id;
        do {
            id = DropTargetId::generate();
        } while (!id || m_impl->targets.contains(id));
        m_impl->targets.emplace(
            id, Impl::TargetEntry{.callbacks = std::move(callbacks)});
        return DropTargetRegistration{m_impl->control, id};
    }

    bool DragDropService::unregisterTarget(DropTargetId target) noexcept {
        const auto it = m_impl->targets.find(target);
        if (it == m_impl->targets.end() || !it->second.registered) {
            return false;
        }

        std::function<void(const DragLeaveEvent &)> onLeave;
        std::optional<DragLeaveEvent> leaveEvent;
        if (m_impl->session && m_impl->session->target == target) {
            const auto base =
                m_impl->targetEvent(target,
                                    m_impl->session->targetLocalPosition,
                                    m_impl->session->operation);
            leaveEvent.emplace(base, DragLeaveReason::targetRemoved);
            onLeave = it->second.callbacks.onLeave;
            m_impl->session->target = {};
            m_impl->session->operation = DragOperation::none;
        }

        // Make the target unavailable before user code. Re-entrant proposal,
        // drop, or registration teardown can then never call it again.
        it->second.registered = false;
        m_impl->pendingTargetRemovals.push_back(target);
        if (onLeave && leaveEvent.has_value()) {
            try {
                DragDropService::Impl::CallbackScope callback{*m_impl};
                onLeave(*leaveEvent);
            } catch (...) {
                // Registration destruction is noexcept. Session/registration
                // state is already consistent even if a visual hook fails.
            }
        }
        if (m_impl->callbackDepth == 0U) {
            m_impl->flushTargetRemovals();
        }
        return true;
    }

    bool DragDropService::containsTarget(DropTargetId target) const noexcept {
        return m_impl->targetAvailable(target);
    }

    DragDropServiceHandle DragDropService::handle() const noexcept {
        return DragDropServiceHandle{m_impl->control};
    }

    DragSessionId DragDropService::arm(DragStartRequest request) {
        const auto allowed = validOperations(request.allowedOperations);
        if (m_impl->callbackDepth != 0U || m_impl->session || !request.source ||
            !finite(request.pressPosition) || allowed == DragOperation::none ||
            (request.payload.empty() && !request.createPayload)) {
            return {};
        }

        const auto id = DragSessionId::generate();
        m_impl->session = Impl::Session{
            .id = id,
            .source = request.source,
            .phase = DragSessionPhase::armed,
            .pressPosition = request.pressPosition,
            .position = request.pressPosition,
            .payload = std::move(request.payload),
            .createPayload = std::move(request.createPayload),
            .allowedOperations = allowed,
            .preferredOperation = selectOperation(request.preferredOperation,
                                                  request.preferredOperation,
                                                  allowed),
            .threshold = normalizedThreshold(
                request.threshold.value_or(m_impl->options.dragThreshold),
                m_impl->options.dragThreshold),
            .cursor = request.cursor,
            .callbacks = std::move(request.callbacks),
        };
        return id;
    }

    DragSessionId
    DragDropService::beginExternal(ExternalDragStartRequest request) {
        const auto allowed = validOperations(request.allowedOperations);
        if (m_impl->callbackDepth != 0U || m_impl->session ||
            !finite(request.position) || request.payload.empty() ||
            allowed == DragOperation::none) {
            return {};
        }

        const auto id = DragSessionId::generate();
        m_impl->session = Impl::Session{
            .id = id,
            .phase = DragSessionPhase::dragging,
            .pressPosition = request.position,
            .position = request.position,
            .payload = std::move(request.payload),
            .allowedOperations = allowed,
            .preferredOperation = selectOperation(request.preferredOperation,
                                                  request.preferredOperation,
                                                  allowed),
            .callbacks = std::move(request.callbacks),
            .external = true,
            .started = true,
        };
        const DragStartedEvent event{
            .session = id,
            .payload = m_impl->session->payload,
            .allowedOperations = allowed,
            .external = true,
        };
        try {
            m_impl->sourceStarted(event);
            static_cast<void>(m_impl->applyDeferredCancel());
        } catch (...) {
            const auto failure = std::current_exception();
            try {
                static_cast<void>(m_impl->finish(
                    DragCompletionReason::callbackFailed, false));
            } catch (...) {
            }
            std::rethrow_exception(failure);
        }
        return id;
    }

    DragUpdateResult
    DragDropService::updatePointer(const DragPointerUpdate &update) {
        if (m_impl->callbackDepth != 0U || !m_impl->session ||
            !finite(update.position)) {
            return {};
        }

        const auto sessionId = m_impl->session->id;
        bool startedNow = false;
        bool thresholdCrossed = false;
        try {
            auto &session = *m_impl->session;
            session.position = update.position;
            session.modifiers = update.modifiers;
            if (session.phase == DragSessionPhase::armed) {
                const auto distance = update.position - session.pressPosition;
                if (glm::dot(distance, distance) <
                    session.threshold * session.threshold) {
                    return {
                        .session = session.id,
                        .started = false,
                        .thresholdCrossed = false,
                        .dragging = false,
                    };
                }
                thresholdCrossed = true;
                startedNow = m_impl->startArmed();
                if (!m_impl->session) {
                    return {
                        .session = sessionId,
                        .started = startedNow,
                        .thresholdCrossed = true,
                    };
                }
            }

            m_impl->session->requestedOperation =
                selectOperation(update.requestedOperation,
                                m_impl->session->preferredOperation,
                                m_impl->session->allowedOperations);
            m_impl->resolveTarget(update.candidates);
            if (!m_impl->session) {
                return {};
            }
            return {
                .session = sessionId,
                .target = m_impl->session->target,
                .operation = m_impl->session->operation,
                .started = startedNow,
                .thresholdCrossed = thresholdCrossed,
                .dragging = true,
                .accepted = m_impl->session->target &&
                            m_impl->session->operation != DragOperation::none,
            };
        } catch (...) {
            const auto failure = std::current_exception();
            try {
                if (m_impl->session && m_impl->session->id == sessionId) {
                    static_cast<void>(m_impl->finish(
                        DragCompletionReason::callbackFailed, false));
                }
            } catch (...) {
            }
            std::rethrow_exception(failure);
        }
    }

    DragCompletedEvent DragDropService::drop(const DragPointerUpdate &update) {
        if (m_impl->callbackDepth != 0U || !m_impl->session) {
            return {.reason = DragCompletionReason::rejected};
        }
        const auto sessionId = m_impl->session->id;
        if (!finite(update.position)) {
            return m_impl->finish(DragCompletionReason::rejected, false);
        }
        static_cast<void>(updatePointer(update));
        if (!m_impl->session || m_impl->session->id != sessionId) {
            return {.session = sessionId,
                    .reason = DragCompletionReason::canceled};
        }
        if (m_impl->session->phase == DragSessionPhase::armed) {
            return m_impl->finish(DragCompletionReason::canceled, false);
        }

        const DropTargetId target = m_impl->session->target;
        const DragOperation operation = m_impl->session->operation;
        if (!target || !isSingleDragOperation(operation) ||
            !m_impl->targetAvailable(target)) {
            return m_impl->finish(DragCompletionReason::rejected, false);
        }

        const auto base = m_impl->targetEvent(
            target, m_impl->session->targetLocalPosition, operation);
        const DropEvent event{base};
        bool accepted = false;
        try {
            accepted = m_impl->performDrop(target, event);
            if (const auto canceled = m_impl->applyDeferredCancel()) {
                return *canceled;
            }
        } catch (...) {
            const auto failure = std::current_exception();
            try {
                if (m_impl->session && m_impl->session->id == sessionId) {
                    static_cast<void>(m_impl->finish(
                        DragCompletionReason::callbackFailed, false));
                }
            } catch (...) {
            }
            std::rethrow_exception(failure);
        }
        return m_impl->finish(accepted ? DragCompletionReason::dropped
                                       : DragCompletionReason::rejected,
                              accepted,
                              target,
                              operation);
    }

    bool DragDropService::cancel(DragCompletionReason reason) {
        if (!m_impl->session) {
            return false;
        }
        if (reason == DragCompletionReason::dropped ||
            reason == DragCompletionReason::rejected) {
            reason = DragCompletionReason::canceled;
        }
        if (m_impl->callbackDepth != 0U) {
            if (!m_impl->pendingCancel.has_value()) {
                m_impl->pendingCancel = reason;
            }
            return true;
        }
        static_cast<void>(m_impl->finish(reason, false));
        return true;
    }

    bool DragDropService::sourceUnavailable(DragSourceId source) noexcept {
        if (!source || !m_impl->session || m_impl->session->source != source) {
            return false;
        }
        try {
            return cancel(DragCompletionReason::sourceUnavailable);
        } catch (...) {
            // Destruction/unmount paths must remain noexcept; session state was
            // cleared before completion callbacks were invoked.
            return true;
        }
    }

    bool DragDropService::hasSession() const noexcept {
        return m_impl->session.has_value();
    }

    bool DragDropService::isDragging() const noexcept {
        return m_impl->session &&
               m_impl->session->phase == DragSessionPhase::dragging;
    }

    bool DragDropService::ownsSession(DragSourceId source) const noexcept {
        return source && m_impl->session && m_impl->session->source == source;
    }

    std::optional<DragSessionSnapshot>
    DragDropService::session() const noexcept {
        if (!m_impl->session) {
            return std::nullopt;
        }
        const auto &active = *m_impl->session;
        return DragSessionSnapshot{
            .id = active.id,
            .source = active.source,
            .target = active.target,
            .phase = active.phase,
            .pressPosition = active.pressPosition,
            .position = active.position,
            .allowedOperations = active.allowedOperations,
            .operation = active.operation,
            .cursor = active.cursor,
            .external = active.external,
        };
    }

    const DragPayload *DragDropService::payload() const noexcept {
        return m_impl->session && !m_impl->session->payload.empty()
                   ? &m_impl->session->payload
                   : nullptr;
    }

    DropTargetId DragDropService::activeTarget() const noexcept {
        return m_impl->session ? m_impl->session->target : DropTargetId{};
    }

    DragOperation DragDropService::activeOperation() const noexcept {
        return m_impl->session ? m_impl->session->operation
                               : DragOperation::none;
    }

    const DragDropOptions &DragDropService::options() const noexcept {
        return m_impl->options;
    }

    void DragDropService::setOptions(DragDropOptions options) noexcept {
        options.dragThreshold = normalizedThreshold(options.dragThreshold, 5.f);
        m_impl->options = options;
    }

} // namespace Bess::UI
