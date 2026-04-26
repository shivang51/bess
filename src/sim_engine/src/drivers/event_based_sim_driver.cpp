#include "drivers/event_based_sim_driver.h"

namespace Bess::SimEngine::Drivers {

    Json::Value EvtBasedCompDef::toJson() const {
        Json::Value json = CompDef::toJson();
        json["shouldAutoReschedule"] = m_autoReschedule;
        json["propDelay"] = m_propDelay.count();
        return json;
    }

    TimeNs EvtBasedCompDef::getSelfSimDelay() {
        return TimeNs(0);
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

    void EvtBasedSimComp::fromJson(const std::shared_ptr<EvtBasedSimComp> &comp,
                                   const Json::Value &json) {
        SimComponent::fromJson(comp, json);
    }

    void EvtBasedSimDriver::run() {
        onBeforeRun();

        BESS_ASSERT(isInitialized(),
                    "SimDriver must be initialized before running");
        BESS_ASSERT(!isDestroyed(),
                    "SimDriver was destroyed, cannot run");

        setState(SimDriverState::running);

        while (!isStopped()) {
            std::unique_lock lk(m_runIterMutex);

            BESS_DEBUG("Sim waiting for events");

            m_runIterCv.wait(lk, [&] {
                return isStopped() || (!isPaused() && !m_events.empty());
            });

            if (isStopped()) {
                break;
            }

            if (isPaused()) {
                continue;
            }

            const auto evtsToSim = collectEvts();
            BESS_DEBUG("Simulating {}", evtsToSim.size());

            if (evtsToSim.empty()) {
                const auto &nextEvt = getNextEvt();
                std::this_thread::sleep_for(nextEvt.simTime - m_currentSimTime);
                m_currentSimTime = nextEvt.simTime;
                continue;
            }

            simulateEvts(evtsToSim);
        }
    }

    void EvtBasedSimDriver::addComponent(
        const std::shared_ptr<EvtBasedSimComp> &comp,
        bool scheduleSim) {
        {
            std::lock_guard lk(m_compMapMutex);
            m_components[comp->getUuid()] = comp;
        }

        if (scheduleSim) {
            BESS_DEBUG("Scheduling initial event for component {} at time {}ns",
                       (uint64_t)comp->getUuid(),
                       (m_currentSimTime + comp->getSelfSimDelay()).count());
            scheduleEvt(comp->getUuid(),
                        m_currentSimTime + comp->getSelfSimDelay(),
                        UUID::null,
                        true);
        }
    }

    std::vector<UUID> EvtBasedSimDriver::getDependants(const UUID &id) {
        return {};
    }

    std::vector<SlotState> EvtBasedSimDriver::collapseInputs(const UUID &id) {
        return {};
    }

    void EvtBasedSimDriver::onBeforeRun() {}

    void EvtBasedSimDriver::onPause() {
        m_runIterCv.notify_all();
    }

    void EvtBasedSimDriver::onResume() {
        m_runIterCv.notify_all();
    }

    void EvtBasedSimDriver::onStop() {
        SimDriver::onStop();
        m_runIterCv.notify_all();
    }

    void EvtBasedSimDriver::scheduleEvt(const UUID &compId,
                                        TimeNs simTime,
                                        const UUID &schedulerId,
                                        bool notify) {
        static uint64_t evtId = 0;
        SimEvt ev{UUID(evtId++),
                  compId,
                  schedulerId,
                  simTime};
        m_events.insert(ev);
        if (notify) {
            m_runIterCv.notify_all();
        }
    }

    void EvtBasedSimDriver::clearPendingEvents() {
        std::lock_guard lk(m_runIterMutex);
        m_events.clear();
        m_runIterCv.notify_all();
    }

    void EvtBasedSimDriver::simulateEvts(const std::vector<SimEvt> &evts) {
        using EvtComp = EvtBasedSimComp;

        for (const auto &evt : evts) {
            const bool simDependants = simulate(evt);

            const auto &comp = getComponent<EvtComp>(evt.compId);

            if (!comp) {
                BESS_WARN("(EvtBasedSimDriver.run) Component with UUID {} not found",
                          (uint64_t)evt.compId);
                continue;
            }

            if (simDependants) {
                scheduleDependantsOf(evt.compId);
            }

            if (comp->getSimSelf()) {
                scheduleEvt(evt.compId,
                            m_currentSimTime + comp->getSelfSimDelay(),
                            UUID::null,
                            true);
            }
        }

        for (const auto &evt : evts) {
            m_events.erase(evt);
        }
    }

    void EvtBasedSimDriver::scheduleDependantsOf(const UUID &compId) {
        using EvtComp = EvtBasedSimComp;

        const auto &comp = getComponent<EvtComp>(compId);

        if (!comp) {
            BESS_WARN("(EvtBasedSimDriver.scheduleDependantsOf) Component with UUID {} not found",
                      (uint64_t)compId);
            return;
        }

        const auto dependants = getDependants(compId);

        for (const auto &id : dependants) {
            scheduleEvt(id,
                        m_currentSimTime + comp->getPropDelay(),
                        compId,
                        false);
        }

        m_runIterCv.notify_all();
    }

    SimEvt EvtBasedSimDriver::getNextEvt() const {
        SimEvt ev;
        if (m_events.empty()) {
            ev.simTime = m_currentSimTime;
            return ev;
        }

        ev.simTime = TimeNs::max();

        for (const auto &evt : m_events) {
            if (evt.simTime < ev.simTime) {
                ev = evt;
            }
        }

        return ev;
    }

    std::vector<SimEvt> EvtBasedSimDriver::collectEvts() {
        std::set<UUID> collectedCompIds = {};
        std::vector<SimEvt> evtsToSim = {};

        for (const auto &evt : m_events) {
            if (evt.simTime > m_currentSimTime) {
                continue;
            }

            if (collectedCompIds.contains(evt.compId)) {
                continue;
            }

            collectedCompIds.insert(evt.compId);
            evtsToSim.push_back(evt);
        }

        std::ranges::sort(evtsToSim,
                          [](const SimEvt &a, const SimEvt &b) {
                              if (a.simTime != b.simTime) {
                                  return a.simTime < b.simTime;
                              }
                              return a.evtId < b.evtId;
                          });

        return evtsToSim;
    }

} // namespace Bess::SimEngine::Drivers
