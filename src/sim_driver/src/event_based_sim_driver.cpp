#include "sim_driver/event_based_sim_driver.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "sim_driver/sim_driver.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <unordered_map>

// #define BESS_ENABLE_LOG_EVENTS

#ifdef BESS_ENABLE_LOG_EVENTS
    #define BESS_LOG_EVENT(...) BESS_TRACE(__VA_ARGS__);
#else
    #define BESS_LOG_EVENT(...)
#endif // !BESS_LOG_EVENT

namespace Bess::SimEngine::Drivers {

    Json::Value EvtBasedCompDef::toJson() const {
        Json::Value json = CompDef::toJson();
        json["shouldAutoReschedule"] = m_autoReschedule;
        json["autoRescheduleDelay"] = m_autoRescheduleDelay.count();
        json["propDelay"] = m_propDelay.count();
        return json;
    }

    TimeNs EvtBasedCompDef::getSelfSimDelay() {
        return m_autoRescheduleDelay;
    }

    TimeNs EvtBasedSimComp::getPropDelay() const {
        auto def = std::dynamic_pointer_cast<EvtBasedCompDef>(m_def);
        return def ? def->getPropDelay() : TimeNs(0);
    }

    bool EvtBasedSimComp::getSimSelf() const {
        auto def = std::dynamic_pointer_cast<EvtBasedCompDef>(m_def);
        return def ? def->getAutoReschedule() : false;
    }

    TimeNs EvtBasedSimComp::getSelfSimDelay() const {
        auto def = std::dynamic_pointer_cast<EvtBasedCompDef>(m_def);
        return def ? def->getSelfSimDelay() : TimeNs(0);
    }

    Json::Value EvtBasedSimComp::toJson() const {
        Json::Value json = SimComponent::toJson();
        return json;
    }

    void EvtBasedSimComp::loadJson(const Json::Value &json) {
        SimComponent::loadJson(json);
    }

    void EvtBasedSimDriver::run() {
        BESS_ASSERT(isInitialized(),
                    "SimDriver must be initialized before running");
        BESS_ASSERT(!isDestroyed(), "SimDriver was destroyed, cannot run");

        // A driver has exactly one event consumer. Starting run() while the
        // engine already coordinates this driver would otherwise create a
        // second consumer racing the global scheduler.
        if (!beginRun(TimeNs(0))) {
            return;
        }

        while (!isStopped()) {
            std::optional<TimeNs> nextTime;
            {
                std::unique_lock lk(m_eventMutex);
                m_runIterCv.wait(lk, [&] {
                    return isStopped() || (!isPaused() && !m_events.empty());
                });
                if (isStopped()) {
                    break;
                }
                if (!isPaused() && !m_events.empty()) {
                    nextTime = m_events.begin()->simTime;
                }
            }

            if (!nextTime) {
                continue;
            }

            (void)getSimulationClock()->advanceTo(*nextTime);
            try {
                processEventsAt(getCurrentSimTime());
            } catch (const std::exception &error) {
                BESS_ERROR("Driver event processing failed: {}", error.what());
                stop();
            } catch (...) {
                BESS_ERROR("Driver event processing failed with an unknown "
                           "exception");
                stop();
            }
        }
    }

    void EvtBasedSimDriver::onRunStart(TimeNs startTime) {
        {
            std::lock_guard lk(m_eventMutex);
            prepareRunStartLocked(startTime);
        }
        onBeforeRun();
        m_runIterCv.notify_all();
        notifyScheduler();
    }

    UUID
    EvtBasedSimDriver::addComponent(const std::shared_ptr<SimComponent> &comp,
                                    bool scheduleSim) {
        auto compCasted = std::dynamic_pointer_cast<EvtBasedSimComp>(comp);

        BESS_ASSERT(compCasted,
                    "EvtBasedSimDriver only supports components of "
                    "type EvtBasedSimComp");

        {
            std::lock_guard lk(m_eventMutex);
            SimDriver::addComponent(comp, scheduleSim);

            if (scheduleSim) {
                m_runStartScheduledCompIds.insert(comp->getUuid());
            } else {
                m_runStartScheduledCompIds.erase(comp->getUuid());
            }

            bool driverActive = false;
            {
                std::lock_guard stateLock(m_stateMutex);
                driverActive = m_state == SimDriverState::running ||
                               m_state == SimDriverState::paused;
            }

            const bool shouldScheduleNow = scheduleSim && driverActive;
            if (shouldScheduleNow) {
                BESS_DEBUG(
                    "Scheduling initial event for component {} at time {}ns",
                    (uint64_t)comp->getUuid(),
                    getCurrentSimTime().count());
                scheduleEvtLocked(
                    comp->getUuid(), getCurrentSimTime(), UUID::null);
            }
        }

        m_runIterCv.notify_all();
        notifyScheduler();

        return comp->getUuid();
    }

    void EvtBasedSimDriver::deleteComponent(const UUID &uuid) {
        {
            std::lock_guard lk(m_eventMutex);
            m_runStartScheduledCompIds.erase(uuid);
            std::erase(m_deferredRunStartCompIds, uuid);

            std::erase_if(m_events, [&](const SimEvt &evt) {
                return evt.compId == uuid || evt.schedulerId == uuid;
            });
        }

        SimDriver::deleteComponent(uuid);
        notifyScheduler();
    }

    void EvtBasedSimDriver::clearComponents() {
        {
            std::lock_guard lk(m_eventMutex);
            m_runStartScheduledCompIds.clear();
            m_deferredRunStartCompIds.clear();
            m_events.clear();
        }

        SimDriver::clearComponents();
        m_runIterCv.notify_all();
        notifyScheduler();
    }

    std::vector<UUID> EvtBasedSimDriver::getDependants(const UUID &id) {
        return {};
    }

    std::vector<PortState> EvtBasedSimDriver::collapseInputs(const UUID &id) {
        return {};
    }

    void EvtBasedSimDriver::propagateFromComponent(const UUID &sourceId) {
        scheduleDependantsOf(sourceId);
    }

    void EvtBasedSimDriver::onBeforeRun() {
    }

    void EvtBasedSimDriver::onPause() {
        m_runIterCv.notify_all();
        notifyScheduler();
    }

    void EvtBasedSimDriver::onResume() {
        m_runIterCv.notify_all();
        notifyScheduler();
    }

    void EvtBasedSimDriver::onStop() {
        SimDriver::onStop();
        m_runIterCv.notify_all();
        notifyScheduler();
    }

    void EvtBasedSimDriver::onStep() {
        const auto nextTime = getNextEventTime();
        if (!nextTime) {
            return;
        }

        (void)getSimulationClock()->advanceTo(*nextTime);
        processEventsAt(getCurrentSimTime());
    }

    void EvtBasedSimDriver::scheduleEvt(const UUID &compId,
                                        TimeNs simTime,
                                        const UUID &schedulerId,
                                        bool notify) {
        const auto count = simTime.count();
        if (!std::isfinite(count)) {
            BESS_ERROR("Ignoring event with a non-finite simulation time");
            return;
        }

        if (simTime < getCurrentSimTime()) {
            simTime = getCurrentSimTime();
        }

        {
            std::lock_guard lk(m_eventMutex);
            scheduleEvtLocked(compId, simTime, schedulerId);
        }

        if (notify) {
            m_runIterCv.notify_all();
            notifyScheduler();
        }
    }

    void EvtBasedSimDriver::scheduleEvtLocked(const UUID &compId,
                                              TimeNs simTime,
                                              const UUID &schedulerId) {
        BESS_LOG_EVENT("(EvtBasedSimDriver.scheduleEvtLocked) Scheduling event "
                       "{} for component {} at time {}ns (scheduled by {})",
                       m_nextEventId,
                       (uint64_t)compId,
                       simTime.count(),
                       (uint64_t)schedulerId);
        SimEvt ev{UUID(m_nextEventId++), compId, schedulerId, simTime};
        m_events.insert(ev);
    }

    void EvtBasedSimDriver::clearPendingEvents() {
        {
            std::lock_guard lk(m_eventMutex);
            m_deferredRunStartCompIds.clear();
            m_events.clear();
        }
        m_runIterCv.notify_all();
        notifyScheduler();
    }

    void EvtBasedSimDriver::prepareRunStartLocked(TimeNs startTime) {
        std::vector<UUID> selfScheduledCompIds;
        std::vector<UUID> initiallyScheduledCompIds;
        m_deferredRunStartCompIds.clear();

        {
            std::lock_guard compLock(m_compMapMutex);
            for (const auto &[compId, compBase] : m_components) {
                const auto comp =
                    std::dynamic_pointer_cast<EvtBasedSimComp>(compBase);
                if (!comp) {
                    continue;
                }

                if (comp->getSimSelf()) {
                    selfScheduledCompIds.emplace_back(compId);
                } else if (m_runStartScheduledCompIds.contains(compId)) {
                    initiallyScheduledCompIds.emplace_back(compId);
                }
            }
        }

        const auto sortByUuid = [](std::vector<UUID> &ids) {
            std::ranges::sort(ids, [](const UUID &a, const UUID &b) {
                return static_cast<uint64_t>(a) < static_cast<uint64_t>(b);
            });
        };
        sortByUuid(selfScheduledCompIds);
        sortByUuid(initiallyScheduledCompIds);

        m_events.clear();
        m_nextEventId = 1;

        for (const auto &compId : selfScheduledCompIds) {
            scheduleEvtLocked(compId, startTime, UUID::null);
        }

        if (!selfScheduledCompIds.empty()) {
            m_deferredRunStartCompIds = std::move(initiallyScheduledCompIds);
            return;
        }

        for (const auto &compId : initiallyScheduledCompIds) {
            scheduleEvtLocked(compId, startTime, UUID::null);
        }
    }

    void EvtBasedSimDriver::scheduleDeferredRunStartEvents() {
        std::vector<UUID> deferredCompIds;

        {
            std::lock_guard lk(m_eventMutex);
            if (m_deferredRunStartCompIds.empty()) {
                return;
            }

            deferredCompIds = std::move(m_deferredRunStartCompIds);
            m_deferredRunStartCompIds.clear();

            {
                std::lock_guard compLock(m_compMapMutex);
                std::erase_if(deferredCompIds, [&](const UUID &compId) {
                    return !m_components.contains(compId);
                });
            }

            if (deferredCompIds.empty()) {
                return;
            }

            HashSet<UUID> pendingCompIds;
            for (const auto &evt : m_events) {
                pendingCompIds.insert(evt.compId);
            }

            for (const auto &compId : deferredCompIds) {
                if (pendingCompIds.contains(compId)) {
                    continue;
                }

                scheduleEvtLocked(compId, getCurrentSimTime(), UUID::null);
            }
        }

        m_runIterCv.notify_all();
        notifyScheduler();
    }

    size_t EvtBasedSimDriver::processEventsAt(TimeNs simTime) {
        std::lock_guard processLock(m_processMutex);

        if (!std::isfinite(simTime.count()) || simTime.count() < 0.0) {
            BESS_ERROR("Ignoring request to process an invalid simulation "
                       "time of {}ns",
                       simTime.count());
            return 0;
        }

        if (simTime < getCurrentSimTime()) {
            simTime = getCurrentSimTime();
        }
        (void)getSimulationClock()->advanceTo(simTime);
        const auto evts = collectEvtsAt(simTime);
        if (evts.empty()) {
            return 0;
        }

        const auto finishBatch = [this, count = evts.size()]() {
            std::lock_guard lk(m_eventMutex);
            m_eventsInFlight -= std::min(m_eventsInFlight, count);
        };

        try {
            simulateEvts(evts, simTime);
            scheduleDeferredRunStartEvents();
        } catch (...) {
            finishBatch();
            throw;
        }
        finishBatch();
        return evts.size();
    }

    void EvtBasedSimDriver::simulateEvts(const std::vector<SimEvt> &evts,
                                         TimeNs simTime) {
        using EvtComp = EvtBasedSimComp;

        std::unordered_map<UUID, std::vector<PortState>> inputsMap = {};
        bool scheduledSelf = false;

        for (auto &ev : evts) {
            inputsMap[ev.compId] = collapseInputs(ev.compId);
        }

        BESS_LOG_EVENT("(EvtBasedSimDriver.simulateEvts) Simulating {} events "
                       "at time {}ns",
                       evts.size(),
                       simTime.count());

        for (const auto &evt : evts) {
            const auto comp = getComponentSP<EvtComp>(evt.compId);

            if (!comp) {
                BESS_WARN(
                    "(EvtBasedSimDriver.run) Component with UUID {} not found",
                    (uint64_t)evt.compId);
                continue;
            }

#ifdef BESS_ENABLE_LOG_EVENTS
            BESS_LOG_EVENT("(EvtBasedSimDriver.simulateEvts) Simulating event "
                           "{} for component {} ({}) with inputs: ",
                           (uint64_t)evt.evtId,
                           comp->getName(),
                           (uint64_t)evt.compId);

            for (const auto &input : inputsMap[evt.compId]) {
                BESS_LOG_EVENT("    state: {}, lastChangeTime: {}ns",
                               static_cast<int>(input.state),
                               input.lastChangeTime.count());
            }

#endif // BESS_ENABLE_LOG_EVENTS

            const bool simDependants = simulate(evt, inputsMap[evt.compId]);

            if (simDependants) {
                scheduleDependantsOfAt(evt.compId, simTime);
            }

            if (comp->getSimSelf()) {
                const auto delay = comp->getSelfSimDelay();
                if (!std::isfinite(delay.count()) || delay.count() <= 0.0) {
                    BESS_ERROR("Component {} requested auto-rescheduling with "
                               "an invalid delay of {}ns; self scheduling was "
                               "disabled",
                               (uint64_t)evt.compId,
                               delay.count());
                } else {
                    scheduleEvt(evt.compId, simTime + delay, UUID::null, false);
                    scheduledSelf = true;
                }
            }
        }

        if (scheduledSelf) {
            m_runIterCv.notify_all();
            notifyScheduler();
        }
    }

    void EvtBasedSimDriver::scheduleDependantsOf(const UUID &compId) {
        scheduleDependantsOfAt(compId, getCurrentSimTime());
    }

    void EvtBasedSimDriver::scheduleDependantsOfAt(const UUID &compId,
                                                   TimeNs simTime) {
        using EvtComp = EvtBasedSimComp;

        const auto comp = getComponentSP<EvtComp>(compId);

        if (!comp) {
            BESS_WARN("(EvtBasedSimDriver.scheduleDependantsOf) Component with "
                      "UUID {} not found",
                      (uint64_t)compId);
            return;
        }

        const auto dependants = getDependants(compId);
        auto delay = comp->getPropDelay();
        if (!std::isfinite(delay.count())) {
            BESS_ERROR("Component {} has a non-finite propagation delay; "
                       "dependants were not scheduled",
                       (uint64_t)compId);
            return;
        }
        if (delay.count() < 0.0) {
            BESS_WARN("Component {} has a negative propagation delay of {}ns; "
                      "it was clamped to zero",
                      (uint64_t)compId,
                      delay.count());
            delay = TimeNs(0);
        }

        for (const auto &id : dependants) {
            scheduleEvt(id, simTime + delay, compId, false);
        }
        if (!dependants.empty()) {
            m_runIterCv.notify_all();
            notifyScheduler();
        }
    }

    std::optional<TimeNs> EvtBasedSimDriver::getNextEventTime() const {
        std::lock_guard lk(m_eventMutex);
        if (m_events.empty()) {
            return std::nullopt;
        }
        return m_events.begin()->simTime;
    }

    bool EvtBasedSimDriver::isSimStable() const {
        std::lock_guard lk(m_eventMutex);
        if (m_eventsInFlight != 0) {
            return false;
        }
        for (const auto &evt : m_events) {
            const auto comp = getComponentSP<EvtBasedSimComp>(evt.compId);
            if (!comp || !comp->getSimSelf()) {
                return false;
            }
        }
        return true;
    }

    std::vector<SimEvt> EvtBasedSimDriver::collectEvtsAt(TimeNs simTime) {
        HashSet<UUID> collectedCompIds;
        std::vector<SimEvt> evtsToSim;

        std::lock_guard lk(m_eventMutex);
        auto it = m_events.begin();
        while (it != m_events.end() && it->simTime <= simTime) {
            if (collectedCompIds.insert(it->compId).second) {
                evtsToSim.push_back(*it);
            }
            it = m_events.erase(it);
        }
        m_eventsInFlight += evtsToSim.size();

        return evtsToSim;
    }

    void EvtBasedSimComp::addOnStateChangeCB(const UUID &id,
                                             const TOnStateChangeFn &cb) {
        m_onStateChangeCbs[id] = cb;
    }

    void EvtBasedSimComp::removeOnStateChangeCB(const UUID &id) {
        m_onStateChangeCbs.erase(id);
    }

    void EvtBasedCompDef::loadJson(const Json::Value &json) {
        CompDef::loadJson(json);
        if (json.isMember("shouldAutoReschedule")) {
            m_autoReschedule = json["shouldAutoReschedule"].asBool();
        }

        if (json.isMember("autoRescheduleDelay")) {
            JsonConvert::fromJsonValue(json["autoRescheduleDelay"],
                                       m_autoRescheduleDelay);
        }

        if (json.isMember("propDelay")) {
            JsonConvert::fromJsonValue(json["propDelay"], m_propDelay);
        }
    }
} // namespace Bess::SimEngine::Drivers
